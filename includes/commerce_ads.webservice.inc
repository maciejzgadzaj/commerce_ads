<?php

/**
 * This function send a request to ADS Webservice.
 *
 * @param string $service
 *   The service we ask to ads
 * @param array $options
 *   The options for drupal_http_request second arguments.
 *
 * @return array
 */
function commerce_ads_request_ads($service, $options = array(), $url = '') {
  if (empty($url)) {
    $url = commerce_ads_get_ads_base_url() . $service;
  }
  else {
    $url .= $service;
  }
  $response = drupal_http_request($url, $options);
  if (!empty($response->data)) {
    $response = json_decode($response->data, TRUE);
    commerce_ads_request_ads_log($response);
  }
  elseif(!empty($response->error)) {
    watchdog('commerce_ads', $response->error . ' => ' . $service);
  }
  return $response;
}

/**
 * Log the ads request response
 *
 * @param array $response
 *  A array containing the response of the webservice.
 */
function commerce_ads_request_ads_log($response) {
  $log = '';
  if (!empty($response['Comment'])) {
    $log .= $response['Comment'] . "\n";
  }
  if (!empty($response['Data']['Number']) && !empty($response['Data']['Shippings'])) {
    $log .= 'Order n°' . $response['Data']['Number'] . ' updated.';
  }
  if (!empty($response['Data']) && is_array($response['Data'])) {
    foreach ($response['Data'] as $value) {
      if (is_array($value) && (isset($value['Data']) || isset($value['Comment']))) {
        $log .= !empty($value['Data']) ? $value['Data'] . ': ' . $value['Comment'] . '. ' : $value['Comment'] . '. ';
      }
    }
  }
  elseif (!empty($response['Message'])) {
    $log .= $response['Message'];
  }
  else {
    $log .= 'Response empty';
  }
  foreach (str_split($log, 5000) as $message) {
    watchdog('commerce_ads', $message);
  }
}

/**
 * Get base url for making calls with ADS WS.
 *
 * @return string
 *  Return the base url.
 */
function commerce_ads_get_ads_base_url() {
  $commerce_ads_ws = variable_get('commerce_ads_ws');
  return !empty($commerce_ads_ws['ads_base_url']) ? $commerce_ads_ws['ads_base_url'] : '';
}


/**
 * Send product(s) to ads.
 *
 * @param array $products
 *  Array of product_id to be sent to ADS.
 */
function commerce_ads_send_products($products = array()) {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($products) && !empty($commerce_ads_ws['ads_product_mapping']) && commerce_ads_check_login_ws()) {
    $product_update_json = array();
    $product_create_json = array();
    foreach ($products as $product) {
      $product = entity_metadata_wrapper('commerce_product', $product);
      // Size.
      $dimension = commerce_ads_get_field_value('Size (Physical)', $product, $commerce_ads_ws['ads_product_mapping'], 'commerce_product');
      $weight = commerce_ads_get_field_value('Size (Weight)', $product, $commerce_ads_ws['ads_product_mapping'], 'commerce_product');
      $size_array = array();
      if (!empty($dimension)) {
        $size_array['Height'] = (int) $dimension['height'];
        $size_array['Length'] = (int) $dimension['length'];
        $size_array['Width'] = (int) $dimension['width'];
        $size_array['Volume'] = 0;
      }
      if (!empty($weight)) {
        $size_array['Weight'] = (int) $weight['weight'];
      }

      // Translations.
      $translation_array = array();
      foreach (commerce_ads_get_product_translation($product) as $langcode => $description) {
        $translation_array[] = array(
          'Label' => substr($description, 0, 100),
          'Language' => $langcode,
        );
      }

      $product_to_send = array(
        'ProductId' => substr($product->product_id->value(), 0, 18),
        'Type' => 'PRODUCT',
        'Picture' => substr(commerce_ads_get_field_value('Picture', $product, $commerce_ads_ws['ads_product_mapping'], 'commerce_product'), 0, 250),
        'fileUrl' => commerce_ads_get_field_value('fileUrl', $product, $commerce_ads_ws['ads_product_mapping'], 'commerce_product'),
        'Size' => $size_array,
        'Translations' => $translation_array,
        'Eans' => array(
          array(
            'Ean13' => substr($product->sku->value(), 0, 30),
          )
        ),
      );
      drupal_alter('commerce_ads_product', $product_to_send);
      if ($product->commerce_ads_product_needs_update->value() == PRODUCT_TO_BE_UPDATED) {
        $product_update_json[] = $product_to_send;
      }
      elseif ($product->commerce_ads_product_needs_update->value() == PRODUCT_TO_BE_CREATED) {
        $product_create_json[] = $product_to_send;
      }
      // Bypass infinite loop.
      db_update('commerce_product')
        ->fields(array(
        'commerce_ads_product_needs_update' => PRODUCT_SENT,
      ))
        ->condition('product_id', $product->product_id->value())
        ->execute();
    }

    if (!empty($product_update_json)) {
      $options = array(
        'method' => 'POST',
        'timeout' => 6000,
        'data' => json_encode($product_update_json),
        'headers' => array(
          'Content-Type' => 'application/json',
          'Accept' => 'application/json',
          'Authorization' => !empty($commerce_ads_ws['ads_authorization_key']) ? $commerce_ads_ws['ads_authorization_key'] : '',
        ),
      );
      commerce_ads_request_ads('api/items/updateMultiple', $options);
    }
    if (!empty($product_create_json)) {
      $options = array(
        'method' => 'POST',
        'timeout' => 6000,
        'data' => json_encode($product_create_json),
        'headers' => array(
          'Content-Type' => 'application/json',
          'Accept' => 'application/json',
          'Authorization' => !empty($commerce_ads_ws['ads_authorization_key']) ? $commerce_ads_ws['ads_authorization_key'] : '',
        ),
      );
      commerce_ads_request_ads('api/items/createMultiple', $options);
    }
  }
}

/**
 * Send order(s) to ads.
 *
 * @param array $orders
 *  Array of order_id to be sent to ADS.
 * @param string $order_type
 *  The type of order on ADS side (SHOP, PROD, ABO).
 */
function commerce_ads_send_order($orders = array(), $order_type = NULL) {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($orders) && !empty($commerce_ads_ws['ads_order_mapping']) && commerce_ads_check_login_ws()) {
    global $language;
    $order_create_json = array();
    $order_update_json = array();
    foreach ($orders as $order) {
      $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
      $result = db_select('commerce_order', 'c')
        ->fields('c', array('commerce_ads_order_needs_update'))
        ->condition('order_id', $order_wrapper->order_id->value())
        ->execute()
        ->fetchAssoc();

      if (commerce_ads_order_needs_to_be_sent($order_wrapper, $commerce_ads_ws['ads_product_types'])) {
        $order_info = commerce_ads_get_order_info($order_wrapper);
        // Shipping.
        $shipping_fields = array('FirstName','LastName','CompanyName','AddressLine1','AddressLine2','AddressLine3','PostalCode','City','State','Country','MobilePhone','Phone','Comment','Floor','Apartment','Building','Digicode','PickupCode','PickupCountry');
        // Address.
        $customer_profile_type = 'commerce_customer_' . $commerce_ads_ws['ads_customer_profile_type'];
        $address_array = array();
        if (!empty($order_wrapper->$customer_profile_type)) {
          foreach ($shipping_fields as $shipping_field) {
            $address_array[$shipping_field] = commerce_ads_get_field_value($shipping_field, entity_metadata_wrapper('commerce_customer_profile', $order_wrapper->$customer_profile_type->value()), $commerce_ads_ws['ads_order_mapping'], 'commerce_customer_profile');
          }
        }
        $address_array['Email'] = $order_wrapper->mail->value();

        // Product lines.
        $product_array = array();
        foreach ($order_info['products'] as $product) {
          $product_array[] = array(
            'Label' => $product['Label'],
            'ProductId' => $product['ProductId'],
            'Quantity' => (int) $product['Quantity'],
          );
        }
        $message = str_split(commerce_ads_get_field_value('GiftMessage', $order_wrapper, $commerce_ads_ws['ads_order_mapping'], 'commerce_order'), 50);
        $order_to_send = array(
          'ShopId' => !empty($commerce_ads_ws['ads_shopid']) ? $commerce_ads_ws['ads_shopid'] : '',
          'Type' => !empty($order_type) ? $order_type : 'SHOP',
          'Number' => $order_wrapper->order_id->value(),
          'Date' => date('d/m/Y', $order_wrapper->created->value()),
          'Language' => $language->language,
          'InvoiceUrl' => commerce_ads_get_field_value('InvoiceUrl', $order_wrapper, $commerce_ads_ws['ads_order_mapping'], 'commerce_order'),
          'Blocked' => commerce_ads_get_field_value('Blocked', $order_wrapper, $commerce_ads_ws['ads_order_mapping'], 'commerce_order'),
          'Gift' => commerce_ads_get_field_value('Gift', $order_wrapper, $commerce_ads_ws['ads_order_mapping'], 'commerce_order'),
          'GiftMessage' => array(
            'Line1' => isset($message[0]) ? $message[0] : '',
            'Line2' => isset($message[1]) ? $message[1] : '',
            'Line3' => isset($message[2]) ? $message[2] : '',
          ),
          'Shippings' => array(
            array(
              'Address' => $address_array,
              'DeliverymanId' => !empty($order_info['DeliverymanId']) ? $order_info['DeliverymanId'] : 'EXAPAQ',
              'ProductLines' => $product_array,
            ),
          ),
        );
        drupal_alter('commerce_ads_order', $order_to_send);
        if ($result['commerce_ads_order_needs_update']== ORDER_TO_BE_UPDATED) {
          $order_update_json[] = $order_to_send;
        }
        elseif ($result['commerce_ads_order_needs_update'] == ORDER_TO_BE_CREATED) {
          $order_create_json[] = $order_to_send;
        }
        // Bypass infinite loop in hook_commerce_order_update().
        db_update('commerce_order')
          ->fields(array(
          'commerce_ads_order_needs_update' => ORDER_SENT,
        ))
          ->condition('order_id', $order_wrapper->order_id->value())
          ->execute();
      }
    }

    if (!empty($order_update_json)) {
      $options = array(
        'method' => 'POST',
        'timeout' => 6000,
        'data' => json_encode($order_update_json),
        'headers' => array(
          'Content-Type' => 'application/json',
          'Accept' => 'application/json',
          'Authorization' => !empty($commerce_ads_ws['ads_authorization_key']) ? $commerce_ads_ws['ads_authorization_key'] : '',
        ),
      );
      commerce_ads_request_ads('api/customerOrders/updateMultiple', $options);
    }
    if (!empty($order_create_json)) {
      $options = array(
        'method' => 'POST',
        'timeout' => 6000,
        'data' => json_encode($order_create_json),
        'headers' => array(
          'Content-Type' => 'application/json',
          'Accept' => 'application/json',
          'Authorization' => !empty($commerce_ads_ws['ads_authorization_key']) ? $commerce_ads_ws['ads_authorization_key'] : '',
        ),
      );
      commerce_ads_request_ads('api/customerOrders/createMultiple', $options);
    }
  }
}

/**
 * Update Drupal commerce_order with ADS info (tracking url, shipping status)
 *
 * @param $orders
 *  Array of order_id to be updated.
 */
function commerce_ads_update_order($orders = array()) {
  if (!empty($orders)) {
    foreach ($orders as $order_id) {
      $commerce_ads_ws = variable_get('commerce_ads_ws', array());
      $response = commerce_ads_request_ads('api/CustomerOrders/get?Number=' . $order_id, array(
        'method' => 'GET',
        'timeout' => 6000,
        'headers' => array(
          'Authorization' => !empty($commerce_ads_ws['ads_authorization_key']) ? $commerce_ads_ws['ads_authorization_key'] : '',
        ),
      ));
      if ($response['Code'] === 0 && isset($commerce_ads_ws['ads_shipping_statuses'][$response['Data']['CustomerOrderState']['StateId']])) {
        $save = FALSE;
        $order_wrapper = entity_metadata_wrapper('commerce_order', $order_id);
        if ($commerce_ads_ws['ads_shipping_statuses'][$response['Data']['CustomerOrderState']['StateId']] != $order_wrapper->status->value()) {
          $order_wrapper->status->set($commerce_ads_ws['ads_shipping_statuses'][$response['Data']['CustomerOrderState']['StateId']]);
          $save = TRUE;
        }
        if (!empty($response['Data']['Shippings'][0]['TrackingURL'])) {
          $order_wrapper->field_ads_tracking_url->set($response['Data']['Shippings'][0]['TrackingURL']);
          $save = TRUE;
        }
        if ($save) {
          commerce_order_save($order_wrapper->value());
        }
      }
    }
  }
}

/**
 * Update all ads orders that needs to be updated.
 */
function commerce_ads_update_all_orders() {
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'commerce_order')
    ->propertyCondition('commerce_ads_order_needs_update', array(ORDER_TO_BE_UPDATED, ORDER_SENT), 'IN');
  $result = $query->execute();
  if (isset($result['commerce_order'])) {
    commerce_ads_update_order(array_keys($result['commerce_order']));
  }
}

/**
 * Send all orders to ADS.
 */
function commerce_ads_send_all_orders() {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($commerce_ads_ws['ads_order_mapping'])) {
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'commerce_order')
      ->propertyCondition('commerce_ads_order_needs_update', array(ORDER_TO_BE_CREATED, ORDER_TO_BE_UPDATED), 'IN');
    $result = $query->execute();
    if (isset($result['commerce_order'])) {
      commerce_ads_send_order(array_keys($result['commerce_order']));
    }
  }
  else {
    form_set_error('commerce_ads', t('You have to map order fields so it can be synchronize with ADS first.'));
  }
}

/**
 * Send all products to ADS.
 */
function commerce_ads_send_all_products() {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($commerce_ads_ws['ads_product_types'])) {
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'commerce_product')
      ->propertyCondition('commerce_ads_product_needs_update', array(PRODUCT_TO_BE_UPDATED, PRODUCT_TO_BE_CREATED), 'IN')
      ->entityCondition('type', array_keys($commerce_ads_ws['ads_product_types']));
    $result = $query->execute();
    if (isset($result['commerce_product'])) {
      commerce_ads_send_products(array_keys($result['commerce_product']));
    }
  }
  else {
    form_set_error('commerce_ads', t('You have to select the product types which will be synchronize with ADS first.'));
  }
}

/**
 * Test that the ADS Webservice is reachable with the current configuration.
 *
 * @param string $url
 *  The url to request (with trailing slash).
 * @param string $authorization_key
 *  The authorization key to send in request header to test webservice login.
 *
 * @return bool
 *  Whether or not the webservice is reachable with those info.
 */
function commerce_ads_check_login_ws($url = '', $authorization_key = '') {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (empty($authorization_key)) {
    if (!empty($commerce_ads_ws['ads_authorization_key'])) {
      $authorization_key = $commerce_ads_ws['ads_authorization_key'];
    }
  }
  $response = commerce_ads_request_ads(
    'api/Check/Login',
    array(
      'method' => 'POST',
      'timeout' => 6000,
      'headers' => array(
        'Authorization' => $authorization_key,
      ),
    ),
    $url
  );
  if (!is_object($response) && isset($response['Code']) && $response['Code'] === 0) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

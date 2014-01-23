<?php

/**
 * Configuration form to store ads settings.
 */
function commerce_ads_configure_form($form, &$form_state) {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  $form['commerce_ads_ws'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('My ADS account'),
  );
  // Login.
  $form['commerce_ads_ws']['shopid'] = array(
    '#type' => 'textfield',
    '#title' => t('Shop id'),
    '#description' => t('The unique identifier of the shop'),
    '#default_value' => isset($commerce_ads_ws['shopid']) ? $commerce_ads_ws['shopid'] : '',
    '#required' => TRUE,
  );
  // Mode
  $form['commerce_ads_ws']['commerce_ads_ws_mode'] = array(
    '#type' => 'radios',
    '#title' => t('Mode'),
    '#options' => array(
      'test' => ('Test - allow you to test in real conditions on Priceminister sandboxes'),
      'live' => ('Live - process real export import to a live account'),
    ),
    '#default_value' => variable_get('commerce_ads_ws_mode', 'test'),
  );
  $form['help'] = array(
    '#type' => 'markup',
    '#value' => t("If you don't have an ADS account, then please refer to ..."),
  );
  $form['#validate'] = array('commerce_ads_configure_form_validate');

  return system_settings_form($form);
}

function commerce_ads_configure_form_validate(&$form, &$form_state) {
  // Todo check that the ads webservice is reachable, otherwise display an error message.
  if (FALSE) {
    form_set_error('commerce_ads', t('The ADS webservice is not reachable with these credential. Please retry or call your ADS contact.'));
  }
}

function commerce_ads_product_configure_form($form, &$form_state) {
  $form = array();
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());

  // Product types.
  $form['product_types'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Product types'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $types = array_keys(commerce_product_types());
  $form['product_types']['types'] = array(
    '#type' => 'select',
    '#multiple' => TRUE,
    '#title' => t('Product types'),
    '#options' => array_combine($types, $types),
    '#description' => t('Select the product types which will be synchronize with ADS.'),
  );

  // Product mapping.
  $form['product_fields_mapping'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Product mapping'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  if (!empty($commerce_ads_ws['product_types'])) {
    $fields = commerce_ads_get_all_product_fields($commerce_ads_ws['product_types']);
    foreach ($commerce_ads_ws['ads_product_fields'] as $ads_field_name => $info) {
      $form['product_fields_mapping'][$ads_field_name] = array(
        '#title' => t($ads_field_name),
        '#type' => 'select',
        '#multiple' => TRUE,
        '#options' => $fields,
        '#description' => $info['description'],
        '#required' => $info['required'],
        '#default_value' => $commerce_ads_ws['product_fields_mapping'][$ads_field_name],
      );
    }
  }
  else {
    $form['product_fields_mapping'] = array(
      '#type' => 'markup',
      '#value' => t("You first have to choose which product types will be synchronize with ADS"),
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );

  return $form;
}

function commerce_ads_product_configure_form_submit($form, &$form_state) {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($form_state['values']['product_types']['types'])) {
    $commerce_ads_ws['product_types'] = $form_state['values']['product_types']['types'];
  }
  if (!empty($form_state['values']['product_fields_mapping'])) {
    $commerce_ads_ws['product_fields_mapping'] = $form_state['values']['product_fields_mapping'];
  }
  variable_set('commerce_ads_ws', $commerce_ads_ws);
}

function commerce_ads_get_all_product_fields($bundles) {
  $fields = array();
  foreach (field_read_instances(array('entity_type' => 'commerce_product')) as $field) {
    if (in_array($field['bundle'], $bundles)) {
      $fields[$field['field_name']] = $field['label'] . ' - (' . $field['field_name'] . ')';
    }
  }
  return $fields;
}

function commerce_ads_order_configure_form($form, &$form_state) {
  $form = array();
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  // Order status.
  $form['ads_order_status'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Order status for validation'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $statuses = array();
  foreach (commerce_order_statuses() as $status) {
    $statuses[$status['name']] = $status['title'];
  }
  $form['ads_order_status']['status'] = array(
    '#type' => 'select',
    '#title' => t('Order validation status'),
    '#options' => $statuses,
    '#description' => t('Select the order status for which one the order will be sent to ADS.'),
  );

  // Order mapping.
  $form['ads_order_mapping'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Order mapping'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#prefix' => '<div id="ads-order-mapping">',
    '#suffix' => '</div>',
  );
  $form['ads_order_mapping']['customer_profile_type'] = array(
    '#type' => 'select',
    '#title' => t('Customer profile to use'),
    '#options' => commerce_customer_profile_type_options_list(),
    '#description' => t('Which customer profile store the shipping address of your customer?'),
    '#ajax' => array(
      'callback' => 'commerce_ads_ajax_get_profile_fields',
      'wrapper' => 'ads-order-mapping',
    ),
    '#default_value' => $commerce_ads_ws['customer_profile_type'],
  );

  if (!empty($form_state['values']['ads_order_mapping']['customer_profile_type'])) {
    $customer_profile_type = $form_state['values']['ads_order_mapping']['customer_profile_type'];
  }
  elseif (!empty($commerce_ads_ws['customer_profile_type'])) {
    $customer_profile_type = $commerce_ads_ws['customer_profile_type'];
  }
  if (!empty($customer_profile_type)) {
    $customer_profile_fields = commerce_ads_get_all_customer_profile_fields($customer_profile_type);
    foreach($commerce_ads_ws['ads_order_fields'] as $ads_field_name => $info) {
      $form['ads_order_mapping']['fields'][$ads_field_name] = array(
        '#title' => t($ads_field_name),
        '#type' => 'select',
        '#multiple' => TRUE,
        '#options' => $customer_profile_fields,
        '#description' => $info['description'],
        '#required' => $info['required'],
        '#default_value' => !empty($commerce_ads_ws['ads_order_mapping'][$ads_field_name]) ? $commerce_ads_ws['ads_order_mapping'][$ads_field_name] : '',
      );
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  return $form;
}

function commerce_ads_order_configure_form_submit($form, &$form_state) {
  $commerce_ads_ws = variable_get('commerce_ads_ws', array());
  if (!empty($form_state['values']['ads_order_status']['status'])) {
    $commerce_ads_ws['ads_order_status'] = $form_state['values']['ads_order_status']['status'];
  }
  if (!empty($form_state['values']['ads_order_mapping']['customer_profile_type'])) {
    $commerce_ads_ws['customer_profile_type'] = $form_state['values']['ads_order_mapping']['customer_profile_type'];
  }
  if (!empty($form_state['values']['ads_order_mapping']['fields'])) {
    $commerce_ads_ws['ads_order_mapping'] = $form_state['values']['ads_order_mapping']['fields'];
  }
  variable_set('commerce_ads_ws', $commerce_ads_ws);
}

function commerce_ads_ajax_get_profile_fields($form, $form_state) {
  return $form['ads_order_mapping'];
}

function commerce_ads_get_all_customer_profile_fields($customer_profile_type) {
  $fields = array();
  $customer_profile_fields = field_info_instances('commerce_customer_profile', $customer_profile_type);
  foreach($customer_profile_fields as $field) {
    if ($field['widget']['module'] !== "addressfield") {
      $fields[$field['field_name']] = $field['label'] . ' - (' . $field['field_name'] . ')';
    }
  }
  return $fields;
}
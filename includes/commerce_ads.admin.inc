<?php

/**
 * Configuration form to store ads settings.
 */
function commerce_ads_configure_form($form, &$form_state) {
  $commerce_ads_api = variable_get('commerce_ads_ws', array());

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
    '#default_value' => isset($commerce_ads_api['shopid']) ? $commerce_ads_api['shopid'] : '',
    '#required' => TRUE,
  );
  // Mode
  $form['commerce_ads_ws_mode'] = array(
    '#type' => 'radios',
    '#title' => t('Mode'),
    '#options' => array(
      'test' => ('Test - allow you to test in real conditions on Priceminister sandboxes'),
      'live' => ('Live - process real export import to a live account'),
    ),
    '#default_value' => variable_get('commerce_ads_ws_mode', 'test'),
  );
  $form['commerce_ads_ws_mode'] = array(
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
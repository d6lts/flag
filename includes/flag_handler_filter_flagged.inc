<?php
// $Id$

/**
 * Handler to filter for content that has not been flagged.
 *
 * @ingroup views
 */
class flag_handler_filter_flagged extends views_handler_filter_boolean_operator {
  function options(&$options) {
    parent::options($options);
    $options['value'] = 1;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $form['value']['#type'] = 'radios';
    $form['value']['#title'] = t('Status');
    $form['value']['#options'] = array(1 => t('Flagged'), 0 => t('Not flagged'));
    $form['value']['#default_value'] = empty($this->options['value']) ? 0 : $this->options['value'];
    $form['value']['#description'] = t('This filter only works if the relationship used has the "Include only flagged content" option unchecked. Otherwise, this filter is not necessary, because all records are already limited to flagged content.');
  }

  function query() {
    $operator = $this->value ? 'IS NOT' : 'IS';
    $this->query->add_where($this->options['group'], $this->relationship .'.uid '. $operator .' NULL');
  }
}

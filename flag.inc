<?php
// $Id$
/**
 * @file flag.inc
 * Implements various flags. Uses object oriented style inspired by that
 * of Views 2.
 */

/**
 * Implmenetation of hook_flag_definitions()
 *
 * Defines the flags this module implements.
 *
 * @return
 *   An "array of arrays", keyed by content-types. The 'handler' slot
 *   should point to the PHP class implementing this flag.
 */
function flag_flag_definitions() {
  return array(
    'node' => array(
      'title' => t('Nodes'),
      'description' => t("Nodes are a Drupal site's primary content."),
      'handler' => 'flag_node',
    ),
    'comment' => array(
      'title' => t('Comments'),
      'description' => t('Comments are responses to node content.'),
      'handler' => 'flag_comment',
    ),
    // The following is only for demonstration.
    //'user' => array(
    //  'title' => t('Users'),
    //  'description' => t('Users who have created accounts on your site.'),
    //  'handler' => 'flag_user',
    //),
  );
}

/**
 * Returns a flag definition.
 */
function flag_fetch_definition($content_type = NULL) {
  static $cache;
  if (!isset($cache)) {
    $cache = module_invoke_all('flag_definitions');
  }

  if (isset($content_type)) {
    if (isset($cache[$content_type])) {
      return $cache[$content_type];
    }
  }
  else {
    return $cache;
  }
}

/**
 * Instantiates a new flag handler. A flag handler is more commonly know as "a
 * flag". A factory method usually populates this empty flag with settings
 * loaded from the database.
 */
function flag_create_handler($content_type) {
  $definition = flag_fetch_definition($content_type);
  if (isset($definition) && class_exists($definition['handler'])) {
    $handler = new $definition['handler'];
  }
  else  {
    $handler = new flag_broken;
  }
  $handler->content_type = $content_type;
  $handler->construct();
  return $handler;
}

/**
 * This abstract class represents a flag, or, in Views 2 terminology, "a handler".
 *
 * This is the base class for all flag implementations. Notable derived
 * classes are flag_node and flag_comment.
 */
class flag_flag {

  // The database ID. Null for flags that haven't been saved to the database yet.
  var $fid = NULL;

  // The content-type this flag works with.
  var $content_type = NULL;

  // The flag's "machine readable" name.
  var $name = '';

  // Various non-serialized properties of the flag, corresponding directly to
  // database columns.
  //
  // It's a pity they're here. They should be serialized and treated just
  // like 'options'.
  var $title = '';
  var $flag_short = '';
  var $flag_long = '';
  var $flag_message = '';
  var $unflag_short = '';
  var $unflag_long = '';
  var $unflag_message = '';
  var $roles = array(DRUPAL_AUTHENTICATED_RID);
  var $global = FALSE;
  // The sub-types, e.g. node types, this flag applies to.
  var $types = array();

  /**
   * Creates a flag from a database row. Returns it.
   *
   * This is static method.
   *
   * The reason this isn't a non-static instance method --like Views's init()--
   * is because the class to instantiate changes according to the 'content_type'
   * database column. This design pattern is known as the "Single Table
   * Inheritance".
   *
   * @static
   */
  function factory($row) {
    $flag = flag_create_handler($row->content_type);

    // Lump all data unto the object...
    foreach ($row as $field => $value) {
      $flag->$field = $value;
    }
    // ...but skip the following two.
    unset($flag->options, $flag->type);

    $options = (array)unserialize($row->options);
    // Make the unserialized options accessible as normal properties.
    foreach ($options as $option => $value) {
      $flag->$option = $value;
    }

    if (!empty($row->type)) {
      // The loop loading from the database should further populate this property.
      $flag->types[] = $row->type;
    }

    $flag->roles = empty($row->roles) ? array() : explode(',', $row->roles);

    return $flag;
  }
  
  /**
   * Another factory method. Returns a new, "empty" flag; e.g., one suitable for
   * the "Add new flag" page.
   *
   * @static
   */
  function factory_by_content_type($content_type) {
    return flag_create_handler($content_type);
  }

  /**
   * Declares the options this flag supports, and their default values.
   *
   * Derived classes should want to override this.
   */ 
  function default_options() {
    return array(
    );
  }

  /**
   * Provides a form for setting options.
   *
   * Derived classes should want to override this.
   */
  function options_form(&$form) {
  }

  /**
   * Default constructor. Loads the default options.
   */  
  function construct() {
    $options = $this->default_options();
    foreach ($options as $option => $value) {
      $this->$option = $value;
    }
  }

  /**
   * Update the flag with settings entered in a form.
   */
  function form_input($form_values) {
    // Load the form fields indiscriminately unto the flag (we don't care about
    // stray FormAPI fields because we aren't touching unknown properties anyway.
    foreach ($form_values as $field => $value) {
      $this->$field = $value;
    } 
    // But checkboxes need some massaging:
    $this->roles = array_values(array_filter($this->roles));
    $this->types = array_values(array_filter($this->types));
  }

  /**
   * Validates a flag settings
   */
  function validate() {
    $this->validate_name();
  }
  
  function validate_name() {
    // Ensure a safe machine name.
    if (!preg_match('/^[a-z_][a-z0-9_]*$/', $this->name)) {
      form_set_error('name', t('The flag name may only contain lowercase letters, underscores, and numbers.'));
    }
    // Ensure the machine name is unique.
    if (!isset($this->fid)) {
      $flag = flag_get_flag($this->name);
      if (!empty($flag)) {
        form_set_error('name', t('Flag names must be unique. This flag name is already in use.'));
      }
    }
  }

  /**
   * Saves a flag to the database. It is a wrapper around update() and insert().
   */
  function save() {
    if (isset($this->fid)) {
      $this->update();
    }
    else {
      $this->insert();
    }
  }

  /**
   * Saves an existing flag to the database. Better use save().
   */
  function update() {
    db_query("UPDATE {flags} SET name = '%s', title = '%s', flag_short = '%s', flag_long = '%s', flag_message = '%s', unflag_short = '%s', unflag_long = '%s', unflag_message = '%s', roles = '%s', global = %d, options = '%s' WHERE fid = %d", $this->name, $this->title, $this->flag_short, $this->flag_long, $this->flag_message, $this->unflag_short, $this->unflag_long, $this->unflag_message, implode(',', $this->roles), $this->global, $this->get_serialized_options(), $this->fid);
    db_query("DELETE FROM {flag_types} WHERE fid = %d", $this->fid);
    foreach ($this->types as $type) {
      db_query("INSERT INTO {flag_types} (fid, type) VALUES (%d, '%s')", $this->fid, $type);
    }
  }

  /**
   * Saves a new flag to the database. Better use save().
   */
  function insert() {
    if (function_exists('db_last_insert_id')) {
      // Drupal 6. We have a 'serial' primary key.
      db_query("INSERT INTO {flags} (content_type, name, title, flag_short, flag_long, flag_message, unflag_short, unflag_long, unflag_message, roles, global, options) VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s')", $this->content_type, $this->name, $this->title, $this->flag_short, $this->flag_long, $this->flag_message, $this->unflag_short, $this->unflag_long, $this->unflag_message, implode(',', $this->roles), $this->global, $this->get_serialized_options());
      $this->fid = db_last_insert_id('flags', 'fid');
    }
    else {
      // Drupal 5. We have an 'integer' primary key.
      $this->fid = db_next_id('{flags}_fid');
      db_query("INSERT INTO {flags} (fid, content_type, name, title, flag_short, flag_long, flag_message, unflag_short, unflag_long, unflag_message, roles, global, options) VALUES (%d, '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s')", $this->fid, $this->content_type, $this->name, $this->title, $this->flag_short, $this->flag_long, $this->flag_message, $this->unflag_short, $this->unflag_long, $this->unflag_message, implode(',', $this->roles), $this->global, $this->get_serialized_options());
    }
    foreach ($this->types as $type) {
      db_query("INSERT INTO {flag_types} (fid, type) VALUES (%d, '%s')", $this->fid, $type);
    }
  }

  /**
   * Options are stored serialized in the database.
   */  
  function get_serialized_options() {
    $option_names = array_keys($this->default_options());
    $options = array();
    foreach ($option_names as $option) {
      $options[$option] = $this->$option;
    }
    return serialize($options);
  }

  /**
   * Deletes a flag from the database.
   */
  function delete() {
    db_query('DELETE FROM {flags} WHERE fid = %d', $this->fid);
    db_query('DELETE FROM {flag_content} WHERE fid = %d', $this->fid);
    db_query('DELETE FROM {flag_types} WHERE fid = %d', $this->fid);
    db_query('DELETE FROM {flag_counts} WHERE fid = %d', $this->fid);
  }

  /**
   * Renders a flag/unflag link. This is a wrapper around theme('flag') that,
   * in Drupal 6, easily channels the call to the right template file.
   *
   * For parameters docmentation, see theme_flag().
   */
  function theme($action, $content_id, $after_flagging = FALSE) {
    // The following 'if ... else' is because I don't want to maintain two
    // versions of this file because of 1 line of code.
    if (function_exists('theme_get_registry')) {
      // We're running Drupal 6.
      return theme($this->theme_suggestions(), $this, $action, $content_id, $after_flagging);
    }
    else {
      // We're running Drupal 5. Noting to do: The theme_suggestions[] are
      // handed to phptemplate in phptemplate_flag(), if the user bothered to
      // copy that function into her 'template.php'.
      return theme('flag', $this, $action, $content_id, $after_flagging);
    }
  }

  /**
   * Provides an array of possible themes to try for a given flag.
   */
  function theme_suggestions() {
    $suggestions = array();
    $suggestions[] = 'flag_' . $this->name;
    $suggestions[] = 'flag';
    return $suggestions;
  }
}

/**
 * Implements a node flag.
 */
class flag_node extends flag_flag {
  function default_options() {
    return array(
      'show_on_page' => TRUE,
      'show_on_teaser' => TRUE,
      'show_on_form' => FALSE,
    );
  }
  
  function options_form(&$form) {
    parent::options_form($form);
    $form['display']['show_on_teaser'] = array(
      '#type' => 'checkbox',
      '#title' => t('Display link on node teaser'),
      '#default_value' => $this->show_on_teaser,
    );
    $form['display']['show_on_page'] = array(
      '#type' => 'checkbox',
      '#title' => t('Display link on node page'),
      '#default_value' => $this->show_on_page,
    );
    $form['display']['show_on_form'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show checkbox on node edit form'),
      '#default_value' => $this->show_on_form,
    );
  }
}

/**
 * Implements a comment flag.
 */
class flag_comment extends flag_flag  {
  function default_options() {
    return array(
      'show_on_comment' => TRUE,
    );
  }
  
  function options_form(&$form) {
    parent::options_form($form);
    $form['display']['show_on_comment'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show checkbox on comments'),
      '#default_value' => $this->show_on_comment,
    );
  }
}

/**
 * Implements a user flag.
 */
class flag_user extends flag_flag {
  function default_options() {
    return array(
      'show_on_profile' => TRUE,
    );
  }
  
  function options_form(&$form) {
    parent::options_form($form);
    $form['types'] = array(
      // A user flag doesn't support node types. (Maybe will support roles instead, in the future.)
      '#type' => 'value',
      '#value' => array(0 => 0),
    );
    $form['display']['show_on_profile'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show checkbox on user profile page'),
      '#default_value' => $this->show_on_profile,
    );
  }
}

/**
 * A dummy flag to be used where the real implementation can't be found.
 */
class flag_broken extends flag_flag {
  function options_form(&$form) {
    $form = array();
    $form['error'] = array(
      '#value' => '<div class="error">'. t("The module providing this flag wasn't found, or this flag type, %type, isn't valid.", array('%type' => $this->content_type)) .'</div>',
    );
  }
}


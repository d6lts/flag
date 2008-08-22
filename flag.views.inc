<?php
// $Id$

/**
 * @file
 * Provides support for the Views module.
 */

/**
 * Implementation of hook_views_data().
 */
function flag_views_data() {
  $data = array();

  $data['flag_content']['table']['group'] = t('Flags');
  $data['flag_counts']['table']['group'] = t('Flags');

  $data['flag_content']['uid'] = array(
    'title' => t('User'),
    'help' => t('The user that flagged an item.'),
    'relationship' => array(
      'base' => 'users',
      'field' => 'uid',
      'handler' => 'views_handler_relationship',
      'label' => t('Flag user'),
    ),
  );

  $data['flag_content']['timestamp'] = array(
    'title' => t('Flagged time'),
    'help' => t('Display the time the content was flagged by a user.'),
    'field' => array(
      'handler' => 'views_handler_field_date',
      'click sortable' => TRUE,
     ),
    'sort' => array(
      'handler' => 'views_handler_sort_date',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_date',
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_date',
    ),
  );

  // Specialized is null/is not null filter.
  $data['flag_content']['flagged'] = array(
    'title' => t('Flagged'),
    'help' => t('Filter to ensure content has or has not been flagged.'),
    'real field' => 'uid',
    'filter' => array(
      'handler' => 'flag_handler_filter_flagged',
      'label' => t('Flagged'),
    ),
  );

  // Flag content links.
  $data['flag_content']['ops'] = array(
    'title' => t('Flag link'),
    'help' => t('Display flag/unflag link.'),
    'field' => array(
      'handler' => 'flag_handler_field_ops',
    ),
  );

  $data['flag_counts']['count'] = array(
    'title' => t('Flag counter'),
    'help' => t('The number of times a piece of content is flagged by any user.'),
    'field' => array(
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
     ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_numeric',
    ),
  );

  return $data;
}

/**
 * Implementation of hook_views_data_alter().
 */
function flag_views_data_alter(&$data) {

  foreach (array_keys(flag_fetch_definition()) as $flag_type) {
    $flag = flag_flag::factory_by_content_type($flag_type);
    $info = $flag->get_views_info();

    // Add the flag relationship.
    $data[$info['views table']]['flag_content_rel'] = array(
      'group' => t('Flags'),
      'title' => $info['title'],
      'help' => $info['help'],
      'real field' => $info['join field'],
      'relationship' => array(
        'flag type' => $flag_type,
        'handler' => 'flag_handler_relationship_content',
        'label' => t('flag'),
        'base' => 'flag_content',
        'base field' => 'content_id',
      ),
    );
  
    // Add the flag counter relationship.
    $data[$info['views table']]['flag_count_rel'] = array(
      'group' => t('Flags'),
      'title' => $info['counter title'],
      'help' => $info['counter help'],
      'real field' => $info['join field'],
      'relationship' => array(
        'flag type' => $flag_type,
        'handler' => 'flag_handler_relationship_counts',
        'label' => t('counter'),
        'base' => 'flag_counts',
        'base field' => 'content_id',
      ),
    );
  }

}

/**
 * A helper function that creates a radio list of available flags.
 *
 * This function is used to select the desired flag when setting up flag
 * relationships and fields.
 */
function flag_views_flag_config_form($form_type, $content_type, $current_flag) {
  $flags = flag_get_flags($content_type);

  $options = array();
  foreach ($flags as $flag) {
    $options[$flag->name] = $flag->get_title();
  }

  $form = array(
    '#type' => $form_type,
    '#title' => t('Flag'),
    '#options' => $options,
    '#default_value' => $current_flag,
    '#required' => TRUE,
  );

  return $form;
}

/**
 * Helper function that gets the first defined flag and returns its name.
 */
function flag_views_flag_default($content_type) {
  static $default_flag = array();

  if (!array_key_exists($content_type, $default_flag)) {
    $flag = array_shift(flag_get_flags($content_type));
    $default_flag[$content_type] = $flag ? $flag->name : NULL;
  }

  return $default_flag[$content_type];
}

/**
 * Specialized relationship handler associating flags and content.
 */
class flag_handler_relationship_content extends views_handler_relationship {
  
  function option_definition() {
    $options = parent::option_definition();

    $content_type = $this->definition['flag type'];
    $options['flag'] = array('default' => flag_views_flag_default($content_type));
    $options['user_scope'] = array('default' => 'current');
    $options['required'] = array('default' => 1);

    return $options;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $content_type = $this->definition['flag type'];
    $form['label']['#description'] .= ' '. t('The name of the selected flag makes a good label.');
    $form['flag'] = flag_views_flag_config_form('radios', $content_type, $this->options['flag']);

    $form['user_scope'] = array(
      '#type' => 'radios',
      '#title' => t('By'),
      '#options' => array('current' => t('Current user'), 'any' => t('Any user')),
      '#default_value' => $this->options['user_scope'],
    );

    $form['required']['#title'] = t('Include only flagged content');
    $form['required']['#description'] = t('If checked, only content that has this flag will be included. Leave unchecked if needing to display lists of specifically unflagged content.');

    if (!$form['flag']['#options']) {
      $form = array(
        'error' => array(
          '#value' => '<p class="error">' . t('No %type flags exist. You must first <a href="@create-url">create a %type flag</a> before being able to use one.', array('%type' => $content_type, '@create-url' => 'admin/build/flags')) . '</p>',
        ),
      );
      $form_state['no flags exist'] = TRUE;
    }
  }

  function options_validate($form, &$form_state) {
    if (!empty($form_state['no flags exist'])) {
      form_error($form, t('You must first create a flag'));
    }
  }

  function admin_summary() {
    return $this->options['user_scope'] == 'current' ? t('by current user') : t('by any user');
  }

  function ui_name() {
    // We put the bookmark name in the UI string to save space.
    return t('!group: !title', array('!group' => $this->definition['group'], '!title' => $this->options['flag']));
  }

  /**
   * Called to implement a relationship in a query.
   */
  function query() {
    $flag = flag_get_flag($this->options['flag']);

    $this->ensure_my_table();

    $join = new views_join();
    $join->definition = array(
      'table' => $this->definition['base'],
      'field' => $this->definition['base field'],
      'left_table' => $this->table_alias,
      'left_field' => $this->real_field,
    );
    if (!empty($this->options['required'])) {
      $join->definition['type'] = 'INNER';
    }
    $join->definition['extra'] = array(
      array(
        'field' => 'fid',
        'value' => $flag->fid,
        'numeric' => TRUE,
      ),
    );
    if ($this->options['user_scope'] == 'current' && !$flag->global) {
      $join->definition['extra'][] = array(
        'field' => 'uid',
        'value' => '***CURRENT_USER***',
        'numeric' => TRUE,
      );
    }
    $join->construct();
    $join->adjusted = TRUE;

    $alias = $join->definition['table'] . '_' . $join->definition['left_table'];

    $this->alias = $this->query->add_relationship($alias, $join, 'flag_content', $this->relationship);
  }
}

/**
 * Specialized relationship handler associating flag counts and content.
 */
class flag_handler_relationship_counts extends views_handler_relationship {

  function option_definition() {
    $options = parent::option_definition();
    
    $content_type = $this->definition['flag type'];
    $options['flag'] = array('default' => flag_views_flag_default($content_type));
    $options['required'] = array('default' => 1);

    return $options;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $content_type = $this->definition['flag type'];
    $form['flag'] = flag_views_flag_config_form('radios', $content_type, $this->options['flag']);

    $form['required']['#title'] = t('Include only flagged content');
    $form['required']['#description'] = t('If checked, only content that is flagged will be included.');
  }

  function admin_summary() {
    // Nothing to show.
  }

  function ui_name() {
    // We put the bookmark name in the UI string to save space.
    return t('!group: !title counter', array('!group' => $this->definition['group'], '!title' => $this->options['flag']));
  }

  /**
   * Called to implement a relationship in a query.
   */
  function query() {
    $flag = flag_get_flag($this->options['flag']);

    $this->ensure_my_table();

    $join = new views_join();
    $join->definition = array(
      'table' => $this->definition['base'],
      'field' => $this->definition['base field'],
      'left_table' => $this->table_alias,
      'left_field' => $this->real_field,
    );
    $join->definition['extra'] = array(
      array(
        'field' => 'fid',
        'value' => $flag->fid,
        'numeric' => TRUE,
      ),
    );
    if (!empty($this->options['required'])) {
      $join->definition['type'] = 'INNER';
      // Unfortunately, we may have zeros in our table,
      // so we filter them out.
      // @todo Make sure this doesn't happen, and remove this code.
      $join->definition['extra'][] = array(
        'field' => 'count',
        'operator' => '>',
        'value' => '0',
        'numeric' => TRUE,
      );
    }
    $join->construct();
    $join->adjusted = TRUE;

    $alias = $join->definition['table'] . '_' . $join->definition['left_table'];

    $this->alias = $this->query->add_relationship($alias, $join, 'flag_counts', $this->relationship);
  }
}

/**
 * Views field handler for the Flag operations links (flag/unflag).
 */
class flag_handler_field_ops extends views_handler_field {

  /**
   * Returns the flag object associated with our field.
   *
   * A field is in some relationship. This function reaches out for this
   * relationship and reads its 'flag' option, which holds the flag name.
   */
  function get_flag() {
    $flag_name = $this->view->relationship[$this->options['relationship']]->options['flag'];
    return flag_get_flag($flag_name);
  }

  /**
   * Return the the relationship we're linked to. That is, the alias for its
   * table (which is suitbale for use with the various methods of the 'query'
   * object).
   */
  function get_parent_relationship() {
    $parent = $this->view->relationship[$this->options['relationship']]->options['relationship'];
    if (!$parent || $parent == 'none') {
      return NULL; // Base query table.
    }
    else {
      return $this->view->relationship[$parent]->alias;
    }
  }

  /**
   * Override base ::query(). The purpose here is to make it possible for the
   * render() method to know two things: what's the content ID, and whether
   * it's flagged.
   */
  function query() {
    $parent = $this->get_parent_relationship();
    $flag = $this->get_flag();
    $info = $flag->get_views_info();

    // Find out if the content is flagged. We can't just peek at some field in
    // our loaded table because it doesn't always reflect the user browsing the
    // page. So we explicitly add the flag_content table to find this out.
    $join = new views_join();
    $join->construct('flag_content', $info['views table'], $info['join field'], 'content_id');
    $join->extra[] = array(
      'field' => 'fid',
      'value' => $flag->fid,
      'numeric' => TRUE,
    );
    if (!$flag->global) {
      $join->extra[] = array(
        'field' => 'uid',
        'value' => '***CURRENT_USER***',
        'numeric' => TRUE,
      );
    }
    $flag_table = $this->query->add_table('flag_content', $parent, $join);
    $this->aliases['is_flagged'] = $this->query->add_field($flag_table, 'content_id');

    // Next, find out the content ID. We can't add_field() on this table
    // (flag_content), because its content_id may be NULL (in case no user has
    // flagged this content, and it's a LEFT JOIN). So we reach to the parent
    // relationship and add_field() *its* content ID column.
    $left_table = $this->view->relationship[$this->options['relationship']]->table_alias;
    $this->aliases['content_id'] = $this->query->add_field($left_table, $info['join field']);
  }
  
  /**
   * Find out if the flag applies to each item seen on the page. It's done in a
   * separate DB query to to avoid complexity and to make 'many to one' tests
   * (e.g. checking user roles) possible without causing duplicate rows.
   */
  function pre_render($values) {
    $flag = $this->get_flag();

    if (!$flag->user_access()) {
      // User has no permission to use this flag.
      return;
    }

    $ids = array();
    foreach ($values as $row) {
      $content_id = $row->{$this->aliases['content_id']};
      if (isset($content_id)) {
        $ids[$content_id] = TRUE;
      }
    }
    $this->flag_applies = $ids ? $flag->applies_to_content_id_array(array_keys($ids)) : array();
  }
  
  function render($values) {
    $flag = $this->get_flag();

    if (!$flag->user_access()) {
      // User has no permission to use this flag.
      return;
    }

    $content_id = $values->{$this->aliases['content_id']};
    $is_flagged = $values->{$this->aliases['is_flagged']};
    
    if (!isset($this->flag_applies[$content_id])) {
      // Flag does not apply to this content.
      return;
    }
    
    return $flag->theme($is_flagged ? 'unflag' : 'flag', $content_id);
  }
}

/**
 * Handler to filter for content that has not been flagged.
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

/**
 * Implementation of hook_views_default_views()
 */
function flag_views_default_views() {
  $views = array();
  _flag_views_node_default_views($views);
  _flag_views_user_default_views($views);
  return $views;
}

function _flag_views_node_default_views(&$views) {
  $flags = flag_get_flags('node');

  foreach ($flags as $flag) {
    $menu_title = $flag->global ? drupal_ucfirst($flag->get_title()) : t('My !flag-title', array('!flag-title' => drupal_strtolower($flag->get_title())));

    $view = new view;
    $view->name = 'flag_'. $flag->name;
    $view->description = t('View for flag: !flag-title', array('!flag-title' => $flag->get_title()));
    $view->tag = 'flag';
    $view->view_php = '';
    $view->base_table = 'node';
    $view->is_cacheable = '0';
    $view->api_version = 2;
    $view->disabled = FALSE; /* Edit this to true to make a default view disabled initially */
    $handler = $view->new_display('default', 'Defaults', 'default');
    $handler->override_option('relationships', array(
      'flag_content_rel' => array(
        'label' => $flag->name,
        'required' => 1,
        'flag' => $flag->name,
        'user_scope' => 'current',
        'id' => 'flag_content_rel',
        'table' => 'node',
        'field' => 'flag_content_rel',
        'relationship' => 'none',
      ),
      'uid' => array(
        'label' => 'user',
        'required' => 0,
        'id' => 'uid',
        'table' => 'flag_content',
        'field' => 'uid',
        'relationship' => 'flag_content_rel',
      ),
    ));

    $fields = array(
      'type' => array(
        'id' => 'type',
        'table' => 'node',
        'field' => 'type',
        'label' => 'Type',
      ),
      'title' => array(
        'id' => 'title',
        'table' => 'node',
        'field' => 'title',
        'label' => 'Title',
        'link_to_node' => 1,
      ),
      'name' => array(
        'label' => 'Author',
        'link_to_user' => 1,
        'id' => 'name',
        'table' => 'users',
        'field' => 'name',
      ),
    );

    if (module_exists('comment')) {
      $fields += array(
        'comment_count' => array(
          'id' => 'comment_count',
          'table' => 'node_comment_statistics',
          'field' => 'comment_count',
          'label' => 'Replies',
        ),
        'last_comment_timestamp' => array(
          'id' => 'last_comment_timestamp',
          'table' => 'node_comment_statistics',
          'field' => 'last_comment_timestamp',
          'label' => 'Last Post',
        ),
      );
    }

    $fields += array(
      'ops' => array(
        'label' => 'Ops',
        'id' => 'ops',
        'table' => 'flag_content',
        'field' => 'ops',
        'relationship' => 'flag_content_rel',
      ),
    );

    $handler->override_option('fields', $fields);
    $handler->override_option('filters', array(
      'status' => array(
        'operator' => '=',
        'value' => 1,
        'group' => '0',
        'exposed' => FALSE,
        'expose' => array(
          'operator' => FALSE,
          'label' => '',
        ),
        'id' => 'status',
        'table' => 'node',
        'field' => 'status',
        'relationship' => 'none',
      ),
    ));
    $handler->override_option('access', array(
      'type' => 'role',
      'role' => drupal_map_assoc($flag->roles),
      'perm' => '',
    ));

    $style_options = array(
      'columns' => array(),
      'default' => module_exists('comment') ? 'last_comment_timestamp' : 'title',
      'info' => array(
        'type' => array(
          'sortable' => TRUE,
        ),
        'title' => array(
          'sortable' => TRUE,
        ),
        'name' => array(
          'sortable' => TRUE,
        ),
      ),
      'override' => FALSE,
      'order' => 'asc',
    );

    if (module_exists('comment')) {
      $style_options['info'] += array(
        'comment_count' => array(
          'sortable' => TRUE,
        ),
        'last_comment_timestamp' => array(
          'sortable' => TRUE,
        ),
      );
    }

    $handler->override_option('title', $menu_title);
    $handler->override_option('items_per_page', '25');
    $handler->override_option('use_pager', TRUE);
    $handler->override_option('style_plugin', 'table');
    $handler->override_option('style_options', $style_options);
    $handler = $view->new_display('page', 'Page', 'page_1');
    $handler->override_option('path', 'flags/'. $flag->name);
    $handler->override_option('menu', array(
      'type' => 'normal',
      'title' => $menu_title,
      'weight' => '0',
    ));
    $handler->override_option('tab_options', array(
      'type' => 'none',
      'title' => NULL,
      'weight' => NULL,
    ));

    $views[$view->name] = $view;
  }
}

function _flag_views_user_default_views(&$views) {
  $flags = flag_get_flags('user');

  foreach ($flags as $flag) {
    $menu_title = $flag->global ? drupal_ucfirst($flag->get_title()) : t('My !flag-title', array('!flag-title' => drupal_strtolower($flag->get_title())));

    $view = new view;
    $view->name = 'flag_' . $flag->name;
    $view->description = t('View for flag: !flag-title', array('!flag-title' => $flag->get_title()));
    $view->tag = 'flag';
    $view->view_php = '';
    $view->base_table = 'users';
    $view->is_cacheable = FALSE;
    $view->api_version = 2;
    $view->disabled = FALSE; /* Edit this to true to make a default view disabled initially */
    $handler = $view->new_display('default', 'Defaults', 'default');

    $handler->override_option('relationships', array(
      'flag_content_rel' => array(
        'label' => $flag->name,
        'required' => 1,
        'flag' => $flag->name,
        'user_scope' => 'current',
        'id' => 'flag_content_rel',
        'table' => 'users',
        'field' => 'flag_content_rel',
        'relationship' => 'none',
      ),
    ));

    $handler->override_option('fields', array(
      'name' => array(
        'label' => 'Name',
        'link_to_user' => 1,
        'exclude' => 0,
        'id' => 'name',
        'table' => 'users',
        'field' => 'name',
        'relationship' => 'none',
      ),
      'ops' => array(
        'label' => 'Ops',
        'exclude' => 0,
        'id' => 'ops',
        'table' => 'flag_content',
        'field' => 'ops',
        'relationship' => 'flag_content_rel',
      ),
    ));

    $handler->override_option('style_plugin', 'table');
    $handler->override_option('style_options', array(
      'grouping' => '',
      'override' => 1,
      'sticky' => 0,
      'order' => 'asc',
      'columns' => array(
        'name' => 'name',
        'ops' => 'ops',
      ),
      'info' => array(
        'name' => array(
          'sortable' => 1,
          'separator' => '',
        ),
        'ops' => array(
          'separator' => '',
        ),
      ),
      'default' => 'name',
    ));

    $handler->override_option('access', array(
      'type' => 'none',
      'role' => drupal_map_assoc($flag->roles),
      'perm' => '',
    ));

    $handler->override_option('title', $menu_title);
    $handler->override_option('items_per_page', 30);
    $handler->override_option('use_pager', TRUE);
    $handler->override_option('empty', 'No users are flagged.');
    $handler->override_option('empty_format', '1');

    $handler = $view->new_display('page', 'Page', 'page_1');
    $handler->override_option('path', 'flags/' . $flag->name);
    $handler->override_option('menu', array(
      'type' => 'normal',
      'title' => $menu_title,
      'weight' => '0',
    ));
    $handler->override_option('tab_options', array(
      'type' => 'none',
      'title' => NULL,
      'weight' => 0,
    ));
    
    $views[$view->name] = $view;
  }
}


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
    'title' => t('User ID'),
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
    'help' => t('Display flag/unflag links.'),
    'real field' => 'uid',
    'field' => array(
      'handler' => 'flag_handler_field_ops',
    ),
  );

  // Flag count totals.
  $data['flag_counts']['count'] = array(
    'title' => t('Flag Count'),
    'help' => t('The number of times a piece of content has been flagged total by any user.'),
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
  foreach (array('node' => 'nid', 'comments' => 'cid') as $table => $join_field) {
    // Flag relationship.
    $data[$table]['flag_content_rel'] = array(
      'group' => t('Flags'),
      'title' => t('Flagged'),
      'help' => t('Limit results to a particular flag, or display information about the flags on a piece of content.'),
      'real field' => $join_field,
      'relationship' => array(
        'handler' => 'flag_handler_relationship_content',
        'base' => 'flag_content',
        'field' => 'content_id',
        'label' => t('flag'),
      ),
    );
  
    // Flag-count relationship.
    $data[$table]['flag_counts_rel'] = array(
      'group' => t('Flags'),
      'title' => t('Flag counts'),
      'help' => t('Add information about the flag total counts.'),
      'real field' => $join_field,
      'relationship' => array(
        'handler' => 'flag_handler_relationship_counts',
        'base' => 'flag_counts',
        'field' => 'content_id',
        'label' => t('flag'),
      ),
    );

    if ($table == 'comments') {
      // Make the titles distinguishable or else user will see duplicates in the list.
      $data[$table]['flag_content_rel']['title'] .= ' ' . t('(comments)');
      $data[$table]['flag_counts_rel' ]['title'] .= ' ' . t('(comments)');
    }
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

  foreach ($flags as $flag) {
    // Localize Flag titles.
    $flag = flag_process_labels($flag, NULL, NULL, array('title'));
    $options[$flag->name] = $flag->title;
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
  static $default_flag;

  if (!isset($default_flag)) {
    $flag = array_shift(flag_get_flags($content_type));
    $default_flag = $flag->name;
  }

  return $default_flag;
}

/**
 * Specialized relationship handler associating flags and content.
 */
class flag_handler_relationship_content extends views_handler_relationship {
  function options(&$options) {
    parent::options($options);
    $content_type =$this->definition['real field'] == 'nid' ? 'node' : 'comment';
    $options['flag'] = flag_views_flag_default($content_type);
    $options['user_scope'] = 'current';
    $options['required'] = 1;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $content_type = $this->definition['real field'] == 'nid' ? 'node' : 'comment';
    $form['label']['#description'] .= ' '. t('The name of the selected flag makes a good label.');
    $form['flag'] = flag_views_flag_config_form('radios', $content_type, $this->options['flag']);
    $form['flag']['#title'] = t('Flagged');

    $form['user_scope'] = array(
      '#type' => 'radios',
      '#title' => t('By'),
      '#options' => array('current' => t('Current user'), 'any' => t('Any user')),
      '#default_value' => $this->options['user_scope'],
    );

    $form['required']['#title'] = t('Include only flagged content');
    $form['required']['#description'] = t('If checked, only content that has this flag will be included. Leave unchecked if needing to display lists of specifically unflagged content.');
  }

  function admin_summary() {
    return $this->options['flag'] .' - '. ($this->options['user_scope'] == 'current' ? 'current user' : 'any user');
  }

  /**
   * Called to implement a relationship in a query.
   */
  function query() {
    $flag = flag_get_flag($this->options['flag']);

    // Figure out what base table this relationship brings to the party.
    $join = new views_join();
    $join->definition = array(
      'table' => 'flag_content',
      'field' => 'content_id',
      'left_table' => $flag->content_type == 'node' ? 'node' : 'comments',
      'left_field' => $flag->content_type == 'node' ? 'nid' : 'cid',
    );

    if (!empty($this->options['required'])) {
      $join->definition['type'] = 'INNER';
    }

    $flag = flag_get_flag($this->options['flag']);
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

    $alias = $join->definition['table'] . '_' . $join->definition['left_table'];

    $this->alias = $this->query->add_relationship($alias, $join, 'flag_content', $this->relationship);
  }
}


/**
 * Specialized relationship handler associating flag counts and content.
 */
class flag_handler_relationship_counts extends views_handler_relationship {
  function options(&$options) {
    parent::options($options);
    $content_type = $this->definition['real field'] == 'nid' ? 'node' : 'comment';
    $options['flag'] = flag_views_flag_default($content_type);
    $options['required'] = 1;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $content_type = $this->definition['real field'] == 'nid' ? 'node' : 'comment';
    $form['flag'] = flag_views_flag_config_form('radios', $content_type, $this->options['flag']);

    $form['required']['#title'] = t('Include only content flagged at least once');
    $form['required']['#description'] = t('If checked, only content that has been flagged will be included.');
  }

  function admin_summary() {
    return $this->options['flag'];
  }

  /**
   * Called to implement a relationship in a query.
   */
  function query() {
    $flag = flag_get_flag($this->options['flag']);

    // Figure out what base table this relationship brings to the party.
    $join = new views_join();
    $join->definition = array(
      'table' => 'flag_counts',
      'field' => 'content_id',
      'left_table' => $flag->content_type == 'node' ? 'node' : 'comments',
      'left_field' => $flag->content_type == 'node' ? 'nid' : 'cid',
    );

    if (!empty($this->options['required'])) {
      $join->definition['type'] = 'INNER';
    }

    $flag = flag_get_flag($this->options['flag']);
    $join->definition['extra'] = array(
      array(
        'field' => 'fid',
        'value' => $flag->fid,
        'numeric' => TRUE,
      ),
    );
    $join->construct();

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
   * Override base ::query() to pull in some fields we need. Namely, the node
   * type and the content ID we're linked to.
   */
  function query() {
    $flag = $this->get_flag();

    // We add the extra fields to the parent relationship. We can't add it to
    // _this_ relationship (by simply registering it in 'additional_fields')
    // because we haven't described to Views how to connect the node/comment
    // table to ours.
    $parent = $this->get_parent_relationship();

    if ($flag->content_type == 'node') {
      $node_table_alias = $this->query->ensure_table('node', $parent);
      $this->content_id_alias = $this->query->add_field($node_table_alias, 'nid');
      // And we need node's 'type' field in the render() method to determine if
      // the flag applies to the node.
      $this->node_type_alias  = $this->query->add_field($node_table_alias, 'type');
    }
    elseif ($flag->content_type == 'comment') {
      $comment_table_alias = $this->query->ensure_table('comment', $parent);
      $this->content_id_alias = $this->query->add_field($comment_table_alias, 'cid');
    }

    parent::query();
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

  function render($values) {
    $flag = $this->get_flag();
    $node_type = $flag->content_type == 'node' ? $values->{$this->node_type_alias} : NULL;
    $content_id = $values->{$this->content_id_alias};
    $is_flagged = !is_null($values->{$this->field_alias});

    if (is_null($content_id)) {
      // Content doesn't exist. This may happen, for example, when we LEFT JOIN to
      // another node relationship, e.g. to "parent book page" and when there's no parent.
      return;
    }

    if (!flag_access($flag) || !flag_content_enabled($flag, $flag->content_type, $node_type)) {
      // Flag does not apply to this user or node type.
      return;
    }

    // Token replacements.
    $flag = flag_process_labels($flag, $flag->content_type, $content_id, array('flag_short', 'flag_long', 'flag_message'));
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
  $flags = flag_get_flags('node');

  foreach ($flags as $flag) {
    $flag = flag_process_labels($flag, NULL, NULL, array('title'));
    $menu_title = $flag->global ? drupal_ucfirst($flag->title) : t('My !flag-title', array('!flag-title' => drupal_strtolower($flag->title)));

    $view = new view;
    $view->name = 'flag_'. $flag->name;
    $view->description = t('View for flag: !flag-title', array('!flag-title' => $flag->title));
    $view->tag = t('default');
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
  return $views;
}


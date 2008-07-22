// $Id$

Theming instructions
====================

Template file
-------------
In order to customize flag theming:

- Copy the 'flag.tpl.php' template file into your theme's folder.

- Copy the 'phptemplate_flag()' function, which is at the end of this
  file, into your 'template.php'.

- Edit 'flag.tpl.php' to your liking.


Template variants
-----------------
In addition, the theme layer will first look for the template
'flag-<FLAG_NAME>.tpl.php' before it turns to 'flag.tpl.php'. This too
you should place in your theme's folder.


The phptemplate_flag()
----------------------
You should paste the following function into your 'tempalte.php'. This
function instructs PHPTempalte to use your template file, or files, when
theming a flag.


  function phptemplate_flag($flag, $action, $content_id, $after_flagging = FALSE) {
    // Create the variables that will be available in the template.
    $variables = array(
      'flag' => $flag,
      'action' => $action,
      'content_id' => $content_id,
      'after_flagging' => $after_flagging,
    );
    template_preprocess_flag($variables);

    // Prepare an array of suggested templates to use.
    $suggestions = array();
    foreach ($flag->theme_suggestions() as $suggestion) {
      $suggestions[] = str_replace('_', '-', $suggestion);
    }
    $suggestions = array_reverse($suggestions);

    // Hand control to the phptemplate engine.
    $output = _phptemplate_callback('flag', $variables, $suggestions);

    // Finally, handle Drupal 5's jQuery bug. See explanation in theme_flag().
    $output = strtr($output, "\r\n", '  ');
    return $output;
  }


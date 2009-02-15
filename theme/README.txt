// $Id$

Theming instructions
====================

You may want to visit the Theming Guide of this module, at:

  http://drupal.org/node/295346

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
'flag--<FLAG_NAME>.tpl.php' before it turns to 'flag.tpl.php'. This too
you should place in your theme's folder.


The phptemplate_flag()
----------------------
You should paste the following function into your 'template.php'. This
function instructs PHPTemplate to use your template file, or files, when
theming flags.


  function phptemplate_flag($flag, $action, $content_id, $after_flagging = FALSE) {
    return flag_phptemplate_adapter($flag, $action, $content_id, $after_flagging);
  }

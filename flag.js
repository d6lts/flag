// $Id$
Drupal.behaviors.flag = function() {
  // Note, extra indentation left here to maintain ease of patching for D5 version.

    /**
     * Flips a link. 'Flag this!' links turn into 'Unflag this!' and
     * vice versa.
     */
    function flipLink(element, settings) {
      // If this is a 'flag this' link...
      if ($(element).is('.flag')) {
        // ...then turn it into an 'unflag this' link;
        var newHtml = settings.unflag;
      }
      else {
        // else, turn it into a 'flag this' link.
        var newHtml = settings.flag;
      }
      updateLink(element, newHtml, settings);
    }

    /**
     * Helper function. Updates a link's HTML with a new one.
     */
    function updateLink(element, newHtml, settings) {
      var $newLink = $(newHtml);

      // Initially hide the message so we can fade it in.
      $('.flag-message', $newLink).css('display', 'none');

      // Reattach the behavior to the new link.
      if ($('a', $newLink).size() > 0) {
        $('a', $newLink).bind('click', function() { return flagClick(this, settings) });
      }
      else {
        $newLink.bind('click', function() { return flagClick(this, settings) });
      }

      // Find the wrapper of the old link.
      var $wrapper = $(element).parents('.flag-wrapper:first');
      if ($wrapper.length == 0) {
        // If no ancestor wrapper was found, or if the 'flag-wrapper' class is
        // attached to the <a> element itself, then take the element itself.
        $wrapper = $(element);
      }
      // Replace the old link with the new one.
      $wrapper.after($newLink).remove();

      $('.flag-message', $newLink).fadeIn();
    }
    
    // Click function for each Flag link.
    function flagClick(element, settings) {
      // Hide any other active messages.
      $('span.flag-message:visible').fadeOut();

      // Send POST request
      $.ajax({
        type: 'POST',
        url: element.href,
        data: { js: true },
        dataType: 'json',
        success: function (data) {
          // Display errors
          if (!data.status) {
            // Change link back
            flipLink(element, settings);
            return;
          }
        },
        error: function (xmlhttp) {
          alert('An HTTP error '+ xmlhttp.status +' occured.\n'+ element.href);
          // Change link back
          flipLink(element, settings);
        }
      });
      // Swap out the links.
      flipLink(element, settings);
      return false;
    }

    /**
     * Like alert(), but displays only once, to keep user from losing sanity.
     */
    var warn = function(message) {
      if (!Drupal.settings.flag.alertShown) {
        alert('Flag module: ' + message);
        Drupal.settings.flag.alertShown = true;
      }
    }

    /**
     * Returns the settings for a certain link element.
     */
    function getLinkSettings(element) {
      // The link URL is of the form ?q=flag/unflag/bookmarks/node/23,
      // so let's parse it to extract the flag name, the content type,
      // and the content ID.
      var matches = element.href.match(/flag\/(un)?flag\/(\w+)\/(\w+)\/(\d+)/);
      if (!matches) {
        warn("Error: Invalid flag URL '" + element.href + "'");
      }
      var flagName    = matches[2];
      var contentType = matches[3];
      var contentId   = matches[4];

      var slot = contentType + '_' + contentId;

      if (!Drupal.settings.flag.flags[flagName][slot]) {
        // Slot does not exist. Create.
        Drupal.settings.flag.flags[flagName][slot] = {};
      }
      // Return a reference to the settings slot.
      return Drupal.settings.flag.flags[flagName][slot];
    }

    // On load, bind the click behavior for all links on the page.
    for (flagName in Drupal.settings.flag.flags) {
      $('a.flag-' + flagName).click(function() {
        return flagClick(this, getLinkSettings(this));
      });
    }
  // Intentional extra indention.
}

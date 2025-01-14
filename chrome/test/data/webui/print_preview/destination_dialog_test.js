// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_dialog_test', function() {
  /** @enum {string} */
  const TestNames = {
    PrinterList: 'PrinterList',
    ShowProvisionalDialog: 'ShowProvisionalDialog',
    ReloadPrinterList: 'ReloadPrinterList',
    UserAccounts: 'UserAccounts',
  };

  const suiteName = 'DestinationDialogTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationDialogElement} */
    let dialog = null;

    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?print_preview.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinations = [];

    /** @type {!Array<!print_preview.RecentDestination>} */
    let recentDestinations = [];

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      // Create data classes
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      destinationStore = print_preview_test_utils.createDestinationStore();
      destinationStore.setCloudPrintInterface(cloudPrintInterface);
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      recentDestinations =
          [print_preview.makeRecentDestination(destinations[4])];
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          recentDestinations /* recentDestinations */);
      nativeLayer.setLocalDestinations(localDestinations);

      // Set up dialog
      dialog = document.createElement('print-preview-destination-dialog');
      dialog.activeUser = '';
      dialog.users = [];
      dialog.destinationStore = destinationStore;
      dialog.invitationStore = new print_preview.InvitationStore();
      document.body.appendChild(dialog);
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(function() {
            destinationStore.startLoadAllDestinations();
            dialog.show();
            return nativeLayer.whenCalled('getPrinters');
          })
          .then(function() {
            Polymer.dom.flush();
          });
    });

    // Test that destinations are correctly displayed in the lists.
    test(assert(TestNames.PrinterList), function() {
      const list = dialog.$$('print-preview-destination-list');

      const printerItems = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');

      const getDisplayedName = item => item.$$('.name').textContent;
      // 5 printers + Save as PDF
      assertEquals(6, printerItems.length);
      // Save as PDF shows up first.
      assertEquals(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          getDisplayedName(printerItems[0]));
      assertEquals(
          'rgb(32, 33, 36)',
          window.getComputedStyle(printerItems[0].$$('.name')).color);
      // FooName will be second since it was updated by the capabilities fetch.
      assertEquals('FooName', getDisplayedName(printerItems[1]));
      Array.from(printerItems).slice(2).forEach((item, index) => {
        assertEquals(destinations[index].displayName, getDisplayedName(item));
      });
    });

    // Test that clicking a provisional destination shows the provisional
    // destinations dialog, and that the escape key closes only the provisional
    // dialog when it is open, not the destinations dialog.
    test(assert(TestNames.ShowProvisionalDialog), function() {
      const provisionalDialog =
          dialog.$$('print-preview-provisional-destination-resolver');
      const provisionalDestination = {
        extensionId: 'ABC123',
        extensionName: 'ABC Printing',
        id: 'XYZDevice',
        name: 'XYZ',
        provisional: true,
      };

      // Set the extension destinations and force the destination store to
      // reload printers.
      nativeLayer.setExtensionDestinations([provisionalDestination]);
      destinationStore.onDestinationsReload();

      return nativeLayer.whenCalled('getPrinters')
          .then(function() {
            Polymer.dom.flush();
            assertFalse(provisionalDialog.$.dialog.open);
            const list = dialog.$$('print-preview-destination-list');
            const printerItems = list.shadowRoot.querySelectorAll(
                'print-preview-destination-list-item');

            // Should have 5 local destinations, Save as PDF + extension
            // destination.
            assertEquals(7, printerItems.length);
            const provisionalItem =
                Array.from(printerItems).find(printerItem => {
                  return printerItem.destination.id ===
                      provisionalDestination.id;
                });

            // Click the provisional destination to select it.
            provisionalItem.click();
            Polymer.dom.flush();
            assertTrue(provisionalDialog.$.dialog.open);

            // Send escape key on provisionalDialog. Destinations dialog should
            // not close.
            const whenClosed =
                test_util.eventToPromise('close', provisionalDialog);
            MockInteractions.keyEventOn(
                provisionalDialog, 'keydown', 19, [], 'Escape');
            Polymer.dom.flush();
            return whenClosed;
          })
          .then(function() {
            assertFalse(provisionalDialog.$.dialog.open);
            assertTrue(dialog.$.dialog.open);
          });
    });

    // Test that destinations are correctly cleared when the destination store
    // reloads the printer list.
    test(assert(TestNames.ReloadPrinterList), function() {
      const list = dialog.$$('print-preview-destination-list');

      const printerItems = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');

      assertEquals(6, printerItems.length);
      const oldPdfDestination = printerItems[0].destination;
      assertEquals(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          oldPdfDestination.id);
      const whenReset = test_util.eventToPromise(
          print_preview.DestinationStore.EventType.DESTINATIONS_RESET,
          destinationStore);
      cr.webUIListenerCallback('reload-printer-list');
      return whenReset.then(() => {
        Polymer.dom.flush();
        const printerItems = dialog.$.printList.shadowRoot.querySelectorAll(
            'print-preview-destination-list-item');
        assertEquals(6, printerItems.length);
        const newPdfDestination = printerItems[0].destination;
        assertEquals(
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
            newPdfDestination.id);
        assertNotEquals(oldPdfDestination, newPdfDestination);
      });
    });

    /**
     * @param {string} account The current active user account.
     * @param {number} numUsers The total number of users that are signed in.
     */
    function assertSignedInState(account, numUsers) {
      const signedIn = account !== '';
      assertEquals(signedIn, dialog.$.cloudprintPromo.hidden);
      assertEquals(!signedIn, dialog.$$('.user-info').hidden);

      if (numUsers > 0) {
        const userSelect = dialog.$$('.md-select');
        const userSelectOptions = userSelect.querySelectorAll('option');
        assertEquals(numUsers + 1, userSelectOptions.length);
        assertEquals('', userSelectOptions[numUsers].value);
        assertEquals(account, userSelect.value);
      }
    }

    /**
     * @param {number} numPrinters The total number of available printers.
     * @param {string} account The current active user account.
     */
    function assertNumPrintersWithDriveAccount(numPrinters, account) {
      const list = dialog.$$('print-preview-destination-list');
      const printerItems = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item:not([hidden])');
      assertEquals(numPrinters, printerItems.length);
      const drivePrinter = Array.from(printerItems).find(item => {
        return item.destination.id ===
            print_preview.Destination.GooglePromotedId.DOCS;
      });
      assertEquals(!!drivePrinter, account !== '');
      if (drivePrinter) {
        assertEquals(account, drivePrinter.destination.account);
      }
    }

    // Test that signing in and switching accounts works as expected.
    test(assert(TestNames.UserAccounts), function() {
      // Set up the cloud print interface with Google Drive printer for a couple
      // different accounts.
      const user1 = 'foo@chromium.org';
      const user2 = 'bar@chromium.org';
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(user1));
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(user2));

      // Check that both cloud print promo and dropdown are hidden when cloud
      // print is disabled.
      assertTrue(dialog.$.cloudprintPromo.hidden);
      assertTrue(dialog.$$('.user-info').hidden);
      const userSelect = dialog.$$('.md-select');

      // Enable cloud print.
      dialog.cloudPrintState = print_preview.CloudPrintState.NOT_SIGNED_IN;
      assertSignedInState('', 0);

      // 6 printers, no Google drive (since not signed in).
      assertNumPrintersWithDriveAccount(6, '');

      // Simulate signing in to an account.
      destinationStore.setActiveUser(user1);
      dialog.$.cloudprintPromo.querySelector('[is=\'action-link\']').click();
      return nativeLayer.whenCalled('signIn')
          .then(addAccount => {
            assertFalse(addAccount);
            nativeLayer.resetResolver('signIn');
            dialog.cloudPrintState = print_preview.CloudPrintState.SIGNED_IN;
            dialog.activeUser = user1;
            dialog.users = [user1];
            Polymer.dom.flush();

            // Promo is hidden and select shows the signed in user.
            assertSignedInState(user1, 1);

            // Now have 7 printers (Google Drive), with user1 signed in.
            assertNumPrintersWithDriveAccount(7, user1);

            // Simulate signing into a second account.
            userSelect.value = '';
            userSelect.dispatchEvent(new CustomEvent('change'));
            return nativeLayer.whenCalled('signIn');
          })
          .then(addAccount => {
            assertTrue(addAccount);
            dialog.users = [user1, user2];
            Polymer.dom.flush();

            // Promo is hidden and select shows the signed in user.
            assertSignedInState(user1, 2);

            // Still have 7 printers (Google Drive), with user1 signed in.
            assertNumPrintersWithDriveAccount(7, user1);

            // Select the second account.
            const whenEventFired =
                test_util.eventToPromise('account-change', dialog);
            userSelect.value = user2;
            userSelect.dispatchEvent(new CustomEvent('change'));
            return whenEventFired;
          })
          .then(() => {
            Polymer.dom.flush();

            // Check printers were temporarily cleared.
            assertNumPrintersWithDriveAccount(0, '');

            // This will all be done by app.js and user_info.js in response to
            // the account-change event.
            destinationStore.setActiveUser(user2);
            dialog.activeUser = user2;
            const whenInserted = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
                destinationStore);
            destinationStore.reloadUserCookieBasedDestinations(user2);

            return whenInserted;
          })
          .then(() => {
            Polymer.dom.flush();

            assertSignedInState(user2, 2);

            // 7 printers (Google Drive), with user2 signed in.
            assertNumPrintersWithDriveAccount(7, user2);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});

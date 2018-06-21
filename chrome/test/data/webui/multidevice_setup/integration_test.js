// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of integration tests for MultiDevice setup WebUI. */
cr.define('multidevice_setup', () => {
  function registerIntegrationTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * MultiDeviceSetup created before each test. Defined in setUp.
       * @type {MultiDeviceSetup|undefined}
       */
      let multiDeviceSetupElement;

      /**
       * Forward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let forwardButton;

      /**
       * Backward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let backwardButton;

      const FAILURE = 'setup-failed-page';
      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      // This is a safety check because it is easy to lost track of parameters.
      let verifySetupParameters = function(uiMode, mojoResponseCode) {
        assertEquals(multiDeviceSetupElement.uiMode, uiMode);
        assertEquals(
            multiDeviceSetupElement.mojoService_.responseCode,
            mojoResponseCode);
      };

      setup(() => {
        multiDeviceSetupElement = document.createElement('multidevice-setup');
        document.body.appendChild(multiDeviceSetupElement);
        multiDeviceSetupElement.mojoService_ =
            new multidevice_setup.FakeMojoService();
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;
        forwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #forward');
        backwardButton =
            multiDeviceSetupElement.$$('button-bar /deep/ #backward');
      });

      // From SetupFailedPage

      test('SetupFailedPage backward button closes UI', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());
        multiDeviceSetupElement.visiblePageName_ = FAILURE;
        backwardButton.click();
      });

      test('SetupFailedPage forward button goes to start page', () => {
        multiDeviceSetupElement.visiblePageName_ = FAILURE;
        forwardButton.click();
        Polymer.dom.flush();
        assertEquals(
            multiDeviceSetupElement.$$('iron-pages > .iron-selected').is,
            START);
      });

      // From SetupSucceededPage

      test('SetupSucceededPage forward button closes UI', done => {
        multiDeviceSetupElement.visiblePageName_ = SUCCESS;
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());
        forwardButton.click();
      });

      // From StartSetupPage

      // OOBE

      test('StartSetupPage backward button continues OOBE (OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => {
          assertFalse(
              multiDeviceSetupElement.mojoService_.settingHostInBackground);
          done();
        });

        multiDeviceSetupElement.visiblePageName_ = START;
        multiDeviceSetupElement.mojoService_.responseCode =
            multidevice_setup.SetBetterTogetherHostResponseCode.SUCCESS;
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.OOBE;

        backwardButton.click();
      });

      test(
          'StartSetupPage forward button sets host in backround and ' +
              'continues OOBE (OOBE).',
          done => {
            multiDeviceSetupElement.addEventListener('setup-exited', () => {
              assertTrue(
                  multiDeviceSetupElement.mojoService_.settingHostInBackground);
              done();
            });

            multiDeviceSetupElement.visiblePageName_ = START;
            multiDeviceSetupElement.mojoService_.responseCode =
                multidevice_setup.SetBetterTogetherHostResponseCode.SUCCESS;
            multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.OOBE;

            forwardButton.click();
          });

      // Post-OOBE

      test('StartSetupPage backward button closes UI (post-OOBE)', done => {
        multiDeviceSetupElement.addEventListener('setup-exited', () => done());

        multiDeviceSetupElement.visiblePageName_ = START;
        multiDeviceSetupElement.mojoService_.responseCode =
            multidevice_setup.SetBetterTogetherHostResponseCode.SUCCESS;
        multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

        backwardButton.click();
      });

      test(
          'StartSetupPage forward button goes to success page if mojo works ' +
              '(post-OOBE)',
          done => {
            multiDeviceSetupElement.addEventListener(
                'visible-page-name_-changed', () => {
                  if (multiDeviceSetupElement.$$('iron-pages > .iron-selected')
                          .is == SUCCESS)
                    done();
                });

            multiDeviceSetupElement.visiblePageName_ = START;
            multiDeviceSetupElement.mojoService_.responseCode =
                multidevice_setup.SetBetterTogetherHostResponseCode.SUCCESS;
            multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

            forwardButton.click();
          });

      test(
          'StartSetupPage forward button goes to failure page if mojo fails' +
              '(post-OOBE)',
          done => {
            multiDeviceSetupElement.addEventListener(
                'visible-page-name_-changed', () => {
                  if (multiDeviceSetupElement.$$('iron-pages > .iron-selected')
                          .is == FAILURE)
                    done();
                });

            multiDeviceSetupElement.visiblePageName_ = START;
            multiDeviceSetupElement.mojoService_.responseCode =
                multidevice_setup.SetBetterTogetherHostResponseCode
                    .ERROR_NETWORK_REQUEST_FAILED;
            multiDeviceSetupElement.uiMode = multidevice_setup.UiMode.POST_OOBE;

            forwardButton.click();
          });
    });
  }
  return {registerIntegrationTests: registerIntegrationTests};
});

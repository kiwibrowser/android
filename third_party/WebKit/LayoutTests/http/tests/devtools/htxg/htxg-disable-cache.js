// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('The \'disable cache\' flag must affect on the certificate fetch request.\n');

  const outerUrl =
      'http://localhost:8000/loading/htxg/resources/htxg-location.sxg';
  const certUrl =
      'http://localhost:8000/loading/htxg/resources/127.0.0.1.sxg.pem.cbor';
  const innerUrl = 'https://www.127.0.0.1/test.html';

  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.addScriptTag('/loading/htxg/resources/htxg-util.js');
  // The timestamp of the test SXG file is "Apr 1 2018 00:00 UTC" and valid
  // until "Apr 8 2018 00:00 UTC".
  await TestRunner.evaluateInPageAsync(
    'setSignedExchangeVerificationTime(new Date("Apr 1 2018 00:01 UTC"))');

  await TestRunner.NetworkAgent.setCacheDisabled(false);

  // Load the test signed exchange first, to cache the certificate file.
  await TestRunner.addIframe(outerUrl);

  BrowserSDK.networkLog.reset();

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.addIframe(outerUrl + '?iframe-1');
  await addPrefetchAndWait(outerUrl + '?prefetch-1', innerUrl);

  await TestRunner.NetworkAgent.setCacheDisabled(false);
  await TestRunner.addIframe(outerUrl + '?iframe-2');
  await addPrefetchAndWait(outerUrl + '?prefetch-2', innerUrl);

  for (var request of BrowserSDK.networkLog.requests()) {
    if (request.url() != certUrl)
      continue;
    TestRunner.addResult(`* ${request.url()}`);
    TestRunner.addResult(`  cached: ${request.cached()}`);
  }
  TestRunner.completeTest();

  async function addPrefetchAndWait(prefetchUrl, waitUrl) {
    const promise = new Promise(resolve => {
        TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
        function loadingFinished(requestId, finishTime, encodedDataLength) {
          var request = BrowserSDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);
          if (request.url() == waitUrl) {
            resolve();
          } else {
            TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
          }
        }
      });
    TestRunner.evaluateInPage(`
          (function () {
            const link = document.createElement('link');
            link.rel = 'prefetch';
            link.href = '${prefetchUrl}';
            document.body.appendChild(link);
          })()
        `);
    await promise;
  }
})();

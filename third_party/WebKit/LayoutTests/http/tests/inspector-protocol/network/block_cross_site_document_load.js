(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests the blockedCrossSiteDocumentLoad bit available as a part of the loading finished signal.`);

  const responses = new Map();
  await dp.Network.enable();

  let blocked_urls = [
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/nosniff.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/simple-iframe.html',
  ];
  for (const url of blocked_urls) {
    session.evaluate(`new Image().src = '${url}';`);
    const response = await dp.Network.onceLoadingFinished();
    testRunner.log(
        `Blocking cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${response.params.shouldReportCorbBlocking}.`);
  }

  let blocked_unreported_urls = [
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/204.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/404.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/content-length-0.pl',
  ];
  for (const url of blocked_unreported_urls) {
    session.evaluate(`new Image().src = '${url}';`);
    const response = await dp.Network.onceLoadingFinished();
    testRunner.log(
        `Blocking, but not reporting cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${response.params.shouldReportCorbBlocking}.`);
  }

  let allowed_urls = [
    'http://127.0.0.1:8000/inspector-protocol/network/resources/nosniff.pl',
    'http://127.0.0.1:8000/inspector-protocol/network/resources/simple-iframe.html',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/test.css',
  ]
  for (const url of allowed_urls) {
    session.evaluate(`new Image().src = '${url}';`);
    const response = await dp.Network.onceLoadingFinished();
    testRunner.log(
        `Allowing cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${response.params.shouldReportCorbBlocking}.`);
  }

  testRunner.completeTest();
})

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the we properly report url fragment in network requests`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Network.onRequestIntercepted(event => {
    const request = event.params.request;
    testRunner.log(`Intercepted URL: ${request.url} fragment: ${request.urlFragment}`);
    dp.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
  });
  dp.Network.onRequestWillBeSent(event => {
    const request = event.params.request;
    testRunner.log(`Request will be sent: ${request.url} fragment: ${request.urlFragment}`);
  });
  dp.Network.setRequestInterception({patterns: [{}]});
  const pageURL = '/inspector-protocol/network/resources/simple.html';
  await dp.Page.navigate({url: pageURL});
  await dp.Page.navigate({url: pageURL + '#ref'});
  const resourceURL = '/devtools/network/resources/resource.php';
  await session.evaluateAsync(`fetch("${resourceURL}")`);
  await session.evaluateAsync(`fetch("${resourceURL}#ref")`);
  await session.evaluateAsync(`fetch("${resourceURL}#")`);
  testRunner.completeTest();
})

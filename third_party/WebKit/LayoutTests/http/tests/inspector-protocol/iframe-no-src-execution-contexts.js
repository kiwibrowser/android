(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for iframes that don't have src attribute.`);
  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/iframe-no-src.html');
  dp.Runtime.onExecutionContextCreated(event => {
    testRunner.log(event);
  });
  await dp.Runtime.enable();
  testRunner.completeTest();
})

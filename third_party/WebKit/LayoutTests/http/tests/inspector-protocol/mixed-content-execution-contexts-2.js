(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for frames that were blocked due to mixed content when Runtime is enabled *after* navigation.`);
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content.html');
  dp.Runtime.onExecutionContextCreated(event => {
    testRunner.log(event);
  });
  await dp.Runtime.enable();
  testRunner.completeTest();
})

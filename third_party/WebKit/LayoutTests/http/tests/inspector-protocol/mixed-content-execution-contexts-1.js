(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for frames that were blocked due to mixed content when runtime is enabled *before* navigation.`);
  await dp.Runtime.enable();
  dp.Runtime.onExecutionContextCreated(event => {
    testRunner.log(event);
  });
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content.html');
  testRunner.completeTest();
})

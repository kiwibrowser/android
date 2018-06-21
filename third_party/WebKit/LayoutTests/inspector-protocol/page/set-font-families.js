(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    `<html>
      <style>
        @font-face {
            font-family: myahem;
            src: url(../../resources/Ahem.woff) format("woff");
        }
        div.standard_font {
        }
        div.sans_serif_font {
          font-family: sans-serif;
        }
        div.fixed_font {
          font-family: fixed;
        }
      </style>
      <body>
        <div class=standard_font>Standard</div>
        <div class=sans_serif_font>SansSerif</div>
        <div class=fixed_font>Fixed</div>
      </body>
    </html>`,
  'Tests Page.setFontFamilies.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function logPlatformFonts(selector) {
    testRunner.log(selector);
    const root = (await dp.DOM.getDocument()).result.root;
    const nodeId = (await dp.DOM.querySelector(
      {nodeId: root.nodeId, selector: selector})).result.nodeId;
    const fonts = (await dp.CSS.getPlatformFontsForNode(
      {nodeId: nodeId})).result.fonts;
    testRunner.log(fonts);
  }

  // Override generic fonts
  await dp.Page.setFontFamilies({fontFamilies: {
    standard: "Ahem",
    sansSerif: "Ahem",
    fixed: "Ahem"
  }});

  // Log overriden generic fonts
  await logPlatformFonts('.standard_font');
  await logPlatformFonts('.sans_serif_font');
  await logPlatformFonts('.fixed_font');

  testRunner.completeTest();
})

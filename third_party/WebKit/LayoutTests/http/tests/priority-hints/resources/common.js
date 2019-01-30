/**
 * These values map to some of the the current
 * priority enum members in blink::ResourceLoadPriority.
 * The values are exposed through window.internals
 * and in these tests, we use the below variables to represent
 * the exposed values in a readable way.
 */
const kLow = 1,
      kMedium = 2,
      kHigh = 3,
      kVeryHigh = 4;

function assert_priority_onload(url, expected_priority, test) {
  return test.step_func(e => {
    assert_equals(expected_priority, getPriority(url, document));
    test.done();
  });
}

function getPriority(url) {
  return internals.getResourcePriority(url, document);
}

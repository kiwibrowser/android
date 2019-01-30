// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-input', function() {
  let crInput;
  let input;

  setup(function() {
    PolymerTest.clearBody();
    crInput = document.createElement('cr-input');
    document.body.appendChild(crInput);
    input = crInput.$.input;
    Polymer.dom.flush();
  });

  test('AttributesCorrectlySupported', function() {
    const attributesToTest = [
      // [externalName, internalName, defaultValue, testValue]
      ['autofocus', 'autofocus', false, true],
      ['disabled', 'disabled', false, true],
      ['incremental', 'incremental', false, true],
      ['maxlength', 'maxLength', -1, 5],
      ['minlength', 'minLength', -1, 5],
      ['pattern', 'pattern', '', '[a-z]+'],
      ['readonly', 'readOnly', false, true],
      ['required', 'required', false, true],
      ['tab-index', 'tabIndex', 0, -1],
      ['type', 'type', 'text', 'password'],
    ];

    attributesToTest.forEach(attr => {
      assertEquals(attr[2], input[attr[1]]);
      crInput.setAttribute(attr[0], attr[3]);
      assertEquals(attr[3], input[attr[1]]);
    });
  });

  test('placeholderCorrectlyBound', function() {
    assertFalse(input.hasAttribute('placeholder'));

    crInput.placeholder = '';
    assertTrue(input.hasAttribute('placeholder'));

    crInput.placeholder = 'hello';
    assertEquals('hello', input.getAttribute('placeholder'));

    crInput.placeholder = null;
    assertFalse(input.hasAttribute('placeholder'));
  });

  test('labelHiddenWhenEmpty', function() {
    const label = crInput.$.label;
    assertEquals('none', getComputedStyle(crInput.$.label).display);
    crInput.label = 'foobar';
    assertEquals('block', getComputedStyle(crInput.$.label).display);
    assertEquals('foobar', label.textContent);
  });

  test('valueSetCorrectly', function() {
    crInput.value = 'hello';
    assertEquals(crInput.value, input.value);

    // |value| is copied when typing triggers inputEvent.
    input.value = 'hello sir';
    input.dispatchEvent(new InputEvent('input'));
    assertEquals(crInput.value, input.value);
  });

  test('focusState', function() {
    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;

    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);

    let whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    input.focus();
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 != underline.offsetWidth);
    });
  });

  test('invalidState', function() {
    crInput.errorMessage = 'error';
    const errorLabel = crInput.$.error;
    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;
    const originalLineColor = getComputedStyle(underline).borderBottomColor;

    assertEquals(crInput.errorMessage, errorLabel.textContent);
    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);
    assertEquals('hidden', getComputedStyle(errorLabel).visibility);

    let whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    crInput.invalid = true;
    Polymer.dom.flush();
    assertTrue(crInput.hasAttribute('invalid'));
    assertEquals('visible', getComputedStyle(errorLabel).visibility);
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    assertTrue(
        originalLineColor != getComputedStyle(underline).borderBottomColor);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 != underline.offsetWidth);
    });
  });

  test('validation', function() {
    crInput.value = 'FOO';
    crInput.autoValidate = true;
    assertFalse(crInput.hasAttribute('required'));
    assertFalse(crInput.invalid);

    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    crInput.setAttribute('required', '');
    assertFalse(crInput.invalid);

    crInput.value = '';
    assertTrue(crInput.invalid);
    crInput.value = 'BAR';
    assertFalse(crInput.invalid);

    const testPattern = '[a-z]+';
    crInput.setAttribute('pattern', testPattern);
    crInput.value = 'FOO';
    assertTrue(crInput.invalid);
    crInput.value = 'foo';
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = 'ALL CAPS';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });

  test('ariaLabelsCorrect', function() {
    assertFalse(!!crInput.inputElement.getAttribute('aria-label'));

    /**
     * This function assumes attributes are passed in priority order.
     * @param {!Array<string>} attributes
     */
    function testAriaLabel(attributes) {
      PolymerTest.clearBody();
      crInput = document.createElement('cr-input');
      attributes.forEach(attribute => {
        // Using their name as the value out of convenience.
        crInput.setAttribute(attribute, attribute);
      });
      document.body.appendChild(crInput);
      Polymer.dom.flush();
      // Assuming first attribute takes priority.
      assertEquals(
          attributes[0], crInput.inputElement.getAttribute('aria-label'));
    }

    testAriaLabel(['aria-label', 'label', 'placeholder']);
    testAriaLabel(['label', 'placeholder']);
    testAriaLabel(['placeholder']);
  });
});

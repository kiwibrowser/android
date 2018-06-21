'use strict';

function ongamepadconnected() {
  return new Promise(resolve => {
    window.addEventListener('gamepadconnected', resolve, { once: true });
  });
}

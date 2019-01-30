self.addEventListener('message', function(event) {
    event.waitUntil(self.clients.claim()
      .then(function(result) {
          if (result !== undefined) {
              event.data.port.postMessage(
                  'FAIL: claim() should be resolved with undefined');
              return;
          }
          event.data.port.postMessage('PASS');
        })
      .catch(function(error) {
          event.data.port.postMessage('FAIL: exception: ' + error.name);
        }));
  });

self.addEventListener('fetch', event => {
    if (event.request.url.indexOf('simple.txt') >= 0) {
      event.respondWith(new Response('sw controls the insecure page'));
    }
  });

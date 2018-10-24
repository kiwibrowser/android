const delta_for_scroll = 100;

function ensurePlatformAPIExists(input_src) {
  if (input_src === "touch" &&
     (!window.chrome || !window.chrome.gpuBenchmarking)) {
    throw "'gpuBenchmarking' needed for this test.";
  } else if (input_src === "wheel" && !window.eventSender) {
    throw "'eventSender' is needed for this test.";
  }
}

function getScrollDeltaFromDirection(direction) {
  let delta_x = (direction === "left") ? delta_for_scroll :
                (direction === "right") ? -delta_for_scroll : 0;
  let delta_y = (delta_x !== 0) ? 0 :
                (direction === "up") ? delta_for_scroll :
                (direction === "down") ? -delta_for_scroll : 0;
  if (delta_x === delta_y)
    throw `Invlaid direction ${direction}.`;
  return {x: delta_x, y: delta_y};
}

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // For now, we just rAF twice. It would be nice to have a proper mechanism
    // for this.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

function touchScrollGesture(touch_point, delta) {
  return new Promise((resolve) => {
            chrome.gpuBenchmarking.pointerActionSequence( [
                {source: "touch",
                 actions: [
                    { name: "pointerDown", x: touch_point.x, y: touch_point.y},
                    { name: "pointerMove",
                      x: (touch_point.x + delta.x),
                      y: (touch_point.y + delta.y)
                    },
                    { name: "pause", duration: 0.1 },
                    { name: "pointerUp" }
                ]}], resolve);
            });
}

async function touchScroll(direction, start_x, start_y) {
  ensurePlatformAPIExists("touch");
  let delta = getScrollDeltaFromDirection(direction);
  await waitForCompositorCommit();
  await touchScrollGesture({x: start_x, y: start_y}, delta);
  await waitForCompositorCommit();
}

function pinchZoomGesture(
  touch_point_1, touch_point_2, move_offset, offset_upper_bound) {
  return new Promise((resolve) => {
    var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
    var pointerAction1 = pointerActions[0];
    var pointerAction2 = pointerActions[1];
    pointerAction1.actions = [];
    pointerAction2.actions = [];
    pointerAction1.actions.push(
        {name: 'pointerDown', x: touch_point_1.x, y: touch_point_1.y});
    pointerAction2.actions.push(
        {name: 'pointerDown', x: touch_point_2.x, y: touch_point_2.y});
    for (var offset = move_offset; offset < offset_upper_bound; offset += move_offset) {
      pointerAction1.actions.push({
          name: 'pointerMove',
          x: (touch_point_1.x - offset),
          y: touch_point_1.y,
      });
      pointerAction2.actions.push({
          name: 'pointerMove',
          x: (touch_point_2.x + offset),
          y: touch_point_2.y,
      });
    }
    pointerAction1.actions.push({name: 'pointerUp'});
    pointerAction2.actions.push({name: 'pointerUp'});
    chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
  })
}

async function pinchZoom(direction, start_x_1, start_y_1, start_x_2, start_y_2) {
  ensurePlatformAPIExists("touch");
  let zoom_in = direction === "in";
  let delta = zoom_in ? -delta_for_scroll : delta_for_scroll;
  let move_offset = 10;
  let offset_upper_bound = 80;
  await waitForCompositorCommit();
  await pinchZoomGesture(
    {x: start_x_1, y: start_y_1},
    {x: start_x_2, y: start_y_2},
    move_offset,
    offset_upper_bound);
  await waitForCompositorCommit();
}

function wheelScroll(direction, start_x, start_y) {
  ensurePlatformAPIExists("wheel");
  let delta = getScrollDeltaFromDirection(direction);
  return new Promise((resolve) => {
          eventSender.mouseMoveTo(start_x, start_y);
          eventSender.mouseDown(0);
          eventSender.mouseUp(0);
          eventSender.mouseScrollBy(delta.x , delta.y);
          resolve();
  });
}

window.input_api_ready = true;
if (window.resolve_on_input_api_ready)
  window.resolve_on_input_api_ready();
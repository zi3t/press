import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { stripTypeScriptTypes } from 'node:module';
import { test } from 'node:test';

const moduleUrl = (source) => `data:text/javascript;base64,${Buffer.from(source).toString('base64')}`;
const motion = moduleUrl(stripTypeScriptTypes(await readFile(new URL('../runtime/clean-room/motion.ts', import.meta.url), 'utf8')));
const source = stripTypeScriptTypes(await readFile(new URL('../runtime/clean-room/volume-interaction.ts', import.meta.url), 'utf8'));
const { installCleanRoomVolumeInteraction } = await import(moduleUrl(source.replace('"./motion"', JSON.stringify(motion))));

class Figure extends EventTarget {
  captured = false;
  setPointerCapture() { this.captured = true; }
  hasPointerCapture() { return this.captured; }
  releasePointerCapture() {
    this.captured = false;
    pointer(this, 'lostpointercapture');
  }
}
function pointer(target, type, extra = {}) {
  const event = new Event(type, { cancelable: true });
  Object.assign(event, {
    pointerId: 1, pointerType: 'mouse', button: 0, buttons: 1,
    clientX: 200, clientY: 200,
    metaKey: false, ctrlKey: false, shiftKey: false, altKey: false,
    ...extra,
  });
  target.dispatchEvent(event);
  return event;
}
function setup() {
  globalThis.HTMLElement = Figure;
  globalThis.window = Object.assign(new EventTarget(), { innerWidth: 1200, innerHeight: 800 });
  const classes = new Set();
  globalThis.document = Object.assign(new EventTarget(), {
    visibilityState: 'visible',
    body: { classList: { add: x => classes.add(x), remove: x => classes.delete(x) } },
  });
  const figure = new Figure();
  const controller = installCleanRoomVolumeInteraction({
    figures: [figure], canInteract: () => true, flightActive: () => false, onWake() {},
  });
  pointer(figure, 'pointerdown');
  pointer(window, 'pointermove', { clientX: 350, clientY: 250 });
  return { figure, controller, classes };
}

test('capture loss does not reset or end a held drag; release outside coasts', () => {
  const { figure, controller, classes } = setup();
  const before = controller.snapshot();
  figure.releasePointerCapture();
  assert.deepEqual(controller.snapshot(), before);
  pointer(window, 'pointermove', { clientX: 1000, clientY: 500 });
  assert.notEqual(controller.snapshot().rotationY, before.rotationY);
  pointer(window, 'pointerup', { buttons: 0, clientX: 1000, clientY: 500 });
  const released = controller.snapshot();
  assert.equal(released.dragging, false);
  assert.notEqual(released.twirlY, 0);
  controller.advanceTwirl(1 / 60);
  assert.notEqual(controller.snapshot().rotationY, released.rotationY);
  assert.equal(classes.has('press-cover-dragging'), false);
});

test('pointer cancellation preserves the pose; route reset clears it', () => {
  const { controller } = setup();
  const before = controller.snapshot();
  pointer(window, 'pointercancel');
  assert.equal(controller.snapshot().rotationX, before.rotationX);
  assert.equal(controller.snapshot().rotationY, before.rotationY);
  assert.equal(controller.snapshot().dragging, false);
  controller.reset();
  assert.equal(controller.snapshot().rotationY, 0);
});

test('window blur preserves orientation and a new drag starts from it', () => {
  const { figure, controller } = setup();
  const before = controller.snapshot();
  window.dispatchEvent(new Event('blur'));
  assert.equal(controller.snapshot().rotationY, before.rotationY);
  pointer(figure, 'pointerdown', { clientX: 800 });
  pointer(window, 'pointermove', { clientX: 900 });
  assert.notEqual(controller.snapshot().rotationY, before.rotationY);
  assert.equal(controller.snapshot().dragging, true);
});

test('missed pointerup is recovered without resetting orientation', () => {
  const { controller } = setup();
  const before = controller.snapshot();
  pointer(window, 'pointermove', { buttons: 0, clientX: 350, clientY: 250 });
  assert.equal(controller.snapshot().dragging, false);
  assert.equal(controller.snapshot().rotationY, before.rotationY);
});

test('released book ignores cursor movement while coasting and after settling', () => {
  const { figure, controller } = setup();
  pointer(window, 'pointerup', { buttons: 0, clientX: 350, clientY: 250 });
  const released = controller.snapshot();
  pointer(window, 'pointermove', { buttons: 0, clientX: 1100, clientY: 700 });
  assert.deepEqual(controller.snapshot(), released);
  controller.advanceTwirl(1 / 60);
  assert.notEqual(controller.snapshot().rotationY, released.rotationY);
  for (let i = 0; i < 200; i++) controller.advanceTwirl(1 / 60);
  const settled = controller.snapshot();
  pointer(window, 'pointermove', { buttons: 0, clientX: 100, clientY: 100 });
  assert.deepEqual(controller.snapshot(), settled);
  pointer(figure, 'pointerdown', { clientX: 300, clientY: 300 });
  assert.equal(controller.snapshot().rotationY, settled.rotationY);
  pointer(window, 'pointermove', { clientX: 400, clientY: 300 });
  assert.notEqual(controller.snapshot().rotationY, settled.rotationY);
});

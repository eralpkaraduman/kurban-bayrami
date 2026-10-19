// Smoke test for the game page: renders shell.html.in, swaps the game for a
// stub that records the keys it receives, and drives the page in headless
// Chrome. Run: npm test (from this folder).
// Set CHROME_PATH if Chrome is not in the default macOS location.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { copyFileSync, readFileSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { extname, join } from 'node:path';
import puppeteer, { KnownDevices } from 'puppeteer-core';
import { render, tempDir, webDir } from './helpers.mjs';

const CHROME = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

// A stand-in for the Emscripten game: it sizes the canvas to the game's
// resolution, 360x120 unless ?game=WxH says otherwise, records every key the
// touch bar sends and every mouse press and release on the canvas, runs the
// page's postRun hook, and cancels touches and mouse wheel events on the
// canvas, as the real raylib game does.
const GAME_STUB = `<script>
  window.keys = [];
  window.mouse = [];
  Module._flx_touch_key = function (key, down) { window.keys.push(key + (down ? ' down' : ' up')); };
  ['mousedown', 'mouseup'].forEach(function (type) {
    canvas.addEventListener(type, function (e) { window.mouse.push(type + ' ' + e.clientX + ',' + e.clientY); });
  });
  var size = (new URLSearchParams(location.search).get('game') || '360x120').split('x');
  canvas.width = Number(size[0]);
  canvas.height = Number(size[1]);
  Module.postRun.forEach(function (run) { run(); });
  // Like raylib on the web, cancel every touch on the canvas.
  ['touchstart', 'touchmove', 'touchend'].forEach(function (type) {
    canvas.addEventListener(type, function (e) { e.preventDefault(); }, { passive: false });
  });
  // Like Emscripten's GLFW, cancel every wheel event on the canvas.
  canvas.addEventListener('wheel', function (e) { e.preventDefault(); }, { passive: false, capture: true });
</script>`;

let server, browser, baseUrl;

before(async () => {
  const dir = tempDir();
  const shell = render('shell.html.in', {
    WEB_TITLE: 'Test Game',
    WEB_TOUCH_DPAD: '{ up: 265, down: 264, left: 263, right: 262 }',
    WEB_TOUCH_BUTTONS: "[{ label: 'X', caption: 'Jump', key: 88 }, { label: 'C', caption: 'Carry', key: 67 }]",
  }, dir);
  writeFileSync(join(dir, 'index.html'), readFileSync(shell, 'utf8').replace('{{{ SCRIPT }}}', GAME_STUB));
  // A second page with only left and right, to check the one-row D-pad.
  const rowShell = render('shell.html.in', {
    WEB_TITLE: 'Row Game',
    WEB_TOUCH_DPAD: '{ left: 263, right: 262 }',
    WEB_TOUCH_BUTTONS: "[{ label: 'X', caption: 'Jump', key: 88 }]",
  }, tempDir());
  writeFileSync(join(dir, 'row.html'), readFileSync(rowShell, 'utf8').replace('{{{ SCRIPT }}}', GAME_STUB));
  // A 3:2 game with a thumb stick, three buttons, and taps as clicks, like Rockmorse.
  const tapShell = render('shell.html.in', {
    WEB_TITLE: 'Tap Game',
    WEB_TOUCH_DPAD: '{ up: 265, down: 264, left: 263, right: 262 }',
    WEB_TOUCH_BUTTONS: "[{ label: 'Space', caption: 'Pull', key: 32 }, { label: 'M', caption: 'Map', key: 77 }, { label: 'H', caption: 'Help', key: 72 }]",
    WEB_TOUCH_TAP_CLICKS: 'ON',
    WEB_TOUCH_STICK: 'ON',
  }, tempDir());
  writeFileSync(join(dir, 'tap.html'), readFileSync(tapShell, 'utf8').replace('{{{ SCRIPT }}}', GAME_STUB));
  // A parent page with the game in a frame and text below, like the cover page.
  writeFileSync(join(dir, 'embed.html'), `<!doctype html><meta name="viewport" content="width=device-width, initial-scale=1">
<body style="margin:0"><iframe src="index.html" style="display:block;width:100%;height:320px;border:0"></iframe>
<div style="height:2000px">Text below the game</div></body>`);
  copyFileSync(join(webDir, 'touch-controls.js'), join(dir, 'touch-controls.js'));
  copyFileSync(join(webDir, 'touch-controls.css'), join(dir, 'touch-controls.css'));

  const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
  server = createServer((req, res) => {
    const name = new URL(req.url, 'http://x').pathname.slice(1) || 'index.html';
    let body;
    try {
      body = readFileSync(join(dir, name));
    } catch {
      res.writeHead(404).end();
      return;
    }
    res.writeHead(200, { 'content-type': types[extname(name)] || 'text/plain' });
    res.end(body);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  baseUrl = `http://127.0.0.1:${server.address().port}/`;
  browser = await puppeteer.launch({ executablePath: CHROME, headless: true });
});

after(async () => {
  await browser?.close();
  server?.close();
});

async function openPage(path, device) {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  if (device) await page.emulate(device);
  else await page.setViewport({ width: 1000, height: 600, deviceScaleFactor: 1 });
  await page.goto(baseUrl + path, { waitUntil: 'load' });
  page.errors = errors;
  return page;
}

const canvasSize = page => page.evaluate(() => {
  const r = document.getElementById('canvas').getBoundingClientRect();
  return [Math.round(r.width), Math.round(r.height)];
});

// Real touch input through the Chrome DevTools protocol, so the bar sees the
// same pointer events a phone sends. Each point is { x, y, id }.
async function touchAt(page) {
  const cdp = await page.createCDPSession();
  const centre = async key => page.evaluate(k => {
    const r = document.querySelector(`[data-key="${k}"]`).getBoundingClientRect();
    return { x: r.x + r.width / 2, y: r.y + r.height / 2 };
  }, key);
  const send = (type, touchPoints) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints });
  return { centre, send };
}

test('keyboard mode: no control bar, and the game scales by a whole number', async () => {
  const page = await openPage('');
  assert.equal(await page.$('.tc-bar'), null);
  // 1000x600 window, 360x120 game: 2x is the largest whole number that fits.
  assert.deepEqual(await canvasSize(page), [720, 240]);
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('keyboard mode: below 2x the game fills the window instead', async () => {
  const page = await openPage('');
  await page.setViewport({ width: 600, height: 400, deviceScaleFactor: 1 });
  await page.evaluate(() => window.dispatchEvent(new Event('resize')));
  assert.deepEqual(await canvasSize(page), [600, 200]);
  await page.close();
});

test('touch mode: the bar shows the configured buttons, and the game fills the width', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  const labels = await page.$$eval('.tc-button', buttons => buttons.map(b => b.dataset.key + ' ' + b.textContent));
  assert.deepEqual(labels, ['265 ▲up', '263 ◀left', '262 ▶right', '264 ▼down', '88 XJump', '67 CCarry']);
  assert.deepEqual(await canvasSize(page), [390, 130]);
  assert.equal(await page.evaluate(() => document.documentElement.classList.contains('tc-mobile')), true);
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('touch mode, landscape: a 3:2 game fills the screen height, with controls over it', async () => {
  const page = await openPage('row.html?game=240x160', KnownDevices['iPhone 13 landscape']);
  assert.equal(await page.evaluate(() => document.documentElement.classList.contains('tc-overlay')), true);
  // An 844x390 screen: the game takes the full 390 px height.
  assert.deepEqual(await canvasSize(page), [585, 390]);
  // The buttons sit on top of the game and still work, two at once.
  const { centre, send } = await touchAt(page);
  await send('touchStart', [{ ...(await centre(263)), id: 1 }, { ...(await centre(88)), id: 2 }]);
  await send('touchEnd', []);
  assert.deepEqual(await page.evaluate(() => window.keys), ['263 down', '88 down', '263 up', '88 up']);
  // A touch on the game away from the buttons sends no key.
  await send('touchStart', [{ x: 422, y: 100, id: 1 }]);
  await send('touchEnd', []);
  assert.equal(await page.evaluate(() => window.keys.length), 4);
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('touch mode, landscape: a 3:1 game fills the screen width, with controls over it', async () => {
  const page = await openPage('', KnownDevices['iPhone 13 landscape']);
  assert.equal(await page.evaluate(() => document.documentElement.classList.contains('tc-overlay')), true);
  assert.deepEqual(await canvasSize(page), [844, 281]);
  await page.close();
});

test('touch mode, portrait: the bar stays under the game, not over it', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  assert.equal(await page.evaluate(() => document.documentElement.classList.contains('tc-overlay')), false);
  const gameBottom = await page.$eval('canvas', c => c.getBoundingClientRect().bottom);
  const barTop = await page.$eval('.tc-bar', b => b.getBoundingClientRect().top);
  assert.ok(barTop >= gameBottom - 0.5, 'the bar starts below the game');
  await page.close();
});

test('touch mode: a left and right D-pad becomes one row', async () => {
  const page = await openPage('row.html', KnownDevices['iPhone 13']);
  assert.equal(await page.$$eval('.tc-dpad .tc-button', b => b.length), 2);
  assert.equal(await page.$eval('.tc-dpad', d => d.classList.contains('tc-dpad-row')), true);
  await page.close();
});

test('touch mode: presses, two fingers at once, and sliding send the right keys', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  const { centre, send } = await touchAt(page);
  const x = await centre(88), left = await centre(263), right = await centre(262);

  await send('touchStart', [{ ...x, id: 1 }]);
  await send('touchEnd', []);
  await send('touchStart', [{ ...right, id: 1 }]);
  await send('touchStart', [{ ...right, id: 1 }, { ...x, id: 2 }]);
  await send('touchEnd', []);
  await send('touchStart', [{ ...left, id: 1 }]);
  await send('touchMove', [{ ...right, id: 1 }]);
  await send('touchEnd', []);

  assert.deepEqual(await page.evaluate(() => window.keys), [
    '88 down', '88 up',
    '262 down', '88 down', '262 up', '88 up',
    '263 down', '263 up', '262 down', '262 up',
  ]);
  assert.equal(await page.$$eval('.tc-pressed', b => b.length), 0);
  await page.close();
});

test('touch mode: a held key is released when the page loses focus', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  const { centre, send } = await touchAt(page);
  await send('touchStart', [{ ...(await centre(67)), id: 1 }]);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.deepEqual(await page.evaluate(() => window.keys), ['67 down', '67 up']);
  await page.close();
});

test('touch mode: touch starts are cancelled, so iOS cannot swipe the page away', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  // Listen last, after the page's own handlers, and record what they did.
  await page.evaluate(() => {
    window.touchStarts = [];
    window.addEventListener('touchstart', e => window.touchStarts.push(e.defaultPrevented));
  });
  const { centre, send } = await touchAt(page);
  await send('touchStart', [{ x: 2, y: 400, id: 1 }]);            // at the left edge, outside the buttons
  await send('touchEnd', []);
  await send('touchStart', [{ ...(await centre(88)), id: 1 }]);   // on a button
  await send('touchEnd', []);
  assert.deepEqual(await page.evaluate(() => window.touchStarts), [true, true]);
  // The button still works, since the bar listens to pointer events.
  assert.deepEqual(await page.evaluate(() => window.keys), ['88 down', '88 up']);
  await page.close();
});

test('touch mode, embedded: a swipe over the game scrolls the outer page, but not over the bar', async () => {
  const page = await openPage('embed.html', KnownDevices['iPhone 13']);
  await new Promise(r => setTimeout(r, 500));
  const frame = await (await page.$('iframe')).contentFrame();
  assert.equal(await frame.evaluate(() => document.documentElement.classList.contains('tc-embedded')), true);
  const cdp = await page.createCDPSession();
  const drag = async (x, fromY, toY) => {
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y: fromY, id: 1 }] });
    for (let i = 1; i <= 10; i++) {
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x, y: fromY + (toY - fromY) * i / 10, id: 1 }] });
    }
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await new Promise(r => setTimeout(r, 400));
  };
  // The game sits at the top of the frame, 130 px tall. The bar is below it.
  await drag(200, 100, 20);
  assert.ok(await page.evaluate(() => window.scrollY) > 20, 'swipe over the game scrolled the page');
  await page.evaluate(() => window.scrollTo(0, 0));
  const barTop = await frame.$eval('.tc-bar', b => b.getBoundingClientRect().top);
  await drag(200, barTop + 150, barTop + 60);
  assert.equal(await page.evaluate(() => window.scrollY), 0, 'swipe over the bar did not scroll');
  await page.close();
});

test('touch mode: double-tap does not zoom the page', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  const { send } = await touchAt(page);
  for (let i = 0; i < 2; i++) {
    await send('touchStart', [{ x: 200, y: 60, id: 1 }]);
    await send('touchEnd', []);
  }
  await new Promise(r => setTimeout(r, 400));
  assert.equal(await page.evaluate(() => visualViewport.scale), 1);
  await page.close();
});

test('touch mode: the thumb stick sends one or two arrow keys, and a button works beside it', async () => {
  const page = await openPage('tap.html?game=660x440', KnownDevices['iPhone 13']);
  assert.equal(await page.$$eval('.tc-dpad .tc-button', b => b.length), 0);
  const { centre, send } = await touchAt(page);
  const stick = await page.$eval('.tc-stick', d => {
    const r = d.getBoundingClientRect();
    return { x: r.x + r.width / 2, y: r.y + r.height / 2, r: r.width / 2 };
  });
  const at = (dx, dy) => ({ x: stick.x + dx * stick.r, y: stick.y + dy * stick.r, id: 1 });
  const space = await centre(32);
  const log = () => page.evaluate(() => window.keys.splice(0));

  await send('touchStart', [at(0.1, 0)]);            // inside the dead zone
  assert.deepEqual(await log(), []);
  await send('touchMove', [at(0.8, 0)]);             // right
  assert.deepEqual(await log(), ['262 down']);
  await send('touchMove', [at(0.6, -0.6)]);          // up and right
  assert.deepEqual(await log(), ['265 down']);
  await send('touchMove', [at(0.8, -0.2)]);          // 14 degrees up: right only
  assert.deepEqual(await log(), ['265 up']);
  await send('touchMove', [at(-2, -2)]);             // far off the base, up and left
  assert.deepEqual(await log(), ['262 up', '263 down', '265 down']);
  const [kx, ky] = await page.$eval('.tc-knob', k => {
    const m = new DOMMatrix(getComputedStyle(k).transform);
    return [m.e, m.f];
  });
  assert.ok(kx < 0 && ky < 0 && Math.hypot(kx, ky) < stick.r, `the knob is at ${kx}, ${ky}, on the base`);
  await send('touchStart', [at(-2, -2), { ...space, id: 2 }]);
  assert.deepEqual(await log(), ['32 down']);
  await send('touchEnd', []);
  assert.deepEqual((await log()).sort(), ['263 up', '265 up', '32 up']);
  assert.equal(await page.$eval('.tc-knob', k => k.style.transform), '');
  assert.equal(await page.$$eval('.tc-pressed', b => b.length), 0);
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('touch mode: three buttons and a full D-pad fit on a 360 px wide phone', async () => {
  for (const width of [360, 390]) {
    const page = await openPage('tap.html?game=660x440', { ...KnownDevices['iPhone 13'], viewport: { ...KnownDevices['iPhone 13'].viewport, width } });
    const right = await page.$$eval('.tc-button', b => Math.max(...b.map(e => e.getBoundingClientRect().right)));
    assert.ok(right <= width, `the last button ends at ${right}, inside ${width}`);
    assert.equal(await page.$eval('.tc-actions', a => a.classList.contains('tc-actions-many')), true);
    await page.close();
  }
  // Two buttons keep their size.
  const page = await openPage('', KnownDevices['iPhone 13']);
  assert.equal(await page.$eval('.tc-actions', a => a.classList.contains('tc-actions-many')), false);
  await page.close();
});

test('touch mode: with tapClicks, a tap on the game is a mouse click, a swipe is not', async () => {
  const page = await openPage('tap.html?game=660x440', KnownDevices['iPhone 13']);
  const { centre, send } = await touchAt(page);
  await send('touchStart', [{ x: 200, y: 100, id: 1 }]);
  await send('touchEnd', []);
  await new Promise(r => setTimeout(r, 250));
  assert.deepEqual(await page.evaluate(() => window.mouse), ['mousedown 200,100', 'mouseup 200,100']);
  // A swipe over the game, a touch on a button and a long hold send no click.
  await send('touchStart', [{ x: 200, y: 100, id: 1 }]);
  await send('touchMove', [{ x: 200, y: 160, id: 1 }]);
  await send('touchEnd', []);
  await send('touchStart', [{ ...(await centre(32)), id: 1 }]);
  await send('touchEnd', []);
  await send('touchStart', [{ x: 200, y: 100, id: 1 }]);
  await new Promise(r => setTimeout(r, 700));
  await send('touchEnd', []);
  await new Promise(r => setTimeout(r, 250));
  assert.equal(await page.evaluate(() => window.mouse.length), 2);
  assert.deepEqual(await page.evaluate(() => window.keys), ['32 down', '32 up']);
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('touch mode: without tapClicks, a tap on the game sends no click', async () => {
  const page = await openPage('', KnownDevices['iPhone 13']);
  const { send } = await touchAt(page);
  await send('touchStart', [{ x: 200, y: 60, id: 1 }]);
  await send('touchEnd', []);
  await new Promise(r => setTimeout(r, 250));
  assert.deepEqual(await page.evaluate(() => window.mouse), []);
  await page.close();
});

test('keyboard mode, embedded: the mouse wheel over the game scrolls the outer page', async () => {
  const page = await openPage('embed.html');
  await page.mouse.move(300, 150);
  for (let i = 0; i < 3; i++) await page.mouse.wheel({ deltaY: 200 });
  await page.waitForFunction(() => window.scrollY > 0, { timeout: 2000 });
  assert.deepEqual(page.errors, []);
  await page.close();
});

test('the loading bar shows the download, and hides once the game runs', async () => {
  const page = await openPage('');
  // The stub has run postRun, as a started game does.
  assert.equal(await page.$eval('#loading', el => el.hidden), true);
  await page.evaluate(() => Module.setStatus('Downloading data... (1048576/4194304)'));
  assert.equal(await page.$eval('#loading-text', el => el.textContent), 'Loading 1.0 / 4.0 MB');
  assert.equal(await page.$eval('#loading-bar', el => el.style.width), '25%');
  assert.equal(await page.$eval('#loading', el => el.getAttribute('aria-valuenow')), '25');
  assert.deepEqual(page.errors, []);
  await page.close();
});

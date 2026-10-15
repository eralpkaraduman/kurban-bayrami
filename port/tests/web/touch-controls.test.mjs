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
// 360x120 resolution, records every key the touch bar sends, and runs the
// page's postRun hook, as the real game does once it starts.
const GAME_STUB = `<script>
  window.keys = [];
  Module._flx_touch_key = function (key, down) { window.keys.push(key + (down ? ' down' : ' up')); };
  canvas.width = 360;
  canvas.height = 120;
  Module.postRun.forEach(function (run) { run(); });
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

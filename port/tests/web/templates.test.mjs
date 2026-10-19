// Checks that both web templates take their values from CMake and output
// them in the right places. Run: npm test (from this folder).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { render, tempDir, leftoverPlaceholder } from './helpers.mjs';

test('game page template fills in the title and touch keys', () => {
  const out = render('shell.html.in', {
    WEB_TITLE: 'Test Game: The Title',
    WEB_TOUCH_DPAD: '{ left: 263, right: 262 }',
    WEB_TOUCH_BUTTONS: "[{ label: 'X', caption: 'Jump', key: 88 }]",
  }, tempDir());
  const html = readFileSync(out, 'utf8');

  assert.match(html, /<title>Test Game: The Title<\/title>/);
  assert.match(html, /dpad: \{ left: 263, right: 262 \},/);
  assert.match(html, /buttons: \[\{ label: 'X', caption: 'Jump', key: 88 \}\],/);
  // Taps as clicks stay off unless a game asks for them.
  assert.match(html, /tapClicks: '' === 'ON',/);
  assert.doesNotMatch(html, leftoverPlaceholder);
  // Emscripten fills in its own marker later. CMake must leave it alone.
  assert.match(html, /\{\{\{ SCRIPT \}\}\}/);
  // The explanatory comment survives, so it holds no accidental placeholder.
  assert.match(html, /a name between two at signs/);
});

test('cover page template fills in every value and the page text', () => {
  const content = '<h2>About</h2>\n<p id="sentinel">Text from site-content.html</p>\n';
  const out = render('site.html.in', {
    WEB_TITLE: 'Test Game',
    SITE_TAGLINE: 'A one-line tagline.',
    SITE_URL: 'https://example.github.io/test-game/',
    SITE_THEME: '--bg: #010203; --accent: #abcdef;',
    SITE_GAME_RATIO: '3 / 2',
    SITE_TOUCH_FRAME_HEIGHT: 'calc(100vw * 2 / 3 + 150px)',
    SITE_HINT_KEYBOARD: 'Press <kbd>X</kbd> on a keyboard.',
    SITE_HINT_TOUCH: 'Tap <b>X</b> on a phone.',
  }, tempDir(), content);
  const html = readFileSync(out, 'utf8');

  assert.match(html, /<title>Test Game<\/title>/);
  assert.match(html, /<h1 class="pixel">Test Game<\/h1>/);
  assert.match(html, /<meta name="description" content="A one-line tagline\.">/);
  assert.match(html, /<meta property="og:image" content="https:\/\/example\.github\.io\/test-game\/cover\.png">/);
  assert.match(html, /:root \{ --bg: #010203; --accent: #abcdef; \}/);
  assert.match(html, /aspect-ratio: 3 \/ 2;/);
  assert.match(html, /height: calc\(100vw \* 2 \/ 3 \+ 150px\);/);
  assert.match(html, /<span class="hint-keyboard">Press <kbd>X<\/kbd> on a keyboard\.<\/span>/);
  assert.match(html, /<span class="hint-touch">Tap <b>X<\/b> on a phone\.<\/span>/);
  assert.match(html, /<article>\s*<h2>About<\/h2>\s*<p id="sentinel">Text from site-content\.html<\/p>\s*<\/article>/);
  assert.match(html, /<iframe src="play\/" title="Test Game, playable game">/);
  assert.doesNotMatch(html, leftoverPlaceholder);
  assert.match(html, /a name between two at signs/);
});

test('a value nobody sets becomes empty text, without any error', () => {
  // configure_file silently replaces a placeholder that has no value with
  // nothing. A misspelled name in a template or a CMakeLists.txt fails the
  // same way. That is why the tests above check for each filled-in value,
  // not only for the absence of leftover placeholders.
  const out = render('shell.html.in', { WEB_TITLE: 'Only a title' }, tempDir());
  const html = readFileSync(out, 'utf8');
  assert.match(html, /dpad: ,/);
});

test('a game can turn taps into clicks', () => {
  const out = render('shell.html.in', { WEB_TITLE: 'Tap', WEB_TOUCH_TAP_CLICKS: 'ON' }, tempDir());
  assert.match(readFileSync(out, 'utf8'), /tapClicks: 'ON' === 'ON',/);
});

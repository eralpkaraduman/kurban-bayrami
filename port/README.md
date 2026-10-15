# Kurban Bayrami: The Gathering (C port)

A C rewrite of the 2011 Flash game, built on raylib 6.0.
CMake downloads raylib on the first configure, checks it against a pinned SHA-256 hash, and links it statically. Every build is self-contained, with no raylib library to ship beside it.

Bring sheep to the pit in the leftmost screen.
Arrow keys move, X jumps, and holding C near a sheep carries it.

## Layout

- `src/flx.c`, `src/flx.h`: a small Flixel-style layer with sprites, motion, animation, tilemaps and collision.
- `src/actors.c`: the player (Haci) and the sheep (Koyun).
- `src/game.c`: the four screens, ported from the original FlxStates.
- `src/main.c`: window, fixed 60 Hz loop, and pixel-perfect scaling.
- `tests/check.c`: logic checks that run without a window.
- `assets/`: sprites, tiles and Tiled maps taken from the original SWF.
- `web/shell.html.in`: the web page around the game. CMake fills in the title and the touch keys.
- `web/touch-controls.js`, `web/touch-controls.css`: on-screen controls for touch screens, and the canvas scaling.
- `web/site.html.in`, `web/site-content.html`: the GitHub Pages cover page. The template is shared, and the content file holds this game's text.

## Web page and touch controls

On a touch screen, a control bar sits under the game: a D-pad, X to jump and C to carry.
The game fills the screen width, and the page never zooms or scrolls.
With a keyboard, the bar stays hidden. Add `?touch=1` to the address to test the touch layout on a desktop.

The bar calls `flx_touch_key()` in `src/flx.c`, which feeds the same input as the keyboard.

Both pages are templates. CMake fills in each placeholder, a name between two at signs, from variables set in `CMakeLists.txt`: the title, tagline, colours, game shape and touch keys. The cover page's text comes from `web/site-content.html`.

Time Escape uses the same templates and controls. On every configure, its CMake file downloads `shell.html.in`, `site.html.in` and the two `touch-controls` files from this repo's `main` branch.
A change pushed here reaches Time Escape on its next configure, so test both games after changing these files.

## Build

Desktop (macOS, Windows, Linux) needs CMake 3.24 or newer, a C compiler, and network access on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/kurban
```

On Linux, install the X11 and OpenGL headers first.
On Debian or Ubuntu that is `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

Web needs Emscripten.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server 8765 --bind 127.0.0.1 --directory build-web
```

Then open http://127.0.0.1:8765/.

## Publish on itch.io

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target itch
```

Upload `build-web/kurban-itch.zip`. On the itch.io project page, use these settings:

- Kind of project: HTML.
- Tick "This file will be played in the browser".
- Viewport dimensions: 720 x 240.
- Leave "Mobile friendly" unticked, since the game needs a keyboard.

## Publish on GitHub Pages

The site is live at https://eralpkaraduman.github.io/kurban-bayrami/.
It has a cover page at the root, from `web/site.html`, and the game itself under `play/`.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target pages
```

This assembles the whole site in `build-web/pages/`.
Commit that folder's contents to the `gh-pages` branch and push it. GitHub serves that branch.

Logic checks run from this folder:

```sh
./build/kurban_check
```

## Web frame tests

`tests/web/` tests the shared web files in `web/`. They need CMake, Node.js 18 or newer, and Google Chrome.

```sh
cd tests/web
npm install
npm test
```

- `templates.test.mjs` fills both templates through CMake's `configure_file`, as the game builds do, and checks that every value lands in the right place.
- `touch-controls.test.mjs` renders the game page with a stub in place of the game, then drives it in headless Chrome. It checks keyboard and touch layouts, the configured buttons, presses, two fingers at once, sliding between buttons, release on focus loss, and that double-tap does not zoom.

Set `CHROME_PATH` if Chrome is not in the default macOS location.
Run these tests after changing `web/`, since Time Escape takes these files from this repo's `main` branch.

## Code checks

These tools are deterministic: the same code gives the same result every run.

Static analysis reads the code without running it.
Run it after a desktop configure, which downloads the raylib headers into `build/`.

```sh
cppcheck --enable=warning,performance,portability,style --std=c99 \
    -D__STDC__=1 -D__STDC_VERSION__=199901L --quiet \
    -I src -I build/_deps/raylib-src/src --suppress=missingIncludeSystem src tests
for f in src/*.c tests/*.c; do clang --analyze -I src -I build/_deps/raylib-src/src "$f" -o /dev/null; done
```

AddressSanitizer catches bad memory access, and UndefinedBehaviorSanitizer catches undefined behaviour.
Both run the logic checks with extra checking compiled in.

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target kurban_check
./build-asan/kurban_check
```

On macOS, the built-in `leaks` tool finds memory that was never freed.
On Linux, AddressSanitizer reports leaks itself.

```sh
leaks --atExit -- ./build/kurban_check
```

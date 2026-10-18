# Kurban Bayrami: The Gathering

A small Flash platformer, made from November 2010 to August 2011, my warm-up game for Ludum Dare 21.
Carry sheep from the fields, through town, to the pit. Bring as many as you like.

The early source code survives in the [godstrokegames repo](https://github.com/eralpkaraduman/godstrokegames/tree/master/KurbanBayrami). The source of the final August 2011 build was not kept. In October 2026 the game was rebuilt in C with raylib, from a decompile of that build's SWF.

## Play it

- [Game page](https://eralpkaraduman.github.io/kurban-bayrami/), with the description, controls and screenshots.
- [Game only](https://eralpkaraduman.github.io/kurban-bayrami/play/), full window.

It runs in the browser on desktop with a keyboard or gamepad, and on phones and tablets with on-screen controls.

## What is here

- `KurbanBayrami.swf`, `index.html`, `js/`, `expressInstall.swf`: the original 2011 Flash build. It was made with ActionScript 3 and Flixel 2.35.
- `cityMap.tmx`, `ruralMap.tmx`, `rageMap.tmx`, `cityMap2.tmx`: the original Tiled maps, which the Flash build loaded at runtime.
- `port/`: the C port. See `port/README.md` to build it for web, macOS, Windows and Linux.
- `port/ITCH.txt`, `port/kurban-cover-630x500.png`, `port/screenshots/`: the itch.io page text and images.
- `dist/`: ready-made builds of the port.

## Builds in dist/

- `kurban-web-itch.zip`: the web version. Upload it to itch.io as an HTML game with a 720 x 240 viewport.
- `kurban-macos-arm64.zip`: for Macs with Apple silicon. The build is unsigned, so the first time, right-click `kurban` and choose Open.

## Controls

Arrow keys walk and climb. X jumps. Press C near a sheep to pick it up, and press C again to throw it.

On a gamepad, the D-pad or left stick is the arrows. The bottom face button is X, and the left face button is C.

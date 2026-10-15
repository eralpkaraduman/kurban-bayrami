// flx: a tiny Flixel-style layer on top of raylib.
// It mirrors the parts of Flixel 2.x that Kurban Bayrami uses:
// sprites with Flixel's motion model, frame animations, a TMX-loaded tilemap,
// and box collision. Names follow Flixel so the original code maps across.
#ifndef FLX_H
#define FLX_H

#include <stdbool.h>
#include <stddef.h>
#include "raylib.h"

// The game renders at 360x120 and shows it at 2x zoom, like the original.
#define FLX_WIDTH 360
#define FLX_HEIGHT 120
#define FLX_ZOOM 2
#define FLX_FPS 60
#define FLX_ELAPSED (1.0f / FLX_FPS)

// Flixel treats 10000 as "no speed limit".
#define FLX_NO_LIMIT 10000.0f

#define FLX_MAX_ANIMS 8
#define FLX_MAX_ANIM_FRAMES 8

typedef enum { FLX_RIGHT, FLX_LEFT } FlxFacing;

typedef struct {
    const char *name;
    int frames[FLX_MAX_ANIM_FRAMES];
    int frameCount;
    float fps;
    bool looped;
} FlxAnim;

typedef struct {
    float x, y, width, height;
    Vector2 velocity, acceleration, drag, maxVelocity;
    float angle;      // degrees, clockwise, around the sprite center
    Vector2 scale;    // a negative y draws the sprite upside down
    FlxFacing facing; // FLX_LEFT draws the graphic mirrored
    bool onFloor;     // set by collision, cleared at the start of each move

    // Position at the start of the frame. Collision uses it to find
    // which side an object came from.
    float lastX, lastY;

    Texture2D texture;
    int frameWidth, frameHeight;
    int frame;

    FlxAnim anims[FLX_MAX_ANIMS];
    int animCount;
    int currentAnim; // -1 when no animation has played yet
    int animIndex;
    float animTimer;
} FlxSprite;

typedef struct {
    int widthInTiles, heightInTiles;
    int *data; // tile index per cell; 0 is empty and not solid
    Texture2D tiles;
} FlxTilemap;

void flx_sprite_init(FlxSprite *s, float x, float y, Texture2D texture, int frameWidth, int frameHeight);
void flx_sprite_add_animation(FlxSprite *s, const char *name, const int *frames, int frameCount, float fps, bool looped);
void flx_sprite_play(FlxSprite *s, const char *name);
void flx_sprite_save_position(FlxSprite *s);
void flx_sprite_update(FlxSprite *s);
void flx_sprite_draw(const FlxSprite *s);
bool flx_sprite_on_screen(const FlxSprite *s);
bool flx_overlaps(const FlxSprite *a, const FlxSprite *b);

// Pushes two movable sprites apart. The upper one lands on the lower one.
void flx_collide_sprites(FlxSprite *a, FlxSprite *b);

// Loads one layer from TMX text. Tile values follow the original loader:
// value = gid - 1, and a value of 0 is neither drawn nor solid.
bool flx_tilemap_load(FlxTilemap *map, const char *tmx, const char *layerName, Texture2D tiles);
void flx_tilemap_unload(FlxTilemap *map);
void flx_tilemap_draw(const FlxTilemap *map);
void flx_tilemap_collide(const FlxTilemap *map, FlxSprite *s);

// Finds a named object in TMX text. Returns false when it is missing.
bool flx_tmx_object(const char *tmx, const char *name, Rectangle *out);

float flx_random(void); // 0 <= n < 1, like Math.random()

// Key presses and releases are latched per rendered frame, then consumed by
// the next fixed 60 Hz update. A press is never lost or seen twice, even on
// 120 Hz screens. A key counts as down if the keyboard or a touch button
// holds it, so game code reads one input whatever the device.
void flx_input_poll(void);
void flx_input_clear(void);
bool flx_key_down(int key);
bool flx_key_pressed(int key);
bool flx_key_released(int key);

// Called by the on-screen touch controls on the web (web/touch-controls.js)
// with a raylib key code, when a button goes down or up.
void flx_touch_key(int key, bool down);

#endif

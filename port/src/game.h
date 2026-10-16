// Kurban Bayrami: The Gathering. Actors and shared game state.
// Names follow the original ActionScript: Haci is the player,
// Koyun is a sheep, and the original Kurbanlik base class is folded into Koyun.
#ifndef GAME_H
#define GAME_H

#include "flx.h"

#define MAX_KOYUN 256

// Flixel draws its "system" font at 8 px per em. raylib sizes a font by
// ascent plus descent instead, which is 11 px for this font. At 11, one
// font pixel lands on exactly one screen pixel.
#define FONT_SIZE 11

typedef struct Koyun Koyun;

typedef struct {
    FlxSprite sprite;
    Koyun *caughtSomething;
    bool hasSheep; // set every frame by a carried sheep, picks the walk graphic
    bool canClimb;
    bool canAirJump;
    bool airJumping;
} Haci;

struct Koyun {
    FlxSprite sprite;
    Haci *haci;
    bool isCaught;
    bool justReleased;
    bool thrown; // in the air after a throw, until it lands
    bool markCollected; // already counted in the pit
    bool canJump;
    bool movingLeft;
    int stuckCounter;
    Vector2 lastPosition;
};

typedef enum { HACI_ON_MAP, HACI_LEFT_RIGHT_EDGE, HACI_LEFT_LEFT_EDGE } HaciExit;

typedef struct {
    Texture2D haci, koyun, tiles, title;
    Font font;
} Assets;

extern Assets assets;

void haci_init(Haci *h, float x, float y);
HaciExit haci_update(Haci *h);
void haci_set_can_climb(Haci *h, bool value);

void koyun_init(Koyun *k, float x, float y);
void koyun_update(Koyun *k);
void koyun_caught(Koyun *k, Haci *haci);
void koyun_released(Koyun *k, Haci *haci);
void koyun_thrown(Koyun *k, Haci *haci);

void game_init(void);
void game_update(void);
void game_draw(void);

#endif

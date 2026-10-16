#include "game.h"

#include <math.h>

// --- Haci (the player) ---------------------------------------------------

#define HACI_SPEED_X 15.0f   // added to velocity every frame, not per second
#define HACI_CLIMB_SPEED 0.8f
#define HACI_WALK_FPS 4
#define HACI_FLIP_SPEED 5    // degrees per frame during a backflip

void haci_init(Haci *h, float x, float y)
{
    *h = (Haci){0};
    FlxSprite *s = &h->sprite;
    flx_sprite_init(s, x, y, assets.haci, 8, 12);

    flx_sprite_add_animation(s, "idle", (int[]){0}, 1, 0, false);
    flx_sprite_add_animation(s, "jump", (int[]){1}, 1, 0, false);
    flx_sprite_add_animation(s, "walk", (int[]){2, 3}, 2, HACI_WALK_FPS, true);
    flx_sprite_add_animation(s, "idle_sheep", (int[]){4}, 1, 0, false);
    flx_sprite_add_animation(s, "walk_sheep", (int[]){5, 6}, 2, HACI_WALK_FPS, true);

    s->maxVelocity = (Vector2){120, 140};
    s->acceleration.y = 400;
    s->drag.x = s->maxVelocity.x * 4;
}

// Matches the original setter: gravity drops to 300 after leaving a ladder.
void haci_set_can_climb(Haci *h, bool value)
{
    if (value) {
        h->sprite.acceleration.y = 0;
        h->sprite.velocity.y = 0;
        h->airJumping = false;
        h->canAirJump = false;
    } else {
        h->sprite.acceleration.y = 300;
    }
    h->canClimb = value;
}

static void haci_jump(Haci *h)
{
    h->sprite.velocity.y = -h->sprite.acceleration.y * 0.51f;
    flx_sprite_play(&h->sprite, "jump");
}

HaciExit haci_update(Haci *h)
{
    FlxSprite *s = &h->sprite;
    flx_sprite_save_position(s);
    s->acceleration.x = 0;

    if (flx_key_down(KEY_LEFT)) {
        s->velocity.x -= HACI_SPEED_X;
        s->facing = FLX_LEFT;
    } else if (flx_key_down(KEY_RIGHT)) {
        s->velocity.x += HACI_SPEED_X;
        s->facing = FLX_RIGHT;
    }

    if (flx_key_down(KEY_UP) && h->canClimb) {
        h->airJumping = false;
        s->y -= HACI_CLIMB_SPEED;
    } else if (flx_key_down(KEY_DOWN) && h->canClimb) {
        h->airJumping = false;
        s->y += HACI_CLIMB_SPEED;
    }

    if (h->airJumping) {
        s->angle += (s->facing == FLX_LEFT ? 1 : -1) * HACI_FLIP_SPEED;
    } else {
        s->angle = 0;
    }

    bool jumpPressed = flx_key_pressed(KEY_X);
    if (s->onFloor) {
        if (jumpPressed) {
            haci_jump(h);
            h->canAirJump = true;
        } else if (s->velocity.x != 0) {
            flx_sprite_play(s, h->hasSheep ? "walk_sheep" : "walk");
        } else {
            flx_sprite_play(s, h->hasSheep ? "idle_sheep" : "idle");
        }
        h->hasSheep = false;
        h->airJumping = false;
    } else if (s->velocity.y < 0) {
        // Rising: a second jump while moving sideways starts a backflip.
        flx_sprite_play(s, "jump");
        if (h->canAirJump && jumpPressed && fabsf(s->velocity.x) > 0) {
            haci_jump(h);
            h->airJumping = true;
            h->canAirJump = false;
        }
    } else {
        h->canAirJump = false;
        flx_sprite_play(s, h->canClimb ? "walk" : "jump");
    }

    flx_sprite_update(s);

    if (h->airJumping && fabsf(s->angle) > 360) h->airJumping = false;

    if (!flx_sprite_on_screen(s)) {
        if (s->x > FLX_WIDTH) return HACI_LEFT_RIGHT_EDGE;
        if (s->x < 0) return HACI_LEFT_LEFT_EDGE;
    }
    return HACI_ON_MAP;
}

// --- Koyun (a sheep) -----------------------------------------------------

#define KOYUN_TURN_BACK_MARGIN 25
#define KOYUN_STUCK_FRAMES 10
#define KOYUN_WALK_SPEED 50
// A throw sends the sheep about 60 px forward and 17 px up from the
// haci's head, with gravity at 300.
#define KOYUN_THROW_SPEED_X 90
#define KOYUN_THROW_SPEED_Y 100

static bool coin_flip(void)
{
    return GetRandomValue(0, 1) == 0;
}

void koyun_init(Koyun *k, float x, float y)
{
    *k = (Koyun){0};
    FlxSprite *s = &k->sprite;
    flx_sprite_init(s, x, y, assets.koyun, 7, 7);

    flx_sprite_add_animation(s, "walk", (int[]){0, 1}, 2, 8, true);
    flx_sprite_add_animation(s, "caught", (int[]){0, 1}, 2, 15, true);
    flx_sprite_add_animation(s, "idle", (int[]){0}, 1, 0, false);

    s->maxVelocity.x = KOYUN_WALK_SPEED;
    s->acceleration.y = 300;
    s->drag.x = s->maxVelocity.x * 4;
    k->movingLeft = coin_flip();
    k->canJump = coin_flip();
}

void koyun_caught(Koyun *k, Haci *haci)
{
    if (k->isCaught) return;
    k->haci = haci;
    haci->caughtSomething = k;
    k->isCaught = true;
}

void koyun_released(Koyun *k, Haci *haci)
{
    if (!k->isCaught) return;
    k->haci = haci;
    haci->caughtSomething = NULL;
    k->isCaught = false;
    k->movingLeft = haci->sprite.facing == FLX_LEFT;
    k->justReleased = true;
}

// A port addition: the original only dropped sheep. The haci throws the
// sheep forward and a little up, the way it faces. In the air the sheep goes
// faster than it can walk and keeps its speed, until it lands.
void koyun_thrown(Koyun *k, Haci *haci)
{
    if (!k->isCaught) return;
    FlxSprite *s = &k->sprite;
    k->haci = haci;
    haci->caughtSomething = NULL;
    k->isCaught = false;
    k->thrown = true;
    k->movingLeft = haci->sprite.facing == FLX_LEFT;
    s->maxVelocity.x = KOYUN_THROW_SPEED_X;
    s->drag.x = 0;
    s->velocity.x = (k->movingLeft ? -1 : 1) * KOYUN_THROW_SPEED_X;
    s->velocity.y = -KOYUN_THROW_SPEED_Y;
}

static void koyun_fly(Koyun *k)
{
    FlxSprite *s = &k->sprite;
    s->scale.y = 1;
    s->acceleration.x = 0;
    s->facing = k->movingLeft ? FLX_RIGHT : FLX_LEFT; // the art faces left
    flx_sprite_update(s);

    // The screen edges act as walls in flight. Past them the map has no
    // floor, so a sheep thrown off screen would fall forever.
    if (s->x < 0) {
        s->x = 0;
        s->velocity.x = 0;
    } else if (s->x > FLX_WIDTH - s->width) {
        s->x = FLX_WIDTH - s->width;
        s->velocity.x = 0;
    }
}

static void koyun_land(Koyun *k)
{
    k->thrown = false;
    k->sprite.maxVelocity.x = KOYUN_WALK_SPEED;
    k->sprite.drag.x = KOYUN_WALK_SPEED * 4;
}

// The original counts a sheep as stuck unless both x and y changed.
// On flat ground y never changes, so sheep "get stuck" constantly.
// That quirk is what makes them wander and hop at random, so it stays.
static bool koyun_is_stuck(Koyun *k)
{
    bool moved = k->lastPosition.x != k->sprite.x && k->lastPosition.y != k->sprite.y;
    k->lastPosition = (Vector2){k->sprite.x, k->sprite.y};
    return !moved;
}

static void koyun_wander(Koyun *k)
{
    FlxSprite *s = &k->sprite;
    s->scale.y = 1;

    if (s->x < KOYUN_TURN_BACK_MARGIN) k->movingLeft = false;
    if (s->x > FLX_WIDTH - KOYUN_TURN_BACK_MARGIN) k->movingLeft = true;

    // Sheep art faces left, so "moving left" uses the unmirrored graphic.
    if (k->justReleased) {
        s->facing = k->movingLeft ? FLX_RIGHT : FLX_LEFT;
        s->velocity.y = -s->acceleration.y * 0.21f;
        s->velocity.x = 0;
        k->justReleased = false;
    } else if (k->movingLeft) {
        s->acceleration.x = -s->drag.x;
        s->facing = FLX_RIGHT;
    } else {
        s->acceleration.x = s->drag.x;
        s->facing = FLX_LEFT;
    }

    if (s->onFloor && k->canJump) {
        s->velocity.y = -s->acceleration.y * 0.42f;
        k->canJump = false;
    }

    flx_sprite_update(s);

    if (!koyun_is_stuck(k)) return;
    if (k->stuckCounter <= KOYUN_STUCK_FRAMES) {
        k->stuckCounter++;
        return;
    }
    k->stuckCounter = 0;
    if (flx_random() * 10 >= 3) k->movingLeft = !k->movingLeft;
    else k->canJump = true;
}

void koyun_update(Koyun *k)
{
    FlxSprite *s = &k->sprite;
    flx_sprite_save_position(s);
    s->acceleration.x = 0;

    if (k->isCaught) {
        // Carried upside down above the haci's head.
        flx_sprite_update(s);
        s->x = k->haci->sprite.x;
        s->y = k->haci->sprite.y - k->haci->sprite.height / 2 + 2;
        s->scale.y = -1;
        k->haci->hasSheep = true;
    } else if (k->thrown && !s->onFloor) {
        koyun_fly(k);
    } else {
        if (k->thrown) koyun_land(k);
        koyun_wander(k);
    }

    if (s->velocity.x != 0) flx_sprite_play(s, "walk");
    else if (k->isCaught) flx_sprite_play(s, "caught");
    else flx_sprite_play(s, "idle");
}

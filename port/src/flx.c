#include "flx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 6

void flx_sprite_init(FlxSprite *s, float x, float y, Texture2D texture, int frameWidth, int frameHeight)
{
    *s = (FlxSprite){0};
    s->x = s->lastX = x;
    s->y = s->lastY = y;
    s->width = frameWidth;
    s->height = frameHeight;
    s->maxVelocity = (Vector2){FLX_NO_LIMIT, FLX_NO_LIMIT};
    s->scale = (Vector2){1, 1};
    s->facing = FLX_RIGHT;
    s->texture = texture;
    s->frameWidth = frameWidth;
    s->frameHeight = frameHeight;
    s->currentAnim = -1;
}

void flx_sprite_add_animation(FlxSprite *s, const char *name, const int *frames, int frameCount, float fps, bool looped)
{
    FlxAnim *a = &s->anims[s->animCount++];
    a->name = name;
    a->frameCount = frameCount;
    memcpy(a->frames, frames, sizeof(int) * frameCount);
    a->fps = fps;
    a->looped = looped;
}

// Like Flixel, playing the animation that already runs does not restart it.
void flx_sprite_play(FlxSprite *s, const char *name)
{
    for (int i = 0; i < s->animCount; i++) {
        if (strcmp(s->anims[i].name, name) != 0) continue;
        if (s->currentAnim == i) return;
        s->currentAnim = i;
        s->animIndex = 0;
        s->animTimer = 0;
        s->frame = s->anims[i].frames[0];
        return;
    }
}

void flx_sprite_save_position(FlxSprite *s)
{
    s->lastX = s->x;
    s->lastY = s->y;
}

// FlxU.computeVelocity: acceleration wins over drag, then the speed is capped.
static float compute_velocity(float velocity, float acceleration, float drag, float max)
{
    if (acceleration != 0) {
        velocity += acceleration * FLX_ELAPSED;
    } else if (drag != 0) {
        float d = drag * FLX_ELAPSED;
        if (velocity - d > 0) velocity -= d;
        else if (velocity + d < 0) velocity += d;
        else velocity = 0;
    }
    if (velocity != 0 && max != FLX_NO_LIMIT) {
        if (velocity > max) velocity = max;
        else if (velocity < -max) velocity = -max;
    }
    return velocity;
}

// FlxObject.updateMotion: half the velocity change applies before the move
// and half after, which is how Flixel 2 integrates.
static void update_motion(FlxSprite *s)
{
    s->onFloor = false;

    float half = (compute_velocity(s->velocity.x, s->acceleration.x, s->drag.x, s->maxVelocity.x) - s->velocity.x) / 2;
    s->velocity.x += half;
    float dx = s->velocity.x * FLX_ELAPSED;
    s->velocity.x += half;

    half = (compute_velocity(s->velocity.y, s->acceleration.y, s->drag.y, s->maxVelocity.y) - s->velocity.y) / 2;
    s->velocity.y += half;
    float dy = s->velocity.y * FLX_ELAPSED;
    s->velocity.y += half;

    s->x += dx;
    s->y += dy;
}

static void update_animation(FlxSprite *s)
{
    if (s->currentAnim < 0) return;
    const FlxAnim *a = &s->anims[s->currentAnim];
    if (a->fps <= 0 || a->frameCount < 2) return;

    s->animTimer += FLX_ELAPSED;
    float delay = 1.0f / a->fps;
    while (s->animTimer > delay) {
        s->animTimer -= delay;
        if (s->animIndex == a->frameCount - 1) {
            if (a->looped) s->animIndex = 0;
        } else {
            s->animIndex++;
        }
        s->frame = a->frames[s->animIndex];
    }
}

void flx_sprite_update(FlxSprite *s)
{
    update_motion(s);
    update_animation(s);
}

void flx_sprite_draw(const FlxSprite *s)
{
    int columns = s->texture.width / s->frameWidth;
    Rectangle source = {
        (float)(s->frame % columns) * s->frameWidth,
        (float)(s->frame / columns) * s->frameHeight,
        (float)s->frameWidth,
        (float)s->frameHeight,
    };
    // A negative source size mirrors the image in raylib.
    if (s->facing == FLX_LEFT) source.width = -source.width;
    if (s->scale.y < 0) source.height = -source.height;

    float w = s->frameWidth * fabsf(s->scale.x);
    float h = s->frameHeight * fabsf(s->scale.y);
    Rectangle dest = {s->x + s->width / 2, s->y + s->height / 2, w, h};
    DrawTexturePro(s->texture, source, dest, (Vector2){w / 2, h / 2}, s->angle, WHITE);
}

bool flx_sprite_on_screen(const FlxSprite *s)
{
    return !(s->x + s->width < 0 || s->x > FLX_WIDTH || s->y + s->height < 0 || s->y > FLX_HEIGHT);
}

bool flx_overlaps(const FlxSprite *a, const FlxSprite *b)
{
    return a->x < b->x + b->width && a->x + a->width > b->x &&
           a->y < b->y + b->height && a->y + a->height > b->y;
}

// ponytail: simpler than Flixel's mass-sharing separation. Enough for sheep
// stacking in the pit. Port FlxU.solveXCollision if a heavier game needs it.
void flx_collide_sprites(FlxSprite *a, FlxSprite *b)
{
    if (!flx_overlaps(a, b)) return;

    float overlapX = fminf(a->x + a->width, b->x + b->width) - fmaxf(a->x, b->x);
    float overlapY = fminf(a->y + a->height, b->y + b->height) - fmaxf(a->y, b->y);

    if (overlapX < overlapY) {
        float push = overlapX / 2;
        if (a->x < b->x) { a->x -= push; b->x += push; }
        else { a->x += push; b->x -= push; }
        float shared = (a->velocity.x + b->velocity.x) / 2;
        a->velocity.x = b->velocity.x = shared;
        return;
    }

    FlxSprite *upper = a->y < b->y ? a : b;
    const FlxSprite *lower = upper == a ? b : a;
    upper->y = lower->y - upper->height;
    if (upper->velocity.y > 0) upper->velocity.y = 0;
    upper->onFloor = true;
}

// --- Tilemap -------------------------------------------------------------

// Reads a number attribute, such as key = "x=\"", from inside one tag.
static bool attr_float(const char *tag, const char *tagEnd, const char *key, float *out)
{
    const char *p = strstr(tag, key);
    if (!p || p > tagEnd) return false;
    *out = strtof(p + strlen(key), NULL);
    return true;
}

bool flx_tilemap_load(FlxTilemap *map, const char *tmx, const char *layerName, Texture2D tiles)
{
    char header[96];
    snprintf(header, sizeof header, "<layer name=\"%s\"", layerName);
    const char *layer = strstr(tmx, header);
    if (!layer) return false;

    float w = 0, h = 0;
    const char *tagEnd = strchr(layer, '>');
    attr_float(layer, tagEnd, "width=\"", &w);
    attr_float(layer, tagEnd, "height=\"", &h);

    map->widthInTiles = (int)w;
    map->heightInTiles = (int)h;
    map->tiles = tiles;
    map->data = calloc(map->widthInTiles * map->heightInTiles, sizeof(int));

    const char *end = strstr(layer, "</layer>");
    const char *p = layer;
    for (int i = 0; i < map->widthInTiles * map->heightInTiles; i++) {
        p = strstr(p, "gid=\"");
        if (!p || p > end) break;
        p += 5;
        int gid = atoi(p);
        map->data[i] = gid > 0 ? gid - 1 : 0;
    }
    return true;
}

void flx_tilemap_unload(FlxTilemap *map)
{
    free(map->data);
    map->data = NULL;
}

void flx_tilemap_draw(const FlxTilemap *map)
{
    int columns = map->tiles.width / TILE_SIZE;
    for (int ty = 0; ty < map->heightInTiles; ty++) {
        for (int tx = 0; tx < map->widthInTiles; tx++) {
            int tile = map->data[ty * map->widthInTiles + tx];
            if (tile == 0) continue;
            Rectangle source = {(float)(tile % columns) * TILE_SIZE, (float)(tile / columns) * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            DrawTextureRec(map->tiles, source, (Vector2){(float)tx * TILE_SIZE, (float)ty * TILE_SIZE}, WHITE);
        }
    }
}

static bool tile_solid(const FlxTilemap *map, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= map->widthInTiles || ty >= map->heightInTiles) return false;
    return map->data[ty * map->widthInTiles + tx] != 0;
}

// Calls fn for each solid tile the sprite overlaps. Stops when fn returns false.
typedef bool (*TileFn)(FlxSprite *s, Rectangle tile);

static void each_overlapping_tile(const FlxTilemap *map, FlxSprite *s, TileFn fn)
{
    int left = (int)floorf(s->x / TILE_SIZE);
    int right = (int)floorf((s->x + s->width - 0.001f) / TILE_SIZE);
    int top = (int)floorf(s->y / TILE_SIZE);
    int bottom = (int)floorf((s->y + s->height - 0.001f) / TILE_SIZE);

    for (int ty = top; ty <= bottom; ty++) {
        for (int tx = left; tx <= right; tx++) {
            if (!tile_solid(map, tx, ty)) continue;
            Rectangle tile = {(float)tx * TILE_SIZE, (float)ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            if (!fn(s, tile)) return;
        }
    }
}

// The side a sprite came from decides where it is pushed out.
static bool push_out_x(FlxSprite *s, Rectangle tile)
{
    bool cameFromLeft = s->lastX + s->width <= tile.x + 0.01f;
    bool cameFromRight = s->lastX >= tile.x + tile.width - 0.01f;
    // Already overlapping sideways before this move, like the haci spawning
    // 6 px into the street. Sideways motion did not cause it, so leave it to
    // the vertical pass, as Flixel does. Pushing sideways here would shove
    // the sprite tile by tile along the street and off the map.
    if (!cameFromLeft && !cameFromRight) return true;
    s->x = cameFromLeft ? tile.x - s->width : tile.x + tile.width;
    s->velocity.x = 0;
    return false; // the sprite moved, so the next tile check is stale
}

static bool push_out_y(FlxSprite *s, Rectangle tile)
{
    bool cameFromAbove = s->lastY + s->height <= tile.y + 0.01f;
    bool cameFromBelow = s->lastY >= tile.y + tile.height - 0.01f;
    if (!cameFromAbove && !cameFromBelow) {
        cameFromAbove = s->y + s->height / 2 < tile.y + tile.height / 2;
    }
    if (cameFromAbove) {
        s->y = tile.y - s->height;
        s->onFloor = true;
    } else {
        s->y = tile.y + tile.height;
    }
    s->velocity.y = 0;
    return false;
}

// Like FlxU.collide: resolve the horizontal move first, then the vertical one.
void flx_tilemap_collide(const FlxTilemap *map, FlxSprite *s)
{
    float newY = s->y;
    s->y = s->lastY;
    for (int i = 0; i < 4; i++) each_overlapping_tile(map, s, push_out_x);
    s->y = newY;
    for (int i = 0; i < 4; i++) each_overlapping_tile(map, s, push_out_y);
}

bool flx_tmx_object(const char *tmx, const char *name, Rectangle *out)
{
    char key[96];
    snprintf(key, sizeof key, "<object name=\"%s\"", name);
    const char *tag = strstr(tmx, key);
    if (!tag) return false;

    const char *tagEnd = strchr(tag, '>');
    *out = (Rectangle){0};
    attr_float(tag, tagEnd, " x=\"", &out->x);
    attr_float(tag, tagEnd, " y=\"", &out->y);
    attr_float(tag, tagEnd, " width=\"", &out->width);
    attr_float(tag, tagEnd, " height=\"", &out->height);
    return true;
}

float flx_random(void)
{
    return (float)GetRandomValue(0, 999999) / 1000000.0f;
}

// --- Input -----------------------------------------------------------------

#define KEY_SLOTS 512

static bool pressed[KEY_SLOTS];
static bool released[KEY_SLOTS];

void flx_input_poll(void)
{
    for (int key = 0; key < KEY_SLOTS; key++) {
        if (IsKeyPressed(key)) pressed[key] = true;
        if (IsKeyReleased(key)) released[key] = true;
    }
}

void flx_input_clear(void)
{
    memset(pressed, 0, sizeof pressed);
    memset(released, 0, sizeof released);
}

bool flx_key_pressed(int key)
{
    return pressed[key];
}

bool flx_key_released(int key)
{
    return released[key];
}

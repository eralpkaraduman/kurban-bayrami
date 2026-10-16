// The four screens of the game. Each original FlxState is one enter_/update_
// pair here. Only one screen exists at a time, so they share one struct.
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { STATE_INTRO, STATE_CITY, STATE_COLLECT, STATE_KILL } StateId;

static const Color SKY_COLOR = {0x73, 0xAD, 0xD2, 0xFF};

static struct {
    StateId id;
    char *tmx;
    FlxTilemap tilemap;
    FlxTilemap tools; // KillStage only: ladder and decor, drawn but not solid
    bool hasTools;
    Haci haci;
    Koyun koyunlar[MAX_KOYUN];
    int koyunCount;
    char message[128];

    // CityState remembers whether the haci arrived with a sheep.
    // The original passes that same value on to KillStage, even if the
    // sheep was dropped in the city. That behavior is kept.
    bool arrivedWithSheep;

    Rectangle ladder, dropZone, spawnZone; // KillStage only

    float introTime, blinkTimer; // IntroState only
    bool showPressX;
} state;

// Survives screen changes, like the original static RAM class.
static int cityStateMessages;
static int killStateMessages;
static int sacrificedCount;

static Koyun *add_koyun(float x, float y)
{
    if (state.koyunCount == MAX_KOYUN) return NULL;
    Koyun *k = &state.koyunlar[state.koyunCount++];
    koyun_init(k, x, y);
    return k;
}

static void unload_level(void)
{
    if (state.tmx) UnloadFileText(state.tmx);
    flx_tilemap_unload(&state.tilemap);
    flx_tilemap_unload(&state.tools);
    state.tmx = NULL;
    state.hasTools = false;
    state.koyunCount = 0;
    state.message[0] = '\0';
}

static void load_level(StateId id, const char *tmxPath)
{
    unload_level();
    state.id = id;
    state.tmx = LoadFileText(tmxPath);
    if (!state.tmx || !flx_tilemap_load(&state.tilemap, state.tmx, "map", assets.tiles)) {
        TraceLog(LOG_FATAL, "Cannot load map %s", tmxPath);
    }
}

static Rectangle tmx_object(const char *name)
{
    Rectangle r;
    if (!flx_tmx_object(state.tmx, name, &r)) TraceLog(LOG_FATAL, "Map object %s is missing", name);
    return r;
}

// C toggles carrying: press it next to a sheep to pick the sheep up, and
// press it again to throw the sheep forward. The original game needed C held
// down the whole time, which is hard to do with a thumb on a touch screen.
static void toggle_carry(bool canPickUp)
{
    if (!flx_key_pressed(KEY_C)) return;
    if (state.haci.caughtSomething) {
        koyun_thrown(state.haci.caughtSomething, &state.haci);
        return;
    }
    if (!canPickUp) return;
    for (int i = 0; i < state.koyunCount; i++) {
        if (flx_overlaps(&state.haci.sprite, &state.koyunlar[i].sprite)) {
            koyun_caught(&state.koyunlar[i], &state.haci);
            return;
        }
    }
}

static void update_koyunlar(void)
{
    for (int i = 0; i < state.koyunCount; i++) {
        Koyun *k = &state.koyunlar[i];
        koyun_update(k);
        // ponytail: carried sheep skip map collision so they never snag on
        // ceilings. The original collided them too.
        if (!k->isCaught) flx_tilemap_collide(&state.tilemap, &k->sprite);
    }
}

static void enter_intro(void);
static void enter_city(bool fromLeft, bool withSheep);
static void enter_collect(void);
static void enter_kill(bool withSheep);

// --- IntroState ----------------------------------------------------------

static void enter_intro(void)
{
    unload_level();
    state.id = STATE_INTRO;
    state.introTime = 0;
    state.blinkTimer = 0;
    state.showPressX = false;
}

// Bounce.easeOut from TweenMax, for t from 0 to 1.
static float bounce_ease_out(float t)
{
    if (t < 1 / 2.75f) return 7.5625f * t * t;
    if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

static void update_intro(void)
{
    state.introTime += FLX_ELAPSED;
    state.blinkTimer += FLX_ELAPSED;
    if (state.blinkTimer > 0.4f) {
        state.showPressX = !state.showPressX;
        state.blinkTimer = 0;
    }
    if (flx_key_pressed(KEY_X)) enter_city(true, false);
}

static void draw_intro(void)
{
    ClearBackground(BLACK);

    // The title shrinks from double size to fit the screen over 2 seconds.
    float t = state.introTime / 2;
    if (t > 1) t = 1;
    float scale = 1 - 0.5f * bounce_ease_out(t);
    float w = assets.title.width * scale;
    float h = assets.title.height * scale;
    Rectangle source = {0, 0, (float)assets.title.width, (float)assets.title.height};
    Rectangle dest = {FLX_WIDTH / 2.0f, FLX_HEIGHT / 2.0f, w, h};
    DrawTexturePro(assets.title, source, dest, (Vector2){w / 2, h / 2}, 0, WHITE);

    // The original prompt sat under the title, near the screen's center.
    const char *prompt = "Press X To Start";
    float promptX = floorf((FLX_WIDTH - MeasureTextEx(assets.font, prompt, FONT_SIZE, 0).x) / 2);
    if (state.showPressX) DrawTextEx(assets.font, prompt, (Vector2){promptX, 92}, FONT_SIZE, 0, WHITE);
}

// --- CityState: the street between the fields and the pit ----------------

static const char *CITY_MESSAGES[] = {
    "Press X to jump, arrow keys to move",
    "You can do a backflip while in the air",
    "You carry sheeps to the pit to the left",
    "Press C near a sheep to carry it",
    "The key X is the jump key",
    "Forest is a good place to find sheeps, here is not",
    "Come near a sheep and press C, this will make you carry it",
    "Dont leave sheeps here, I don't know what happens if you do",
    "Those appartments are designed to make you lose time, yes.",
    "You can do backflip, but do you really need it?",
    "X key makes you jump",
    "Toss sheeps in the pit to the left",
    "THE BAYRAM IS COMING",
    "I'm not really sure why I made this game",
    "This game IS NOT pointless!",
    "Every game has a purpose, maybe this is not one of them?",
    "You are not bored, your mind is tricking you",
    "There are no sheeps in this level, unless you bring one",
};

static void enter_city(bool fromLeft, bool withSheep)
{
    load_level(STATE_CITY, "assets/cityMap.tmx");
    int count = sizeof CITY_MESSAGES / sizeof CITY_MESSAGES[0];
    snprintf(state.message, sizeof state.message, "%s", CITY_MESSAGES[cityStateMessages++ % count]);

    Rectangle spawn = tmx_object(fromLeft ? "HaciSpawnLeft" : "HaciSpawnRight");
    haci_init(&state.haci, spawn.x, spawn.y);

    state.arrivedWithSheep = withSheep;
    if (withSheep) {
        Koyun *k = add_koyun(state.haci.sprite.x, state.haci.sprite.y);
        koyun_caught(k, &state.haci);
    }
}

static void update_city(void)
{
    HaciExit exit = haci_update(&state.haci);
    if (exit == HACI_LEFT_RIGHT_EDGE) { enter_collect(); return; }
    if (exit == HACI_LEFT_LEFT_EDGE) { enter_kill(state.arrivedWithSheep); return; }

    flx_tilemap_collide(&state.tilemap, &state.haci.sprite);
    update_koyunlar();
    toggle_carry(true);
}

// --- CollectState: the fields where the sheep live -----------------------

static void enter_collect(void)
{
    load_level(STATE_COLLECT, "assets/ruralMap.tmx");
    add_koyun(15, 15);
    add_koyun(165, -15);
    add_koyun(165, -45);
    add_koyun(200, 15);
    haci_init(&state.haci, 10, 80);
}

static void update_collect(void)
{
    // Walking off the right edge does nothing, as in the original.
    if (haci_update(&state.haci) == HACI_LEFT_LEFT_EDGE) {
        enter_city(false, state.haci.caughtSomething != NULL);
        return;
    }

    flx_tilemap_collide(&state.tilemap, &state.haci.sprite);
    update_koyunlar();
    toggle_carry(true);
}

// --- KillStage: the pit where sheep are collected -------------------------

// "%d" lines get a random number from 0 to 999, like the original.
static const char *KILL_MESSAGES[] = {
    "Snatch a sheep and bring it here",
    "Toss sheeps here, and get a new one",
    "You can do a backflip while in the air",
    "You can't carry sheeps in this pit, leave them be",
    "Come on, gather more sheeps!",
    "%d more sheeps remaining, won't you get them?",
    "Sheeps.. sheeps, this pit wants more sheeps",
    "Is this all you can get? Bring more!",
    "Don't try to grab sheeps in here, you won't succeed",
    "I need more sheeps.",
    "These are not enough, we require %d more.",
    "More sheeps.. they are needed here",
    "Carrying seeps is not allowed here.. SRSLY NO",
    "Lets see if you can bring %d more seeps?",
    "The more sheeps you bring, the more sheeps you will bring",
    "Those sheeps never finish.. do they?",
    "Use the ladder, it's useful",
};

static Vector2 random_point_in(Rectangle r)
{
    return (Vector2){flx_random() * r.width + r.x, flx_random() * r.height + r.y};
}

// The original test: horizontal overlap, and the feet inside the rectangle.
static bool feet_in_rect(const FlxSprite *s, Rectangle r)
{
    float bottom = s->y + s->height;
    return s->x + s->width >= r.x && s->x <= r.x + r.width && bottom >= r.y && bottom <= r.y + r.height;
}

static void enter_kill(bool withSheep)
{
    load_level(STATE_KILL, "assets/rageMap.tmx");
    int count = sizeof KILL_MESSAGES / sizeof KILL_MESSAGES[0];
    snprintf(state.message, sizeof state.message, KILL_MESSAGES[killStateMessages++ % count], GetRandomValue(0, 999));

    state.hasTools = flx_tilemap_load(&state.tools, state.tmx, "tools", assets.tiles);
    state.ladder = tmx_object("Ladder");
    state.dropZone = tmx_object("DropZone");
    state.spawnZone = tmx_object("SpawnZone");

    haci_init(&state.haci, 354, 10);

    // Every sheep collected so far comes back, dropped somewhere in the pit.
    for (int i = 0; i < sacrificedCount; i++) {
        Vector2 p = random_point_in(state.spawnZone);
        Koyun *k = add_koyun(p.x, p.y);
        if (k) k->markCollected = true;
    }

    // A brought sheep is let go at once and falls into the pit.
    if (withSheep) {
        Koyun *k = add_koyun(state.haci.sprite.x, state.haci.sprite.y);
        if (k) {
            koyun_caught(k, &state.haci);
            koyun_released(k, &state.haci);
        }
    }
}

static void update_kill(void)
{
    // Walking off the left edge does nothing, as in the original.
    if (haci_update(&state.haci) == HACI_LEFT_RIGHT_EDGE) {
        enter_city(true, false);
        return;
    }

    flx_tilemap_collide(&state.tilemap, &state.haci.sprite);
    update_koyunlar();
    for (int i = 0; i < state.koyunCount; i++) {
        for (int j = i + 1; j < state.koyunCount; j++) {
            flx_collide_sprites(&state.koyunlar[i].sprite, &state.koyunlar[j].sprite);
        }
    }
    toggle_carry(false); // "You can't carry sheeps in this pit"

    for (int i = 0; i < state.koyunCount; i++) {
        Koyun *k = &state.koyunlar[i];
        if (!k->markCollected && feet_in_rect(&k->sprite, state.dropZone)) {
            k->markCollected = true;
            sacrificedCount++;
            TraceLog(LOG_INFO, "Sheep collected. Total: %d", sacrificedCount);
        }
    }

    haci_set_can_climb(&state.haci, feet_in_rect(&state.haci.sprite, state.ladder));
}

// --- Entry points ---------------------------------------------------------

void game_init(void)
{
    enter_intro();
}

void game_update(void)
{
    switch (state.id) {
    case STATE_INTRO: update_intro(); break;
    case STATE_CITY: update_city(); break;
    case STATE_COLLECT: update_collect(); break;
    case STATE_KILL: update_kill(); break;
    }
}

void game_draw(void)
{
    if (state.id == STATE_INTRO) {
        draw_intro();
        return;
    }

    ClearBackground(SKY_COLOR);
    // The original adds the message first, so the map draws over it.
    if (state.message[0]) DrawTextEx(assets.font, state.message, (Vector2){4, 0}, FONT_SIZE, 0, WHITE);
    flx_tilemap_draw(&state.tilemap);
    if (state.hasTools) flx_tilemap_draw(&state.tools);
    for (int i = 0; i < state.koyunCount; i++) flx_sprite_draw(&state.koyunlar[i].sprite);
    flx_sprite_draw(&state.haci.sprite);
}

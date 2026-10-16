// Logic checks that need no window. Run from the port folder:
//   ./build/kurban_check
#include "../src/game.h"

// Release builds define NDEBUG, which would remove every assert below,
// including the setup calls written inside them. Keep them in all builds.
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>

Assets assets; // textures stay empty; only update logic runs here

static void run_frames(Haci *h, const FlxTilemap *map, int frames)
{
    for (int i = 0; i < frames; i++) {
        haci_update(h);
        flx_tilemap_collide(map, &h->sprite);
    }
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    char *tmx = LoadFileText("assets/ruralMap.tmx");
    assert(tmx);
    FlxTilemap map = {0};
    assert(flx_tilemap_load(&map, tmx, "map", assets.tiles));
    assert(map.widthInTiles == 60 && map.heightInTiles == 20);

    // The haci falls from its CollectState spawn point and lands on the ground.
    Haci haci;
    haci_init(&haci, 10, 80);
    run_frames(&haci, &map, 120);
    assert(haci.sprite.onFloor || haci.sprite.velocity.y == 0);
    float restingY = haci.sprite.y;
    run_frames(&haci, &map, 60);
    assert(fabsf(haci.sprite.y - restingY) < 0.01f); // stays put, no sinking
    assert(haci.sprite.y < FLX_HEIGHT);

    // Both city spawn points sit 6 px inside the street. The haci must be
    // lifted onto the street, not shoved sideways off the map.
    char *city = LoadFileText("assets/cityMap.tmx");
    FlxTilemap street = {0};
    assert(flx_tilemap_load(&street, city, "map", assets.tiles));
    const char *spawns[] = {"HaciSpawnLeft", "HaciSpawnRight"};
    for (int i = 0; i < 2; i++) {
        Rectangle spawn;
        assert(flx_tmx_object(city, spawns[i], &spawn));
        Haci h;
        haci_init(&h, spawn.x, spawn.y);
        run_frames(&h, &street, 60);
        assert(h.sprite.x == spawn.x);           // not pushed sideways
        assert(h.sprite.y + h.sprite.height == 108); // standing on the street top
    }
    flx_tilemap_unload(&street);
    UnloadFileText(city);

    // A caught sheep rides upside down above the haci's head.
    Koyun koyun;
    koyun_init(&koyun, haci.sprite.x, haci.sprite.y);
    koyun_caught(&koyun, &haci);
    assert(haci.caughtSomething == &koyun);
    koyun_update(&koyun);
    assert(koyun.sprite.x == haci.sprite.x);
    assert(koyun.sprite.y == haci.sprite.y - haci.sprite.height / 2 + 2);
    assert(koyun.sprite.scale.y < 0);
    assert(haci.hasSheep);

    // Releasing it makes it hop up and walk again.
    koyun_released(&koyun, &haci);
    assert(!koyun.isCaught && haci.caughtSomething == NULL);
    koyun_update(&koyun);
    assert(koyun.sprite.velocity.y < 0);
    assert(koyun.sprite.scale.y > 0);

    // Pressing C again throws the sheep forward and up, the way the haci
    // faces. It lands ahead of him, then walks at its normal speed again.
    // The pit has a long, flat floor, so nothing gets in the way.
    char *pitTmx = LoadFileText("assets/rageMap.tmx");
    FlxTilemap pit = {0};
    assert(flx_tilemap_load(&pit, pitTmx, "map", assets.tiles));
    for (int dir = -1; dir <= 1; dir += 2) {
        Haci thrower;
        haci_init(&thrower, 180, 40);
        run_frames(&thrower, &pit, 120); // land first
        assert(thrower.sprite.onFloor || thrower.sprite.velocity.y == 0);
        thrower.sprite.facing = dir < 0 ? FLX_LEFT : FLX_RIGHT;
        Koyun flier;
        koyun_init(&flier, thrower.sprite.x, thrower.sprite.y);
        koyun_caught(&flier, &thrower);
        koyun_update(&flier);
        float startX = flier.sprite.x, startY = flier.sprite.y, highest = startY;
        koyun_thrown(&flier, &thrower);
        assert(!flier.isCaught && thrower.caughtSomething == NULL && flier.thrown);
        for (int i = 0; i < 180 && flier.thrown; i++) {
            koyun_update(&flier);
            flx_tilemap_collide(&pit, &flier.sprite);
            if (flier.sprite.y < highest) highest = flier.sprite.y;
        }
        assert(!flier.thrown);                              // it landed
        assert((flier.sprite.x - startX) * dir > 30);       // forward, the way he faced
        assert(highest < startY - 10);                      // up first
        assert(flier.sprite.maxVelocity.x == 50 && flier.sprite.drag.x == 200);
    }
    flx_tilemap_unload(&pit);
    UnloadFileText(pitTmx);

    // A sheep thrown at the screen edge stays on screen.
    {
        Haci thrower;
        haci_init(&thrower, FLX_WIDTH - 10, 80);
        thrower.sprite.facing = FLX_RIGHT;
        Koyun flier;
        koyun_init(&flier, thrower.sprite.x, thrower.sprite.y);
        koyun_caught(&flier, &thrower);
        koyun_update(&flier);
        koyun_thrown(&flier, &thrower);
        for (int i = 0; i < 60; i++) koyun_update(&flier);
        assert(flier.sprite.x <= FLX_WIDTH - flier.sprite.width);
    }

    // Sheep dropped on each other stack instead of merging.
    Koyun a, b;
    koyun_init(&a, 100, 50);
    koyun_init(&b, 101, 46);
    flx_collide_sprites(&a.sprite, &b.sprite);
    assert(b.sprite.y + b.sprite.height <= a.sprite.y + 0.01f);
    assert(b.sprite.onFloor);

    // KillStage objects parse from the TMX.
    char *rage = LoadFileText("assets/rageMap.tmx");
    Rectangle ladder, drop;
    assert(flx_tmx_object(rage, "Ladder", &ladder));
    assert(ladder.x == 338 && ladder.y == 33 && ladder.width == 0 && ladder.height == 75);
    assert(flx_tmx_object(rage, "DropZone", &drop));
    assert(drop.width == 357 && drop.height == 56);

    // Touch buttons act like keys: a press and a release each latch once,
    // and the key reads as held in between. Repeated events are ignored.
    flx_input_clear();
    flx_touch_key(KEY_X, true);
    flx_touch_key(KEY_X, true);
    assert(flx_key_pressed(KEY_X) && flx_key_down(KEY_X) && !flx_key_released(KEY_X));
    flx_input_clear();
    assert(!flx_key_pressed(KEY_X) && flx_key_down(KEY_X));
    flx_touch_key(KEY_X, false);
    assert(flx_key_released(KEY_X) && !flx_key_down(KEY_X));
    flx_input_clear();
    // A tap shorter than one frame still counts as a press.
    flx_touch_key(KEY_C, true);
    flx_touch_key(KEY_C, false);
    assert(flx_key_pressed(KEY_C) && flx_key_released(KEY_C) && !flx_key_down(KEY_C));
    flx_input_clear();

    UnloadFileText(rage);
    UnloadFileText(tmx);
    flx_tilemap_unload(&map);
    puts("all checks passed");
    return 0;
}

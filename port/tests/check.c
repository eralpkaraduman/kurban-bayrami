// Logic checks that need no window. Run from the port folder:
//   ./build/kurban_check
#include "../src/game.h"

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

    UnloadFileText(rage);
    UnloadFileText(tmx);
    flx_tilemap_unload(&map);
    puts("all checks passed");
    return 0;
}

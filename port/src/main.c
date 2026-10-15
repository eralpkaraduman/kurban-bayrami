#include "game.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

Assets assets;

static RenderTexture2D screen;
static double accumulator;

// Picks where to draw the 360x120 game image in the window.
// Each game pixel covers a whole number of physical screen pixels, so pixel
// art stays sharp on any display density. Leftover space becomes black bars.
static Rectangle pixel_perfect_rect(void)
{
#if defined(PLATFORM_WEB)
    Vector2 dpi = {1, 1}; // the browser handles density, see web/shell.html
#else
    Vector2 dpi = GetWindowScaleDPI(); // 2 on a Retina display, 1 on most others
#endif
    int physicalW = GetRenderWidth();
    int physicalH = GetRenderHeight();
    int scale = physicalW / FLX_WIDTH < physicalH / FLX_HEIGHT ? physicalW / FLX_WIDTH : physicalH / FLX_HEIGHT;
    if (scale < 1) scale = 1;

    // raylib takes drawing coordinates in logical points, not physical pixels.
    float w = (float)(FLX_WIDTH * scale) / dpi.x;
    float h = (float)(FLX_HEIGHT * scale) / dpi.y;
    return (Rectangle){(GetScreenWidth() - w) / 2, (GetScreenHeight() - h) / 2, w, h};
}

// Runs the game at a fixed 60 updates per second, whatever the display rate.
// The original's movement code adds speed per frame, so the rate matters.
static void frame(void)
{
    flx_input_poll();

    accumulator += GetFrameTime();
    if (accumulator > 0.25) accumulator = 0.25; // avoid a spiral after a stall
    while (accumulator >= FLX_ELAPSED) {
        game_update();
        flx_input_clear();
        accumulator -= FLX_ELAPSED;
    }

    BeginTextureMode(screen);
    game_draw();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    // Render textures are stored upside down, so flip the source height.
    Rectangle source = {0, 0, FLX_WIDTH, -FLX_HEIGHT};
    DrawTexturePro(screen.texture, source, pixel_perfect_rect(), (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
}

// LoadFontEx anti-aliases glyphs, which smears this pixel font.
// FONT_BITMAP rasterizes each pixel fully on or fully off instead.
static Font load_pixel_font(const char *path, int size)
{
    int dataSize = 0;
    unsigned char *data = LoadFileData(path, &dataSize);
    Font font = {.baseSize = size, .glyphCount = 95};
    font.glyphs = LoadFontData(data, dataSize, size, NULL, 0, FONT_BITMAP, &font.glyphCount);
    Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, size, 2, 0);
    font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    UnloadFileData(data);
    return font;
}

int main(void)
{
#if defined(PLATFORM_WEB)
    // On the web the canvas is exactly the game's size, one canvas pixel per
    // game pixel. The page scales it up by a whole number, see web/shell.html.
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(FLX_WIDTH, FLX_HEIGHT,
#else
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(FLX_WIDTH * FLX_ZOOM, FLX_HEIGHT * FLX_ZOOM,
#endif
               "Kurban Bayrami: The Gathering");
#if !defined(PLATFORM_WEB)
    ChangeDirectory(GetApplicationDirectory()); // assets sit next to the binary
    SetTargetFPS(FLX_FPS);
#endif

    assets.haci = LoadTexture("assets/haci.png");
    assets.koyun = LoadTexture("assets/koyun.png");
    assets.tiles = LoadTexture("assets/tiles.png");
    assets.title = LoadTexture("assets/title.png");
    assets.font = load_pixel_font("assets/system.ttf", FONT_SIZE);
    screen = LoadRenderTexture(FLX_WIDTH, FLX_HEIGHT);

    game_init();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose()) frame();
#endif

    CloseWindow();
    return 0;
}

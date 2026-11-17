#pragma once
// Draws the game into an RGB565 frame of 288x195: the original 96x65 screen, 1 bit, scaled x3.
#include "assets.h"
#include "game.h"
#include "mono.h"
#include "raster.h"
#include <cstdint>
#include <vector>

namespace mk {

static const int VW = 96, VH = 65;     // original screen
static const int OUT_W = 288, OUT_H = 195;

enum Scheme {
    SCHEME_3410,  // green backlit LCD
    SCHEME_BW,    // black on white
    SCHEME_COUNT
};

struct Palette { uint16_t ink, paper; };
Palette palette(Scheme s);

class Renderer {
public:
    // `out` must hold OUT_W*OUT_H pixels.
    bool init(const Assets& a, uint16_t* out);
    void setScheme(Scheme s) { scheme_ = s; pal_ = palette(s); }
    Scheme scheme() const { return scheme_; }
    uint16_t ink() const { return pal_.ink; }
    uint16_t paper() const { return pal_.paper; }

    void drawGame(const Game& g);
    // Full-screen 96x65 picture (title, game over).
    void drawPicture(const Bitmap& b, int x = 0, int y = 0);
    void fill(uint16_t c);
    void drawMono(const Mono& m);
    // Bitmap at virtual coords, with an optional clip rectangle (virtual coords).
    void blit(const Bitmap& b, int x, int y, bool transparentWhite = false,
              int cx = 0, int cy = 0, int cw = VW, int ch = VH);

private:
    void scene3D(const Game& g);
    void hero(const Game& g);
    void effects(const Game& g, bool& heroDrawn);
    void hud(const Game& g);
    void put(int vx, int vy, uint16_t c);  // one virtual pixel
    void dither();
    void upscale();

    const Assets* A = nullptr;
    uint16_t* out_ = nullptr;
    Scheme scheme_ = SCHEME_3410;
    Palette pal_{0, 0xFFFF};
    Raster r_;
    std::vector<float> depth_;
    std::vector<uint16_t> buf_;  // 96x65 work buffer
    bool toBuf_ = false;         // put() writes the work buffer instead of the output
};

} // namespace mk

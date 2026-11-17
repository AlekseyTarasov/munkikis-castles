#include "render.h"
#include <cmath>

namespace mk {

namespace {
const uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
uint16_t gray(int v) { return rgb565(v, v, v); }
} // namespace

Palette palette(Scheme s) {
    if (s == SCHEME_BW) return {rgb565(18, 18, 18), rgb565(240, 240, 232)};
    return {rgb565(33, 49, 24), rgb565(156, 198, 123)};  // Nokia 3410 green backlight
}

bool Renderer::init(const Assets& a, uint16_t* out) {
    A = &a;
    out_ = out;
    depth_.assign(VW * VH, 0);
    buf_.assign(VW * VH, 0);
    r_.init(VW, VH, buf_.data(), depth_.data());
    r_.frustum(-3, 3, -2.03125f, 2.03125f, 3, 1000);  // the original projection
    setScheme(scheme_);
    return true;
}

void Renderer::put(int vx, int vy, uint16_t c) {
    if (vx < 0 || vy < 0 || vx >= VW || vy >= VH) return;
    if (toBuf_) {
        buf_[vy * VW + vx] = c;
        return;
    }
    uint16_t* p = out_ + vy * 3 * OUT_W + vx * 3;
    for (int j = 0; j < 3; j++, p += OUT_W) p[0] = p[1] = p[2] = c;
}

void Renderer::fill(uint16_t c) {
    for (int i = 0; i < OUT_W * OUT_H; i++) out_[i] = c;
}

void Renderer::blit(const Bitmap& b, int x, int y, bool transparentWhite, int cx, int cy, int cw, int ch) {
    for (int j = 0; j < b.h; j++)
        for (int i = 0; i < b.w; i++) {
            int vx = x + i, vy = y + j;
            if (vx < cx || vy < cy || vx >= cx + cw || vy >= cy + ch) continue;
            if (!b.opaque(i, j)) continue;
            bool k = b.black(i, j);
            if (!k && transparentWhite) continue;
            put(vx, vy, k ? pal_.ink : pal_.paper);
        }
}

void Renderer::drawMono(const Mono& m) {
    for (int y = 0; y < VH; y++)
        for (int x = 0; x < VW; x++) put(x, y, m.px[y * VW + x] ? pal_.ink : pal_.paper);
}

void Renderer::drawPicture(const Bitmap& b, int x, int y) {
    fill(pal_.paper);
    blit(b, x, y);
}

// ------------------------------------------------------------------ 3D (in grays, dithered later)

void Renderer::scene3D(const Game& g) {
    const float ONE = 65536.f;
    Raster& r = r_;
    r.clear(gray(255));
    r.loadIdentity();
    r.setColor(gray(0xE0));  // the distant ground plane
    r.scale(10, 10, 10);
    r.drawArrays(A->mesh[M_BACKDROP_VTX].data(), 3);

    // camera: one block behind Munkiki, tilted down 20 degrees
    r.loadIdentity();
    if (g.view != 1) {
        r.translate(0, -0.5f, -20);
        r.rotate(20, 1, 0, 0);
    } else {
        r.translate(0, -0.5f, -2);
        r.rotate(g.lookPitch / ONE, 1, 0, 0);
        r.rotate(g.lookYaw / ONE, 0, 1, 0);
        r.translate(0, 0, g.lookPitch / ONE / 8);
    }
    r.rotate(g.facing / ONE * 90, 0, 1, 0);
    r.translate(-g.px / ONE * 20, (-g.py / ONE - 0.5f) * 20, -g.pz / ONE * 20);

    // bouncy blocks pulse; standing on one makes the whole world bob
    float bounce = sinf((g.frame & 15) * 3.14159265f / 8) / 8;
    if (g.onBouncy) r.translate(0, -bounce * 20, 0);

    const int8_t* colors = A->mesh[M_COLORS].data();
    const auto& M = A->mesh;
    for (int i = 0; i < g.blockCount; i++) {
        const RenderBlock& b = g.blocks[i];
        if (!b.type) continue;
        r.push();
        r.translate(b.x / ONE * 20, b.y / ONE * 20, b.z / ONE * 20);
        bool bottom = b.y == 0;
        bool flat = bottom;  // bottom-layer blocks are drawn as floor tiles
        if (bottom) {
            if ((b.type == B_BOUNCY && bounce > 0) || (b.type == B_WALL && b.sy > 1)) flat = false;
            if (!flat) r.translate(0, 20, 0);
        }
        if (b.type == B_BOUNCY && !flat) r.scale(1, (bottom ? 0 : 1) + bounce, 1);
        else if (b.type == B_WALL) r.scale((float)b.sx, flat ? 1.f : (float)(b.sy - (bottom ? 1 : 0)), (float)b.sz);
        else if (b.y < 65536 && !flat) {
            r.translate(0, 20 - b.y / ONE * 20, 0);
            r.scale(1, b.y / ONE, 1);
        }
        r.setColor(0);
        if (flat) {
            r.drawElements(M[M_FLAT_EDGE_VTX].data(), 6, (const uint8_t*)M[M_FLAT_EDGE_IDX].data(), 12);
            r.setColor(gray((uint8_t)colors[b.type - 1]));
            r.drawElements(M[M_FLAT_VTX].data(), 4, (const uint8_t*)M[M_FLAT_IDX].data(), 6);
        } else {
            r.drawElements(M[M_CUBE_EDGE_VTX].data(), 20, (const uint8_t*)M[M_CUBE_EDGE_IDX].data(), 72);
            r.setColor(gray((uint8_t)colors[b.type - 1]));
            r.drawElements(M[M_CUBE_VTX].data(), 8, (const uint8_t*)M[M_CUBE_IDX].data(), 36);
        }
        r.pop();
    }

    for (int i = 0; i < g.itemCount; i++) {
        const RenderItem& it = g.items[i];
        if (!it.type) continue;
        r.push();
        r.translate(it.x / ONE * 20, it.y / ONE * 20, it.z / ONE * 20);
        r.rotate((g.frame & 31) * 45 / 4.f, 0, 1, 0);
        const Assets::Texture& t = A->texture[((it.type >> 3) - 1) & 3];
        r.bindTexture(t.lum, t.w, t.h, gray(0), gray(255));
        if (it.type == I_EXIT) {
            r.translate(0, 10, 0);
            r.rotate((g.frame & 15) * 45 / 2.f, 0, 0, 1);
            r.drawElements(M[M_KEY_VTX].data(), 4, (const uint8_t*)M[M_KEY_IDX].data(), 12, M[M_KEY_UV].data());
        } else {
            r.drawElements(M[M_NOTE_VTX].data(), 4, (const uint8_t*)M[M_NOTE_IDX].data(), 12, M[M_NOTE_UV].data());
        }
        r.unbindTexture();
        r.pop();
    }
}

// Munkiki is a 2D sprite always drawn at the same spot over the 3D scene.
void Renderer::hero(const Game& g) {
    const SpriteSheet& S = A->hero;
    int x = 28, y = 22 + g.heroDY;
    int cut = g.heroDY > 0 ? 2 + g.heroDY : 0;
    int skip = y < 0 ? -y : 0;
    cut += skip;
    if (y < 0) y = 0;
    int rows = 40 - cut;
    if (rows <= 0) return;
    int f = g.heroFrame & 0xFF;
    bool half = (f & 0x40) != 0, mirror = (f & 0x80) != 0;
    int idx = f & 0x3F;
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < 40; i++) {
            int sx, scan, off;
            if (half) {
                sx = i < 20 ? i : 39 - i;
                scan = 20;
                off = 43200 + idx * 800;
            } else {
                sx = mirror ? 39 - i : i;
                scan = 40;
                off = idx * 1600;
            }
            int sy = j + skip;
            if (!S.get(S.mask, off, scan, sx, sy)) continue;
            put(x + i, y + j, S.get(S.pix, off, scan, sx, sy) ? pal_.ink : pal_.paper);
        }
}

void Renderer::effects(const Game& g, bool& heroDrawn) {
    if (g.starFx >= 0) {
        int a = g.starFx;
        for (int i = 0; i < 9; i++) {
            int ox = i % 3 - 1, oy = i / 3 - 1;
            int x = 40 + (ox * 2) * (6 + (a * (oy == 0 ? 3 : 2) >> 1));
            int y = 20 + oy * (12 + (a * (ox == 0 ? 15 : 10) >> 3)) + (a * a >> 2);
            if (y < 40) blit(A->fx[5], x, y, false);
        }
    }
    if (g.fallFx >= 0) {
        int a = g.fallFx;
        for (int i = 0; i < 8; i++) {
            bool small = i % 3 != 0;
            int d = ((a + i) % 5 << 1) + (small ? 8 - (i * 5 & 7) : 0);
            if (d < a << 1 && d > (a << 2) - 28) blit(A->fx[small ? 1 : 2], 26 + i * 5, 48 - d, false);
            if (i == 3 && g.drawHero) { hero(g); heroDrawn = true; }
        }
    }
}

// Notes counter in the top-left corner, as on the phone.
void Renderer::hud(const Game& g) {
    if (g.notesLeft == 0 || g.outcome != O_NONE) return;
    const Bitmap& d = A->hudDigits;
    blit(d, 0, 0, false, 0, 0, 25, 14);
    int x = g.notesLeft < 10 ? 11 : 16, n = g.notesLeft, k = 0;
    do {
        int cx = x - k++ * 5;
        blit(d, cx - 24 - n % 10 * 4, 3, false, cx, 3, 4, 7);
    } while ((n /= 10) > 0);
}

// The phone had a 1-bit screen: ordered dithering turns the grays into ink and paper.
void Renderer::dither() {
    for (int y = 0; y < VH; y++)
        for (int x = 0; x < VW; x++) {
            uint16_t& c = buf_[y * VW + x];
            int v = ((c >> 11) & 31) << 3;
            int thr = BAYER4[(y & 3) * 4 + (x & 3)] * 16 + 8;
            c = v < thr ? pal_.ink : pal_.paper;
        }
}

void Renderer::upscale() {
    for (int y = 0; y < VH; y++)
        for (int x = 0; x < VW; x++) {
            uint16_t c = buf_[y * VW + x];
            uint16_t* p = out_ + y * 3 * OUT_W + x * 3;
            for (int j = 0; j < 3; j++, p += OUT_W) p[0] = p[1] = p[2] = c;
        }
}

void Renderer::drawGame(const Game& g) {
    toBuf_ = true;
    scene3D(g);
    dither();  // before the sprites, so they stay crisp
    bool heroDrawn = false;
    effects(g, heroDrawn);
    if (g.drawHero && !heroDrawn) hero(g);
    hud(g);
    toBuf_ = false;
    upscale();
}

} // namespace mk

#include "mono.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace mk {

void Mono::clear() {
    memset(px, 0, sizeof px);
    clip(0, 0, W, H);
}

void Mono::set(int x, int y, bool ink) {
    if (x < cx || y < cy || x >= cx + cw || y >= cy + ch) return;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    px[y * W + x] = ink ? 1 : 0;
}

void Mono::line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        set(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Mono::arc(int x, int y, int w, int h, int start, int len) {
    float rx = w / 2.f, ry = h / 2.f, mx = x + rx, my = y + ry;
    int steps = (int)(fabsf((float)len) * 2) + 1;
    for (int i = 0; i <= steps; i++) {
        float a = (start + len * (float)i / steps) * 3.14159265f / 180.f;
        set((int)lroundf(mx + rx * cosf(a)), (int)lroundf(my - ry * sinf(a)), true);
    }
}

void Mono::image(const Bitmap& b, int x, int y) {
    for (int j = 0; j < b.h; j++)
        for (int i = 0; i < b.w; i++) set(x + i, y + j, b.black(i, j));
}

// ------------------------------------------------------------------ castle map

// Five towers, five levels each, the path goes up one tower and down the next.
void CastleMap::spot(int s, int& x, int& y) const {
    x = s / 5 * 13;
    y = 39 - (s % 5) * 10;
    if (s % 10 > 4) {
        y = 39 - y;
        x--;
    }
}

void CastleMap::begin(const Bitmap& sheet, int done, int keys) {
    sheet_ = &sheet;
    done_ = done;
    keys_ = keys;
    Mono& g = base_;
    g.clear();
    int castles = 25 + keys;
    g.line(3, 51, 6, 54);  // the start of the path
    g.line(7, 54, 10, 51);
    int top = castles > 25 ? 3 : 2;
    g.clip(0, 0, castles == 26 ? 65 : 96, 9);
    for (int i = 0; i < top; i++) {
        g.arc(3 + 26 * i, 1, 20, 20, 0, 180);
        g.arc(10 + 26 * i, 6, 6, 6, 0, 180);
    }
    g.clip(0, 42, 96, 51);
    for (int i = 0; i < 2; i++) {
        g.arc(16 + 26 * i, 29, 20, 20, 180, 180);
        g.arc(23 + 26 * i, 38, 6, 6, 180, 180);
    }
    g.clip(0, 0, 96, 65);
    for (int i = 0; i < 5; i++) {
        int bottom = i == 0 ? 51 : 41;
        g.line(3 + i * 13, 9, 3 + i * 13, bottom);
        g.line(10 + i * 13, 9, 10 + i * 13, bottom);
    }
    if (castles == 25) {
        g.line(55, 9, 55, 1);
        g.line(62, 9, 62, 1);
        g.line(55, 1, 62, 1);
    } else if (castles == 26) {
        g.line(64, 1, 75, 1);
        g.line(64, 6, 75, 6);
        g.line(75, 1, 75, 6);
    } else {
        int e = castles * 10 - 251;
        g.line(68, 9, 68, e);
        g.line(75, 9, 75, e);
        g.line(68, e, 75, e);
    }
    // footprints on the levels already cleared
    for (int i = done - 1; i >= 0; i--) {
        int col = i % 5;
        int x = i / 5 * 13 + 2;
        int y = 42 - col * 10;
        if (i % 10 > 4) y = 44 - y;
        if (col == 4) x++;
        else if (col == 0 && i != 0) x--;
        g.clip(x, y, 11, 9);
        g.image(sheet, x, y - 53);
    }
    // the magic keys found so far, next to the extra castles
    for (int i = 0; i < keys; i++) {
        int y = 1 + i * 10;
        g.clip(80, y, 14, 10);
        g.image(sheet, 80 - (done < 26 + i ? 0 : 13), y - 13);
    }
    g.clip(0, 0, 96, 65);
    m_ = base_;
}

void CastleMap::step(int l) {
    Mono& g = m_;
    const Bitmap& sh = *sheet_;
    int tx, ty;
    spot(done_, tx, ty);
    if (l < 19) {
        // Munkiki's face walks from the previous spot to the next one
        int k = l > 8 ? l - 8 : 0;
        g = base_;
        int fx, fy;
        if (done_ > 0) {
            spot(done_ - 1, fx, fy);
        } else {  // a new game: he comes up from the start of the path
            fx = 0;
            fy = 49;
        }
        int x = (tx - fx) * k / 10 + fx, y = (ty - fy) * k / 10 + fy;
        g.clip(x, y, 15, 13);
        g.image(sh, x, y);
    } else if (l < 39 && done_ > 24) {
        // a magic key opens the next extra castle
        if (l < 35) {
            g.clip(tx, ty, 15, 13);
            g.image(sh, tx, ty);
        }
        if (l < 26) {
            g.clip(l + 51, ty + 4, 34 - l, 7);
            g.image(sh, l + 37, ty + 1);
        } else {
            int f = l < 35 ? (l - 1) % 3 : 0;
            if (l > 34) {
                int k = l - 35;
                g.clip(80, ty + 1, 14, 5);
                g.image(sh, 80 - (k == 3 ? 13 : 0), ty - 12 - ((k + 1) & 3) * 10);
            }
            g.clip(78, ty + 4, 7, 7);
            g.image(sh, 65, ty - 22 - f * 10);
        }
    }
    g.clip(0, 0, 96, 65);
}

} // namespace mk

#pragma once
// 96x65 one-bit canvas with the few MIDP-style primitives the castle map needs.
#include "assets.h"
#include <cstdint>

namespace mk {

struct Mono {
    static const int W = 96, H = 65;
    uint8_t px[W * H];  // 1 = ink
    int cx = 0, cy = 0, cw = W, ch = H;

    void clear();
    void clip(int x, int y, int w, int h) { cx = x; cy = y; cw = w; ch = h; }
    void set(int x, int y, bool ink);
    void line(int x0, int y0, int x1, int y1);
    // Arc of the ellipse inscribed in (x, y, w, h); angles in degrees, counter-clockwise from 3 o'clock.
    void arc(int x, int y, int w, int h, int start, int len);
    // Opaque bitmap at (x, y), limited by the clip rectangle.
    void image(const Bitmap& b, int x, int y);
};

// The castle map shown between levels (port of the original drawing).
class CastleMap {
public:
    // done: levels completed (the marker moves from spot done-1 to spot done), keys: magic keys found.
    void begin(const Bitmap& sheet, int done, int keys);
    // Draw the frame for tick `l` (1, 2, 3...) on top of what is already there, like the phone did.
    void step(int l);
    const Mono& screen() const { return m_; }

private:
    void spot(int s, int& x, int& y) const;
    const Bitmap* sheet_ = nullptr;
    Mono base_, m_;
    int done_ = 0, keys_ = 0;
};

} // namespace mk

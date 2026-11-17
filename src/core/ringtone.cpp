#include "ringtone.h"
#include <cmath>

namespace mk {

namespace {
const uint16_t kTempo[32] = {25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90, 100, 112, 125, 140,
                             160, 180, 200, 225, 250, 285, 320, 355, 400, 450, 500, 565, 635, 715, 800, 900};

struct Bits {
    const std::vector<uint8_t>& d;
    size_t p;
    bool err = false;
    int get(int n) {
        int v = 0;
        for (int i = 0; i < n; i++, p++) {
            if ((p >> 3) >= d.size()) { err = true; return 0; }
            v = (v << 1) | ((d[p >> 3] >> (7 - (p & 7))) & 1);
        }
        return v;
    }
};
} // namespace

uint32_t Melody::durationMs() const {
    uint32_t ms = 0;
    for (auto& n : notes) {
        int s = n.size < 0 ? -n.size : n.size;
        uint32_t d = 240000 / tempo / (s ? s : 4);
        if (n.size < 0) d = d * 3 / 2;
        ms += d;
    }
    return ms;
}

bool parseRingtone(const std::vector<uint8_t>& data, Melody& out) {
    out.notes.clear();
    // 0x02 0x4A 0x3A: ringing tone programming, sound; then song type + empty title + sequence length.
    if (data.size() < 6 || data[0] != 0x02 || data[1] != 0x4A || data[2] != 0x3A) return false;
    Bits b{data, 38};
    if (b.get(3) != 0) return false;  // pattern header
    b.get(2);                          // pattern id
    b.get(4);                          // loop
    int count = b.get(8);
    int scale = 1;
    bool tempoSet = false;
    for (int i = 0; i < count && !b.err; i++) {
        int ins = b.get(3);
        if (ins == 1) {
            int note = b.get(4), dur = b.get(3), spec = b.get(2);
            ToneNote t;
            if (note == 0 || note > 12) t.freq = 0;
            else t.freq = (uint16_t)lroundf(440.f * powf(2.f, scale + (note - 10) / 12.f));
            int8_t size = (int8_t)(1 << (dur > 5 ? 5 : dur));
            t.size = spec == 1 || spec == 2 ? (int8_t)-size : size;
            out.notes.push_back(t);
        } else if (ins == 2) {
            scale = b.get(2);
        } else if (ins == 3) {
            b.get(2);  // style
        } else if (ins == 4) {
            int t = kTempo[b.get(5)];
            if (!tempoSet) { out.tempo = t; tempoSet = true; }
        } else if (ins == 5) {
            b.get(4);  // volume
        } else {
            break;
        }
    }
    return !out.notes.empty();
}

} // namespace mk

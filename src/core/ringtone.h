#pragma once
// Nokia Smart Messaging ringtones (the format of the game's sound effects).
#include <cstdint>
#include <vector>

namespace mk {

struct ToneNote {
    uint16_t freq;  // Hz, 0 = rest
    int8_t size;    // 1 whole, 2 half, 4 quarter...; negative = dotted (Lilka buzzer convention)
};

struct Melody {
    int tempo = 120;  // beats per minute
    std::vector<ToneNote> notes;
    uint32_t durationMs() const;
};

bool parseRingtone(const std::vector<uint8_t>& data, Melody& out);

} // namespace mk

#pragma once
// Game data loaded from the user's own copy of the original jar.
// Nothing from the original game is embedded in this engine.
#include <cstdint>
#include <string>
#include <vector>

namespace mk {

// 1-bit bitmap, rows padded to whole bytes, MSB = leftmost pixel, 1 = black.
struct Bitmap {
    int w = 0, h = 0, stride = 0;  // stride in pixels (multiple of 8)
    std::vector<uint8_t> pix;
    std::vector<uint8_t> mask;     // optional, same layout, 1 = opaque
    bool black(int x, int y) const { return (pix[(y * stride + x) >> 3] >> (7 - ((y * stride + x) & 7))) & 1; }
    bool opaque(int x, int y) const {
        return mask.empty() || ((mask[(y * stride + x) >> 3] >> (7 - ((y * stride + x) & 7))) & 1);
    }
};

// Player sprite sheet: 1-bit, packed vertically (8 rows per byte, LSB = top).
struct SpriteSheet {
    std::vector<uint8_t> pix, mask;
    // Frame pixel at (x,y). `half` frames are 20 px wide and mirrored to 40 px.
    bool get(const std::vector<uint8_t>& buf, int bitOffset, int scan, int x, int y) const {
        int P = bitOffset + y * scan + x;
        int row = P / scan, col = P % scan;
        return (buf[(row >> 3) * scan + col] >> (row & 7)) & 1;
    }
};

enum Cell : uint8_t {
    // low 3 bits: block type
    BLOCK_MASK = 0x07,
    B_NORMAL = 1, B_BOUNCY = 2, B_BREAKABLE = 3, B_SLIDING = 4, B_WALL = 5, B_FIXED = 6,
    // bits 3..5: item
    ITEM_MASK = 0x38,
    I_NOTE = 8, I_KEY = 16, I_EXIT = 24, I_LIFE = 32, I_PLAYER = 40,
    // bits 6..7: travelling blocks / their track
    TRACK = 0x80, TRAVELLER = 0xC0,
};

struct Level {
    int number = 0;
    uint8_t grid[2048];  // index = x + z*16 + y*256 ; 16 x 8(height) x 16
    int startX = 0, startY = 0, startZ = 0, facing = 0;
    int par = 0;         // move count used for scoring
    int notes = 0;
};

struct Assets {
    std::vector<std::string> text;          // 27 UI strings (requested language, usually English)
    std::vector<std::string> textUk;        // official Ukrainian strings, if this build has them (empty otherwise)
    SpriteSheet hero;
    std::vector<std::vector<uint8_t>> anims; // animation frame tables
    std::vector<int8_t> mesh[16];           // see MeshId
    Bitmap hudDigits, hudKeypad, hudKeyMark;
    Bitmap fx[12];                          // effects (balloon, stars, ...)
    Bitmap mapScreen, gameOver, title;
    struct Texture { int w = 0, h = 0; uint8_t lum[32 * 32]; };  // 0 = black, 255 = white
    Texture texture[4];                     // 0 note, 1 key, 2 exit, 3 extra life
    std::vector<std::vector<uint8_t>> sounds;
    std::vector<int> levelNumbers;

    // Load everything from a jar image in memory. `lang` like "en", "pl-PL".
    bool load(const uint8_t* jar, size_t len, const char* lang, std::string* error);
    // Same, but also accepts a .zip that contains the jar (like the original dumps).
    bool loadAny(const uint8_t* data, size_t len, const char* lang, std::string* error);
    bool loadLevel(int number, Level& out, bool lifeTaken = false) const;

private:
    std::vector<uint8_t> levelBlob_;
    uint8_t cellCodes_[13] = {0};
    std::vector<int> levelOffsets_;
};

enum MeshId {
    M_COLORS, M_NOTE_IDX, M_NOTE_UV, M_NOTE_VTX, M_KEY_IDX, M_CUBE_IDX, M_FLAT_IDX, M_CUBE_VTX,
    M_FLAT_VTX, M_CUBE_EDGE_IDX, M_FLAT_EDGE_IDX, M_CUBE_EDGE_VTX, M_FLAT_EDGE_VTX, M_KEY_UV,
    M_KEY_VTX, M_BACKDROP_VTX
};

} // namespace mk

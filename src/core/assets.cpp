#include "assets.h"
#include "zip.h"
#include <cstring>

namespace mk {

namespace {

struct Reader {
    const std::vector<uint8_t>& d;
    size_t p;
    bool err = false;
    int u8() {
        if (p >= d.size()) { err = true; return 0; }
        return d[p++];
    }
    int u16le() { int a = u8(); return a | (u8() << 8); }
    void bytes(void* dst, size_t n) {
        if (p + n > d.size()) { err = true; return; }
        memcpy(dst, d.data() + p, n);
        p += n;
    }
};

// Image block: u16le width, u16le height, rows padded to 8 pixels.
bool readImage(Reader& r, Bitmap& b) {
    b.w = r.u16le();
    b.h = r.u16le();
    if (b.w <= 0 || b.h <= 0 || b.w > 256 || b.h > 256) { r.err = true; return false; }
    b.stride = (b.w + 7) & ~7;
    b.pix.resize((b.stride >> 3) * b.h);
    r.bytes(b.pix.data(), b.pix.size());
    return !r.err;
}

// Bit stream used by the level format: LSB first.
struct LevelBits {
    const uint8_t* p;
    const uint8_t* end;
    int cur = 0, used = 8;
    bool err = false;
    int get(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) {
            if (used == 8) {
                if (p >= end) { err = true; return 0; }
                cur = *p++;
                used = 0;
            }
            if (cur & (1 << used)) v |= 1 << i;
            used++;
        }
        return v;
    }
};

} // namespace

bool Assets::load(const uint8_t* jar, size_t len, const char* lang, std::string* error) {
    auto fail = [&](const char* m) { if (error) *error = m; return false; };
    Zip z;
    if (!z.open(jar, len)) return fail("not a zip/jar");

    std::vector<uint8_t> d, buf;
    if (!z.extract("d", d) || d.size() < 1000) return fail("resource 'd' missing - unsupported jar version?");

    // --- UI strings (DataInput.readUTF x 27)
    std::string name = lang ? lang : "en";
    if (!z.extract(name.c_str(), buf)) {
        name = name.substr(0, 2);
        if (!z.extract(name.c_str(), buf) && !z.extract("en", buf)) return fail("language file missing");
    }
    auto readStrings = [](const std::vector<uint8_t>& src, std::vector<std::string>& out) {
        Reader r{src, 0};
        out.clear();
        for (int i = 0; i < 27; i++) {
            int n = (r.u8() << 8) | r.u8();
            std::string str;
            for (int k = 0; k < n; k++) str += (char)r.u8();
            out.push_back(str);
        }
        return !r.err;
    };
    if (!readStrings(buf, text)) return fail("bad language file");
    textUk.clear();
    if (z.extract("uk-UA", buf) || z.extract("uk", buf))
        if (!readStrings(buf, textUk)) textUk.clear();

    // Resource file 'd': blocks stored back to back (offsets are not fixed, so other builds work too):
    // 12 sounds | meshes, HUD and effect bitmaps | animation tables | map sheet | game over | title | 4 textures
    {
        Reader r{d, 0};
        auto sane = [](const Bitmap& b, int maxW, int maxH) { return b.w > 0 && b.h > 0 && b.w <= maxW && b.h <= maxH; };
        sounds.assign(12, {});
        for (auto& snd : sounds) {
            int n = r.u16le();
            if (n <= 0 || n > 1024) return fail("bad sound block");
            snd.resize(n);
            r.bytes(snd.data(), n);
        }
        static const int sizes[16] = {6, 12, 8, 12, 12, 36, 6, 24, 12, 72, 12, 60, 18, 8, 12, 9};
        for (int i = 0; i < 16; i++) {
            mesh[i].resize(sizes[i]);
            r.bytes(mesh[i].data(), sizes[i]);
        }
        if (!readImage(r, hudDigits) || !sane(hudDigits, 128, 32)) return fail("bad HUD image");
        std::vector<uint8_t> raw[23];
        int w[23], h[23];
        for (int i = 0; i < 23; i++) {
            w[i] = r.u8(); r.u8();
            h[i] = r.u8(); r.u8();
            if (w[i] <= 0 || h[i] <= 0 || w[i] > 64 || h[i] > 65) return fail("bad effect bitmap");
            raw[i].resize((w[i] >> 3) * h[i]);
            r.bytes(raw[i].data(), raw[i].size());
        }
        // pixel bitmaps 0..11, masks 12..22 belong to bitmaps 1..11 (bitmap 0 has no mask)
        for (int i = 0; i < 12; i++) {
            fx[i].w = w[i]; fx[i].h = h[i]; fx[i].stride = w[i];
            fx[i].pix = raw[i];
            if (i > 0) fx[i].mask = raw[i + 11];
        }
        readImage(r, hudKeypad);
        readImage(r, hudKeyMark);
        anims.assign(r.u8(), {});
        if (anims.size() < 17) return fail("bad animation tables");
        for (auto& an : anims) {
            an.resize(r.u8());
            if (an.empty()) return fail("bad animation tables");
            r.bytes(an.data(), an.size());
        }
        if (!readImage(r, mapScreen) || !sane(mapScreen, 96, 96)) return fail("bad map image");
        if (!readImage(r, gameOver) || !sane(gameOver, 96, 65)) return fail("bad game over image");
        if (!readImage(r, title) || !sane(title, 96, 65)) return fail("bad title image");
        for (int t = 0; t < 4; t++) {
            Bitmap b;
            readImage(r, b);
            if (r.err || !sane(b, 32, 32)) return fail("bad textures");
            texture[t].w = b.w;
            texture[t].h = b.h;
            for (int y = 0; y < b.h; y++)
                for (int x = 0; x < b.w; x++) texture[t].lum[y * b.w + x] = b.black(x, y) ? 0 : 255;
        }
        if (r.err) return fail("resource file too short");
    }
    // --- hero sprite sheet
    if (!z.extract("i", hero.pix) || !z.extract("a", hero.mask) || hero.pix.size() < 8200 || hero.mask.size() < 8200)
        return fail("sprite data missing");

    // --- levels
    if (!z.extract("MBF", levelBlob_) || levelBlob_.size() < 14) return fail("level data missing");
    memcpy(cellCodes_, levelBlob_.data(), 13);
    int count = levelBlob_[13];
    size_t off = 14 + count;
    levelOffsets_.clear();
    levelNumbers.clear();
    for (int i = 0; i < count; i++) {
        if (off >= levelBlob_.size()) return fail("bad level table");
        levelOffsets_.push_back((int)off);
        levelNumbers.push_back(levelBlob_[off]);
        off += levelBlob_[14 + i];
    }
    return true;
}

bool Assets::loadAny(const uint8_t* data, size_t len, const char* lang, std::string* error) {
    if (load(data, len, lang, error)) return true;
    Zip z;
    if (!z.open(data, len)) return false;
    for (int i = 0;; i++) {
        std::string name = z.findSuffix(".jar", i);
        if (name.empty()) break;
        std::vector<uint8_t> inner;
        if (z.extract(name.c_str(), inner) && load(inner.data(), inner.size(), lang, error)) return true;
    }
    return false;
}

bool Assets::loadLevel(int number, Level& L, bool lifeTaken) const {
    size_t idx = 0;
    for (; idx < levelNumbers.size(); idx++)
        if (levelNumbers[idx] == number) break;
    if (idx == levelNumbers.size()) return false;

    memset(L.grid, 0, sizeof(L.grid));
    L.number = number;
    L.notes = 0;
    const uint8_t* p = levelBlob_.data() + levelOffsets_[idx] + 1;
    LevelBits b{p, levelBlob_.data() + levelBlob_.size()};
    L.facing = b.get(2);
    L.par = b.get(16);
    for (int t = 0; t < 13; t++) {
        if (!b.get(1)) continue;
        uint8_t code = cellCodes_[t];
        if ((0x1E0 >> t) & 1) {  // single object placed at x,y,z
            int x = b.get(4), y = b.get(3), z = b.get(4);
            if (lifeTaken && code == I_LIFE) continue;
            L.grid[x + (z << 4) + (y << 8)] |= code;
            if (code == I_PLAYER) { L.startX = x; L.startY = y; L.startZ = z; }
            continue;
        }
        for (int y = 0; y < 8; y++) {
            if (!b.get(1)) continue;
            int x0 = b.get(4), x1 = x0 + b.get(4);
            int z0 = b.get(4), z1 = z0 + b.get(4);
            bool solid = (x1 - x0) + (z1 - z0) < 2;
            for (int z = z0; z <= z1; z++)
                for (int x = x0; x <= x1; x++)
                    if (solid || b.get(1)) {
                        if (x < 16 && z < 16) L.grid[x + (z << 4) + (y << 8)] |= code;
                        if (code == I_NOTE) L.notes++;
                    }
        }
    }
    return !b.err;
}

} // namespace mk

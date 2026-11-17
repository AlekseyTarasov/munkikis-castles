#include "zip.h"
#include <cstring>

namespace mk {

namespace {

struct BitReader {
    const uint8_t* p;
    const uint8_t* end;
    uint32_t buf = 0;
    int cnt = 0;
    bool err = false;

    int bit() {
        if (cnt == 0) {
            if (p >= end) { err = true; return 0; }
            buf = *p++;
            cnt = 8;
        }
        int b = buf & 1;
        buf >>= 1;
        cnt--;
        return b;
    }
    int bits(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) v |= bit() << i;
        return v;
    }
    void alignByte() { cnt = 0; }
};

// Canonical Huffman table (as in zlib's puff).
struct Huff {
    uint16_t count[16];
    uint16_t symbol[320];
};

bool build(Huff& h, const uint8_t* lengths, int n) {
    memset(h.count, 0, sizeof(h.count));
    for (int i = 0; i < n; i++) h.count[lengths[i]]++;
    if (h.count[0] == n) return true;  // empty code
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return false;
    }
    uint16_t offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
    for (int i = 0; i < n; i++)
        if (lengths[i]) h.symbol[offs[lengths[i]]++] = (uint16_t)i;
    return true;
}

int decode(BitReader& br, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= br.bit();
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (br.err) return -1;
    }
    return -1;
}

const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                               35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                               3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                6145, 8193, 12289, 16385, 24577};
const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(BitReader& br, std::vector<uint8_t>& out, const Huff& lit, const Huff& dist) {
    for (;;) {
        int sym = decode(br, lit);
        if (sym < 0) return false;
        if (sym < 256) {
            out.push_back((uint8_t)sym);
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            int len = kLenBase[sym] + br.bits(kLenExtra[sym]);
            int ds = decode(br, dist);
            if (ds < 0 || ds >= 30) return false;
            size_t d = kDistBase[ds] + br.bits(kDistExtra[ds]);
            if (d > out.size()) return false;
            size_t from = out.size() - d;
            for (int i = 0; i < len; i++) out.push_back(out[from + i]);
        }
        if (br.err) return false;
    }
}

} // namespace

bool inflate(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t expectedLen) {
    out.clear();
    if (expectedLen) out.reserve(expectedLen);
    BitReader br{src, src + srcLen};
    static Huff lit, dist;  // static: keeps them off the (small) MCU stack
    int last;
    do {
        last = br.bit();
        int type = br.bits(2);
        if (type == 0) {
            br.alignByte();
            if (br.end - br.p < 4) return false;
            unsigned len = br.p[0] | (br.p[1] << 8);
            br.p += 4;
            if ((size_t)(br.end - br.p) < len) return false;
            out.insert(out.end(), br.p, br.p + len);
            br.p += len;
        } else if (type == 1) {
            uint8_t l[288];
            int i = 0;
            for (; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            build(lit, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            build(dist, l, 30);
            if (!codes(br, out, lit, dist)) return false;
        } else if (type == 2) {
            int nlen = br.bits(5) + 257, ndist = br.bits(5) + 1, ncode = br.bits(4) + 4;
            static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t lengths[320] = {0};
            for (int i = 0; i < ncode; i++) lengths[order[i]] = (uint8_t)br.bits(3);
            if (!build(lit, lengths, 19)) return false;
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = decode(br, lit);
                if (sym < 0) return false;
                if (sym < 16) {
                    lengths[idx++] = (uint8_t)sym;
                } else {
                    int len = 0, rep;
                    if (sym == 16) {
                        if (idx == 0) return false;
                        len = lengths[idx - 1];
                        rep = 3 + br.bits(2);
                    } else if (sym == 17) {
                        rep = 3 + br.bits(3);
                    } else {
                        rep = 11 + br.bits(7);
                    }
                    if (idx + rep > nlen + ndist) return false;
                    while (rep--) lengths[idx++] = (uint8_t)len;
                }
            }
            if (!build(lit, lengths, nlen)) return false;
            if (!build(dist, lengths + nlen, ndist)) return false;
            if (!codes(br, out, lit, dist)) return false;
        } else {
            return false;
        }
        if (br.err) return false;
    } while (!last);
    return true;
}

static uint32_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool Zip::open(const uint8_t* data, size_t len) {
    data_ = data;
    len_ = len;
    if (len < 22) return false;
    // End of central directory record: search backwards (comment may follow).
    for (size_t i = len - 22;; i--) {
        if (rd32(data + i) == 0x06054b50) {
            entries_ = (int)rd16(data + i + 10);
            cdOffset_ = rd32(data + i + 16);
            return cdOffset_ < len;
        }
        if (i == 0 || len - i > 22 + 65535) break;
    }
    return false;
}

bool Zip::extract(const char* name, std::vector<uint8_t>& out) const {
    size_t p = cdOffset_;
    size_t nameLen = strlen(name);
    for (int e = 0; e < entries_; e++) {
        if (p + 46 > len_ || rd32(data_ + p) != 0x02014b50) return false;
        uint32_t method = rd16(data_ + p + 10);
        uint32_t csize = rd32(data_ + p + 20);
        uint32_t usize = rd32(data_ + p + 24);
        uint32_t fnLen = rd16(data_ + p + 28);
        uint32_t extraLen = rd16(data_ + p + 30);
        uint32_t commLen = rd16(data_ + p + 32);
        uint32_t local = rd32(data_ + p + 42);
        const char* fn = (const char*)data_ + p + 46;
        if (fnLen == nameLen && memcmp(fn, name, nameLen) == 0) {
            if (local + 30 > len_) return false;
            size_t start = local + 30 + rd16(data_ + local + 26) + rd16(data_ + local + 28);
            if (start + csize > len_) return false;
            if (method == 0) {
                out.assign(data_ + start, data_ + start + csize);
                return true;
            }
            if (method == 8) return inflate(data_ + start, csize, out, usize) && out.size() == usize;
            return false;
        }
        p += 46 + fnLen + extraLen + commLen;
    }
    return false;
}

std::string Zip::findSuffix(const char* suffix, int n) const {
    size_t p = cdOffset_;
    size_t sl = strlen(suffix);
    for (int e = 0; e < entries_; e++) {
        if (p + 46 > len_ || rd32(data_ + p) != 0x02014b50) break;
        uint32_t fnLen = rd16(data_ + p + 28), extraLen = rd16(data_ + p + 30), commLen = rd16(data_ + p + 32);
        if (p + 46 + fnLen > len_) break;
        const char* fn = (const char*)data_ + p + 46;
        if (fnLen >= sl) {
            bool match = true;
            for (size_t i = 0; i < sl && match; i++) {
                char a = fn[fnLen - sl + i], b = suffix[i];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                match = a == b;
            }
            if (match && n-- == 0) return std::string(fn, fnLen);
        }
        p += 46 + fnLen + extraLen + commLen;
    }
    return std::string();
}

} // namespace mk

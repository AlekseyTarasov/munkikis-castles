#pragma once
// Minimal ZIP/JAR reader with a built-in DEFLATE decoder.
// Works on a whole archive held in memory (the game jar is ~33 KB).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mk {

// Decode a raw DEFLATE stream. Returns false on malformed input.
bool inflate(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t expectedLen = 0);

class Zip {
public:
    bool open(const uint8_t* data, size_t len);
    // Extract a file by exact name. Returns false if missing or corrupt.
    bool extract(const char* name, std::vector<uint8_t>& out) const;
    // Name of the n-th entry whose name ends with `suffix` (case-insensitive), or empty.
    std::string findSuffix(const char* suffix, int n = 0) const;

private:
    const uint8_t* data_ = nullptr;
    size_t len_ = 0;
    size_t cdOffset_ = 0;
    int entries_ = 0;
};

} // namespace mk

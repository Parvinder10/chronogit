#pragma once

#include <vector>
#include <span>
#include <cstdint>

namespace chronogit {

class Compressor {
public:
    // Compresses data using ChronoGit LZ77 format
    static std::vector<uint8_t> compress(std::span<const uint8_t> data);

    // Decompresses data, throws ChronoGitException on corruption or invalid header
    static std::vector<uint8_t> decompress(std::span<const uint8_t> compressed_data);
};

} // namespace chronogit

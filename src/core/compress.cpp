#include "chronogit/core/compress.hpp"
#include "chronogit/core/error.hpp"
#include <cstring>
#include <algorithm>
#include <array>

namespace chronogit {

namespace {
constexpr uint8_t MAGIC[4] = {'C', 'G', 'Z', '1'};
constexpr size_t MAX_WINDOW = 65535;
constexpr size_t MIN_MATCH = 4;
constexpr size_t MAX_MATCH = 131; // 4 + 127
constexpr size_t HASH_SIZE = 8192;

inline uint16_t hash_bytes(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) ^ (static_cast<uint16_t>(p[1]) << 4) ^ (static_cast<uint16_t>(p[2]) << 8)) % HASH_SIZE;
}
} // namespace

std::vector<uint8_t> Compressor::compress(std::span<const uint8_t> data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() + 16);

    // Magic
    out.push_back(MAGIC[0]);
    out.push_back(MAGIC[1]);
    out.push_back(MAGIC[2]);
    out.push_back(MAGIC[3]);

    // Uncompressed size (uint32_t big-endian)
    uint32_t orig_size = static_cast<uint32_t>(data.size());
    out.push_back(static_cast<uint8_t>((orig_size >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((orig_size >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((orig_size >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(orig_size & 0xFF));

    if (data.empty()) {
        return out;
    }

    std::array<int32_t, HASH_SIZE> head;
    head.fill(-1);
    std::vector<int32_t> prev(data.size(), -1);

    size_t in_pos = 0;
    std::vector<uint8_t> lit_buf;
    lit_buf.reserve(128);

    auto flush_literals = [&]() {
        while (!lit_buf.empty()) {
            size_t count = std::min<size_t>(lit_buf.size(), 128);
            out.push_back(static_cast<uint8_t>(count - 1)); // 0x00..0x7F
            out.insert(out.end(), lit_buf.begin(), lit_buf.begin() + count);
            lit_buf.erase(lit_buf.begin(), lit_buf.begin() + count);
        }
    };

    while (in_pos < data.size()) {
        size_t best_len = 0;
        size_t best_offset = 0;

        if (in_pos + MIN_MATCH <= data.size()) {
            uint16_t h = hash_bytes(&data[in_pos]);
            int32_t candidate = head[h];
            size_t chain_len = 0;

            while (candidate != -1 && chain_len < 64) {
                size_t offset = in_pos - candidate;
                if (offset > MAX_WINDOW) break;

                size_t max_len = std::min(MAX_MATCH, data.size() - in_pos);
                size_t match_len = 0;
                while (match_len < max_len && data[candidate + match_len] == data[in_pos + match_len]) {
                    match_len++;
                }

                if (match_len >= MIN_MATCH && match_len > best_len) {
                    best_len = match_len;
                    best_offset = offset;
                    if (best_len == MAX_MATCH) break;
                }

                candidate = prev[candidate];
                chain_len++;
            }
        }

        if (best_len >= MIN_MATCH) {
            flush_literals();

            // Match token: 0x80 | (best_len - 4)
            out.push_back(static_cast<uint8_t>(0x80 | (best_len - MIN_MATCH)));
            out.push_back(static_cast<uint8_t>((best_offset >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(best_offset & 0xFF));

            // Update hash chains for matched range
            for (size_t i = 0; i < best_len; ++i) {
                if (in_pos + i + 2 < data.size()) {
                    uint16_t h = hash_bytes(&data[in_pos + i]);
                    prev[in_pos + i] = head[h];
                    head[h] = static_cast<int32_t>(in_pos + i);
                }
            }
            in_pos += best_len;
        } else {
            lit_buf.push_back(data[in_pos]);
            if (lit_buf.size() == 128) {
                flush_literals();
            }

            if (in_pos + 2 < data.size()) {
                uint16_t h = hash_bytes(&data[in_pos]);
                prev[in_pos] = head[h];
                head[h] = static_cast<int32_t>(in_pos);
            }
            in_pos++;
        }
    }

    flush_literals();
    return out;
}

std::vector<uint8_t> Compressor::decompress(std::span<const uint8_t> compressed_data) {
    if (compressed_data.size() < 8) {
        throw ObjectCorruptedException("unknown", "compressed stream too small");
    }

    if (compressed_data[0] != MAGIC[0] || compressed_data[1] != MAGIC[1] ||
        compressed_data[2] != MAGIC[2] || compressed_data[3] != MAGIC[3]) {
        throw ObjectCorruptedException("unknown", "invalid compression magic header");
    }

    uint32_t orig_size = (static_cast<uint32_t>(compressed_data[4]) << 24) |
                         (static_cast<uint32_t>(compressed_data[5]) << 16) |
                         (static_cast<uint32_t>(compressed_data[6]) << 8)  |
                         static_cast<uint32_t>(compressed_data[7]);

    std::vector<uint8_t> out;
    out.resize(orig_size);

    size_t in_pos = 8;
    size_t out_pos = 0;

    while (in_pos < compressed_data.size()) {
        uint8_t token = compressed_data[in_pos++];

        if (token & 0x80) {
            // Match
            size_t match_len = (token & 0x7F) + MIN_MATCH;
            if (in_pos + 2 > compressed_data.size()) {
                throw ObjectCorruptedException("unknown", "unexpected EOF reading match offset");
            }
            uint16_t offset = (static_cast<uint16_t>(compressed_data[in_pos]) << 8) |
                               static_cast<uint16_t>(compressed_data[in_pos + 1]);
            in_pos += 2;

            if (offset == 0 || offset > out_pos) {
                throw ObjectCorruptedException("unknown", "invalid match offset in compressed data");
            }
            if (out_pos + match_len > orig_size) {
                throw ObjectCorruptedException("unknown", "decompressed output size exceeds expected length");
            }

            for (size_t i = 0; i < match_len; ++i) {
                out[out_pos] = out[out_pos - offset];
                out_pos++;
            }
        } else {
            // Literal
            size_t lit_len = token + 1;
            if (in_pos + lit_len > compressed_data.size()) {
                throw ObjectCorruptedException("unknown", "unexpected EOF reading literal bytes");
            }
            if (out_pos + lit_len > orig_size) {
                throw ObjectCorruptedException("unknown", "decompressed output size exceeds expected length");
            }

            std::memcpy(&out[out_pos], &compressed_data[in_pos], lit_len);
            in_pos += lit_len;
            out_pos += lit_len;
        }
    }

    if (out_pos != orig_size) {
        throw ObjectCorruptedException("unknown", "decompressed size mismatch");
    }

    return out;
}

} // namespace chronogit

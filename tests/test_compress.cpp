#include "test_framework.hpp"
#include "chronogit/core/compress.hpp"
#include "chronogit/core/error.hpp"

using namespace chronogit;

TEST(CompressTest, EmptyDataRoundtrip) {
    std::vector<uint8_t> empty;
    auto compressed = Compressor::compress(empty);
    auto decompressed = Compressor::decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST(CompressTest, TextRoundtripAndCompression) {
    std::string text = "ChronoGit is a lightweight, high-performance version control engine.\n"
                       "ChronoGit is a lightweight, high-performance version control engine.\n"
                       "ChronoGit is a lightweight, high-performance version control engine.\n"
                       "Repeat repeat repeat repeat repeat repeat repeat repeat repeat!\n";
    std::vector<uint8_t> data(text.begin(), text.end());

    auto compressed = Compressor::compress(data);
    EXPECT_TRUE(compressed.size() < data.size()); // Verifies actual compression

    auto decompressed = Compressor::decompress(compressed);
    std::string result(decompressed.begin(), decompressed.end());
    EXPECT_EQ(result, text);
}

TEST(CompressTest, CorruptedHeaderThrows) {
    std::vector<uint8_t> bad_data = {0x00, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00};
    EXPECT_THROW(Compressor::decompress(bad_data), ObjectCorruptedException);
}

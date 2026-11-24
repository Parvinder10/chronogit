#include "test_framework.hpp"
#include "chronogit/core/sha256.hpp"

using namespace chronogit;

TEST(SHA256Test, EmptyString) {
    // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    ObjectId hash = SHA256::hash("");
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SHA256Test, StandardTestVectors) {
    // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    ObjectId hash_abc = SHA256::hash("abc");
    EXPECT_EQ(hash_abc, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Streaming update test
    SHA256 ctx;
    ctx.update("a");
    ctx.update("b");
    ctx.update("c");
    ObjectId hash_stream = ctx.finalize();
    EXPECT_EQ(hash_stream, hash_abc);
}

TEST(SHA256Test, Determinism) {
    std::string text = "ChronoGit content addressable storage test string 12345!";
    ObjectId hash1 = SHA256::hash(text);
    ObjectId hash2 = SHA256::hash(text);
    EXPECT_EQ(hash1, hash2);
    EXPECT_TRUE(is_valid_object_id(hash1));
}

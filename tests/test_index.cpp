#include "test_framework.hpp"
#include "chronogit/storage/index.hpp"
#include "chronogit/core/atomic_file.hpp"

#include <filesystem>

using namespace chronogit;

TEST(IndexTest, AddRemoveSaveLoad) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_index";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    Index index;
    IndexEntry e1{
        .path = "src/main.cpp",
        .id = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        .mode = FileMode::Regular,
        .file_size = 120,
        .mtime_sec = 1600000000,
        .mtime_nsec = 500
    };
    IndexEntry e2{
        .path = "README.md",
        .id = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        .mode = FileMode::Regular,
        .file_size = 45,
        .mtime_sec = 1600000010,
        .mtime_nsec = 200
    };

    index.add_or_update(e1);
    index.add_or_update(e2);
    EXPECT_EQ(index.size(), 2u);

    auto index_file = test_dir / "index";
    index.save(index_file);
    EXPECT_TRUE(AtomicFile::exists(index_file));

    // Load into fresh Index instance
    Index loaded;
    loaded.load(index_file);
    EXPECT_EQ(loaded.size(), 2u);

    auto found1 = loaded.find("src/main.cpp");
    ASSERT_TRUE(found1 != nullptr);
    EXPECT_EQ(found1->id, e1.id);
    EXPECT_EQ(found1->file_size, 120u);

    auto found2 = loaded.find("README.md");
    ASSERT_TRUE(found2 != nullptr);
    EXPECT_EQ(found2->id, e2.id);

    // Test removal
    EXPECT_TRUE(loaded.remove("README.md"));
    EXPECT_EQ(loaded.size(), 1u);
    EXPECT_TRUE(loaded.find("README.md") == nullptr);

    std::filesystem::remove_all(test_dir, ec);
}

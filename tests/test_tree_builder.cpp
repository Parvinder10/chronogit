#include "test_framework.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include "chronogit/storage/object_db.hpp"

#include <filesystem>

using namespace chronogit;

TEST(TreeBuilderTest, HierarchicalSubtreeBuilding) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_tree_builder";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    ObjectDb db(test_dir / "objects");

    Index index;
    // Two files in a nested directory structure:
    //   src/utils/math.cpp
    //   src/main.cpp
    //   README.md
    ObjectId id1 = "1111111111111111111111111111111111111111111111111111111111111111";
    ObjectId id2 = "2222222222222222222222222222222222222222222222222222222222222222";
    ObjectId id3 = "3333333333333333333333333333333333333333333333333333333333333333";

    index.add_or_update(IndexEntry{.path = "src/utils/math.cpp", .id = id1});
    index.add_or_update(IndexEntry{.path = "src/main.cpp", .id = id2});
    index.add_or_update(IndexEntry{.path = "README.md", .id = id3});

    ObjectId root_tree_id = TreeBuilder::build_tree_from_index(db, index);
    EXPECT_TRUE(is_valid_object_id(root_tree_id));

    // Verify root tree structure
    auto root_tree = db.read_tree(root_tree_id);
    ASSERT_TRUE(root_tree != nullptr);
    EXPECT_EQ(root_tree->entries().size(), 2u); // README.md (blob), src (tree)

    auto readme_entry = root_tree->find_entry("README.md");
    ASSERT_TRUE(readme_entry.has_value());
    EXPECT_EQ(readme_entry->type, ObjectType::Blob);
    EXPECT_EQ(readme_entry->id, id3);

    auto src_entry = root_tree->find_entry("src");
    ASSERT_TRUE(src_entry.has_value());
    EXPECT_EQ(src_entry->type, ObjectType::Tree);

    // Verify subtree src
    auto src_tree = db.read_tree(src_entry->id);
    ASSERT_TRUE(src_tree != nullptr);
    EXPECT_EQ(src_tree->entries().size(), 2u); // main.cpp (blob), utils (tree)

    // Verify flattening tree back to map
    auto flat = TreeBuilder::flatten_tree(db, root_tree_id);
    EXPECT_EQ(flat.size(), 3u);
    EXPECT_EQ(flat["src/utils/math.cpp"].id, id1);
    EXPECT_EQ(flat["src/main.cpp"].id, id2);
    EXPECT_EQ(flat["README.md"].id, id3);

    std::filesystem::remove_all(test_dir, ec);
}

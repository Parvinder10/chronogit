#include "test_framework.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/blob.hpp"
#include "chronogit/storage/tree.hpp"
#include "chronogit/storage/commit.hpp"
#include "chronogit/core/atomic_file.hpp"

#include <filesystem>

using namespace chronogit;

namespace {
std::filesystem::path get_test_dir(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("chronogit_test_" + name);
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    return path;
}
} // namespace

TEST(ObjectDbTest, BlobWriteReadAndDeduplication) {
    auto test_dir = get_test_dir("obj_blob");
    ObjectDb db(test_dir / "objects");

    std::string content = "Hello, ChronoGit World!";
    Blob blob1(content);
    Blob blob2(content); // identical content

    ObjectId id1 = db.write_object(blob1);
    ObjectId id2 = db.write_object(blob2);

    // Deduplication check: identical content must produce identical hash
    EXPECT_EQ(id1, id2);

    // Verify object list has exactly 1 object
    auto all_objs = db.list_all_objects();
    EXPECT_EQ(all_objs.size(), 1u);
    EXPECT_EQ(all_objs[0], id1);

    // Read back and verify content
    auto read_blob = db.read_blob(id1);
    ASSERT_TRUE(read_blob != nullptr);
    EXPECT_EQ(read_blob->as_string_view(), content);

    // Verify integrity
    EXPECT_TRUE(db.verify_object(id1));

    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

TEST(ObjectDbTest, TreeAndCommitObjects) {
    auto test_dir = get_test_dir("obj_tree_commit");
    ObjectDb db(test_dir / "objects");

    Blob blob("print('hello world')\n");
    ObjectId blob_id = db.write_object(blob);

    Tree tree;
    tree.add_entry(TreeEntry{
        .mode = FileMode::Regular,
        .type = ObjectType::Blob,
        .id = blob_id,
        .name = "main.py"
    });
    ObjectId tree_id = db.write_object(tree);

    Signature author = Signature::now("Alice", "alice@example.com");
    Commit commit(tree_id, {}, author, author, "Initial commit");
    ObjectId commit_id = db.write_object(commit);

    // Read tree
    auto read_tree = db.read_tree(tree_id);
    ASSERT_TRUE(read_tree != nullptr);
    EXPECT_EQ(read_tree->entries().size(), 1u);
    EXPECT_EQ(read_tree->entries()[0].name, "main.py");
    EXPECT_EQ(read_tree->entries()[0].id, blob_id);

    // Read commit
    auto read_commit = db.read_commit(commit_id);
    ASSERT_TRUE(read_commit != nullptr);
    EXPECT_EQ(read_commit->tree_id(), tree_id);
    EXPECT_EQ(read_commit->message(), "Initial commit");
    EXPECT_EQ(read_commit->author().name, "Alice");

    // All objects must pass verification
    EXPECT_TRUE(db.verify_object(blob_id));
    EXPECT_TRUE(db.verify_object(tree_id));
    EXPECT_TRUE(db.verify_object(commit_id));

    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

TEST(ObjectDbTest, CorruptionDetection) {
    auto test_dir = get_test_dir("obj_corrupt");
    ObjectDb db(test_dir / "objects");

    Blob blob("Important uncorrupted data");
    ObjectId id = db.write_object(blob);
    EXPECT_TRUE(db.verify_object(id));

    // Intentionally corrupt the file on disk by flipping bytes
    auto path = db.object_path(id);
    auto raw_bytes = AtomicFile::read_binary(path);
    ASSERT_TRUE(!raw_bytes.empty());
    raw_bytes.back() ^= 0xFF; // Flip bits
    AtomicFile::write_binary(path, raw_bytes);

    // Database verification must catch the corruption!
    EXPECT_FALSE(db.verify_object(id));

    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

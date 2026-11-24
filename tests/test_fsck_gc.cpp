#include "test_framework.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/maintenance/fsck.hpp"
#include "chronogit/maintenance/gc.hpp"
#include "chronogit/core/atomic_file.hpp"

#include <filesystem>

using namespace chronogit;

TEST(FsckGcTest, CleanRepoAndGarbageCollection) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_fsck_gc";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    auto repo = Repository::init(test_dir, "main");

    // Create file and commit
    AtomicFile::write_text(test_dir / "file1.txt", "Hello World\n");
    repo.add({"file1.txt"});
    ObjectId c1 = repo.commit("Commit 1");

    // Add another commit
    AtomicFile::write_text(test_dir / "file2.txt", "Another file\n");
    repo.add({"file2.txt"});
    ObjectId c2 = repo.commit("Commit 2");

    // Run FSCK: should be completely healthy
    auto fsck_res = Fsck::check_repository(repo);
    EXPECT_TRUE(fsck_res.is_healthy());
    EXPECT_EQ(fsck_res.corrupted_objects.size(), 0u);
    EXPECT_EQ(fsck_res.missing_objects.size(), 0u);
    EXPECT_EQ(fsck_res.ref_errors.size(), 0u);
    EXPECT_EQ(fsck_res.dangling_objects.size(), 0u);

    // Intentionally create an unreachable orphan blob in the object db
    Blob orphan_blob("I am unreachable garbage data");
    ObjectId orphan_id = repo.db().write_object(orphan_blob);
    EXPECT_TRUE(repo.db().has_object(orphan_id));

    // FSCK should detect 1 dangling object
    auto fsck_dangling = Fsck::check_repository(repo, true);
    EXPECT_EQ(fsck_dangling.dangling_objects.size(), 1u);
    EXPECT_EQ(fsck_dangling.dangling_objects[0], orphan_id);

    // Run GC dry-run
    auto gc_dry = GarbageCollector::run(repo, true);
    EXPECT_TRUE(gc_dry.dry_run);
    EXPECT_EQ(gc_dry.unreachable_count, 1u);
    EXPECT_TRUE(repo.db().has_object(orphan_id)); // Still there after dry run!

    // Run actual GC
    auto gc_actual = GarbageCollector::run(repo, false);
    EXPECT_FALSE(gc_actual.dry_run);
    EXPECT_EQ(gc_actual.unreachable_count, 1u);
    EXPECT_FALSE(repo.db().has_object(orphan_id)); // Deleted!

    // FSCK should now have 0 dangling objects again
    auto fsck_clean = Fsck::check_repository(repo, true);
    EXPECT_TRUE(fsck_clean.is_healthy());
    EXPECT_EQ(fsck_clean.dangling_objects.size(), 0u);

    std::filesystem::remove_all(test_dir, ec);
}

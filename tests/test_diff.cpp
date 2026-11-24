#include "test_framework.hpp"
#include "chronogit/diff/myers_diff.hpp"
#include "chronogit/diff/diff_engine.hpp"

using namespace chronogit;

TEST(DiffTest, MyersLineDiff) {
    std::string text_a = "apple\nbanana\ncherry\ndate\n";
    std::string text_b = "apple\nblueberry\ncherry\nfig\n";

    auto lines_a = MyersDiff::split_lines(text_a);
    auto lines_b = MyersDiff::split_lines(text_b);

    auto diff_lines = MyersDiff::compute_diff(lines_a, lines_b);
    auto hunks = MyersDiff::make_hunks(diff_lines, 1);

    ASSERT_EQ(hunks.size(), 1u);
    auto hunk = hunks[0];
    EXPECT_TRUE(hunk.lines.size() >= 4);

    // Verify unified diff formatting
    auto file_diff = DiffEngine::diff_content("test.txt", text_a, text_b);
    std::string unified = DiffEngine::format_unified_diff({file_diff}, false);

    EXPECT_TRUE(unified.find("-banana") != std::string::npos);
    EXPECT_TRUE(unified.find("+blueberry") != std::string::npos);
    EXPECT_TRUE(unified.find("-date") != std::string::npos);
    EXPECT_TRUE(unified.find("+fig") != std::string::npos);
    EXPECT_TRUE(unified.find("@@") != std::string::npos);
}

TEST(DiffTest, IdenticalFilesHaveNoHunks) {
    std::string text = "line 1\nline 2\nline 3\n";
    auto file_diff = DiffEngine::diff_content("same.txt", text, text);
    EXPECT_TRUE(file_diff.hunks.empty());
}

TEST(DiffTest, NewFileDiff) {
    std::string text = "first line\nsecond line\n";
    auto file_diff = DiffEngine::diff_content("new.txt", "", text, ChangeType::Added);
    std::string unified = DiffEngine::format_unified_diff({file_diff}, false);

    EXPECT_TRUE(unified.find("--- /dev/null") != std::string::npos);
    EXPECT_TRUE(unified.find("+++ b/new.txt") != std::string::npos);
    EXPECT_TRUE(unified.find("+first line") != std::string::npos);
}

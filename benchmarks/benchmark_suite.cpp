#include "chronogit/repo/repository.hpp"
#include "chronogit/merge/merge_engine.hpp"
#include "chronogit/diff/diff_engine.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/cli/formatter.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <random>
#include <fstream>
#include <sstream>

using namespace chronogit;

struct BenchmarkResult {
    int file_count = 0;
    uint64_t raw_uncompressed_bytes = 0;
    uint64_t full_copy_bytes = 0;
    uint64_t cas_storage_bytes = 0;
    double deduplication_savings_pct = 0.0;
    double initial_commit_ms = 0.0;
    double incremental_commit_ms = 0.0;
    double diff_ms = 0.0;
    double checkout_ms = 0.0;
    double merge_ms = 0.0;
    double history_traversal_ms = 0.0;
};

uint64_t calculate_dir_size(const std::filesystem::path& dir) {
    uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file()) {
            total += entry.file_size(ec);
        }
    }
    return total;
}

std::string generate_file_content(int file_id, int duplicate_group, int lines = 20) {
    std::ostringstream oss;
    oss << "// ChronoGit Benchmark Generated File: " << file_id << "\n";
    oss << "// Duplicate Group: " << duplicate_group << "\n";
    for (int i = 0; i < lines; ++i) {
        oss << "static const int CONSTANT_" << duplicate_group << "_" << i << " = " << (i * 17 + duplicate_group) << ";\n";
    }
    return oss.str();
}

BenchmarkResult run_scale_benchmark(int file_count) {
    BenchmarkResult res;
    res.file_count = file_count;

    auto test_root = std::filesystem::temp_directory_path() / ("chronogit_bench_" + std::to_string(file_count));
    std::error_code ec;
    std::filesystem::remove_all(test_root, ec);
    std::filesystem::create_directories(test_root, ec);

    auto repo = Repository::init(test_root, "main");

    // 1. Generate files with realistic deduplication opportunities (every 5 files share identical content)
    std::vector<std::string> file_paths;
    uint64_t raw_single_snapshot_bytes = 0;

    for (int i = 0; i < file_count; ++i) {
        int dup_group = i / 5; // 5 files share the same duplicate group content!
        std::string content = generate_file_content(i, dup_group, 30);
        raw_single_snapshot_bytes += content.size();

        std::string rel_path = "modules/sub_" + std::to_string(i / 50) + "/file_" + std::to_string(i) + ".cpp";
        AtomicFile::write_text(test_root / rel_path, content);
        file_paths.push_back(rel_path);
    }
    res.raw_uncompressed_bytes = raw_single_snapshot_bytes;

    // 2. Initial commit benchmark
    auto t0 = std::chrono::high_resolution_clock::now();
    repo.add({"modules"});
    ObjectId c1 = repo.commit("Initial large snapshot (" + std::to_string(file_count) + " files)");
    auto t1 = std::chrono::high_resolution_clock::now();
    res.initial_commit_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 3. Create a feature branch
    repo.create_branch("feature");

    // 4. Incremental commit: modify 10% of files
    int modified_count = std::max(1, file_count / 10);
    std::vector<std::string> modified_paths;
    for (int i = 0; i < modified_count; ++i) {
        std::string rel_path = file_paths[i];
        std::string extra = "\n// Incremental edit for file " + std::to_string(i) + "\nvoid update_" + std::to_string(i) + "() {}\n";
        std::string existing = AtomicFile::read_text(test_root / rel_path);
        AtomicFile::write_text(test_root / rel_path, existing + extra);
        modified_paths.push_back(rel_path);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    repo.add(modified_paths);
    ObjectId c2 = repo.commit("Incremental update on main");
    auto t3 = std::chrono::high_resolution_clock::now();
    res.incremental_commit_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    // 5. Diff benchmark
    auto t4 = std::chrono::high_resolution_clock::now();
    auto diffs = DiffEngine::diff_commits(repo.db(), c1, c2);
    auto t5 = std::chrono::high_resolution_clock::now();
    res.diff_ms = std::chrono::duration<double, std::milli>(t5 - t4).count();

    // 6. Checkout benchmark
    auto t6 = std::chrono::high_resolution_clock::now();
    repo.checkout("feature");
    auto t7 = std::chrono::high_resolution_clock::now();
    res.checkout_ms = std::chrono::duration<double, std::milli>(t7 - t6).count();

    // 7. Make parallel change on feature branch for merge benchmark
    std::string feat_path = file_paths.back();
    std::string feat_content = AtomicFile::read_text(test_root / feat_path) + "\n// Parallel feature edit\n";
    AtomicFile::write_text(test_root / feat_path, feat_content);
    repo.add({feat_path});
    ObjectId c_feat = repo.commit("Feature commit on parallel branch");

    // Switch back to main and merge feature
    repo.checkout("main");
    MergeEngine engine(repo);

    auto t8 = std::chrono::high_resolution_clock::now();
    auto merge_res = engine.merge("feature");
    auto t9 = std::chrono::high_resolution_clock::now();
    res.merge_ms = std::chrono::duration<double, std::milli>(t9 - t8).count();

    // 8. History traversal benchmark
    auto t10 = std::chrono::high_resolution_clock::now();
    auto hist = repo.get_commit_history(merge_res.commit_id);
    auto t11 = std::chrono::high_resolution_clock::now();
    res.history_traversal_ms = std::chrono::duration<double, std::milli>(t11 - t10).count();

    // 9. Storage and Deduplication analysis
    // Full copy snapshots: 3 commits * raw single snapshot size
    res.full_copy_bytes = raw_single_snapshot_bytes * 3;
    res.cas_storage_bytes = calculate_dir_size(repo.git_dir() / "objects");

    if (res.full_copy_bytes > 0) {
        res.deduplication_savings_pct = 100.0 * (1.0 - static_cast<double>(res.cas_storage_bytes) / static_cast<double>(res.full_copy_bytes));
    }

    std::filesystem::remove_all(test_root, ec);
    return res;
}

int main() {
    std::cout << "\033[1m========================================================================\033[0m\n";
    std::cout << "\033[1m           ChronoGit Storage Engine & System Benchmark Suite            \033[0m\n";
    std::cout << "\033[1m========================================================================\033[0m\n\n";

    std::vector<int> scales = {100, 500, 1000};
    std::vector<BenchmarkResult> results;

    for (int scale : scales) {
        std::cout << ">>> Running benchmark for scale: " << scale << " files...\n";
        auto res = run_scale_benchmark(scale);
        results.push_back(res);
        std::cout << "    Done (" << std::fixed << std::setprecision(2) << res.initial_commit_ms << " ms initial commit)\n\n";
    }

    // Print Results Table
    std::cout << "\033[1m" << std::left
              << std::setw(12) << "Files"
              << std::setw(16) << "Init Commit"
              << std::setw(16) << "Incr Commit"
              << std::setw(14) << "Diff"
              << std::setw(14) << "Checkout"
              << std::setw(14) << "3-Way Merge"
              << std::setw(16) << "History Walk"
              << "\033[0m\n";
    std::cout << std::string(102, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(12) << r.file_count
                  << std::setw(16) << (std::to_string(static_cast<int>(r.initial_commit_ms)) + " ms")
                  << std::setw(16) << (std::to_string(static_cast<int>(r.incremental_commit_ms)) + " ms")
                  << std::setw(14) << (std::to_string(static_cast<int>(r.diff_ms)) + " ms")
                  << std::setw(14) << (std::to_string(static_cast<int>(r.checkout_ms)) + " ms")
                  << std::setw(14) << (std::to_string(static_cast<int>(r.merge_ms)) + " ms")
                  << std::setw(16) << (std::to_string(static_cast<int>(r.history_traversal_ms * 1000)) + " us")
                  << "\n";
    }

    std::cout << "\n\033[1m" << std::left
              << std::setw(12) << "Files"
              << std::setw(20) << "Full Copy (3x)"
              << std::setw(20) << "ChronoGit CAS"
              << std::setw(20) << "Deduplication & Compression Savings"
              << "\033[0m\n";
    std::cout << std::string(72, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(12) << r.file_count
                  << std::setw(20) << Formatter::format_size(r.full_copy_bytes)
                  << std::setw(20) << Formatter::format_size(r.cas_storage_bytes)
                  << "\033[32m" << std::fixed << std::setprecision(1) << r.deduplication_savings_pct << "% saved\033[0m\n";
    }

    // Write JSON and Markdown
    std::ofstream jout("benchmarks/benchmark_results.json");
    jout << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        jout << "  {\n"
             << "    \"file_count\": " << r.file_count << ",\n"
             << "    \"raw_uncompressed_bytes\": " << r.raw_uncompressed_bytes << ",\n"
             << "    \"full_copy_bytes\": " << r.full_copy_bytes << ",\n"
             << "    \"cas_storage_bytes\": " << r.cas_storage_bytes << ",\n"
             << "    \"deduplication_savings_pct\": " << r.deduplication_savings_pct << ",\n"
             << "    \"initial_commit_ms\": " << r.initial_commit_ms << ",\n"
             << "    \"incremental_commit_ms\": " << r.incremental_commit_ms << ",\n"
             << "    \"diff_ms\": " << r.diff_ms << ",\n"
             << "    \"checkout_ms\": " << r.checkout_ms << ",\n"
             << "    \"merge_ms\": " << r.merge_ms << ",\n"
             << "    \"history_traversal_ms\": " << r.history_traversal_ms << "\n"
             << "  }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    jout << "]\n";

    std::cout << "\nSaved benchmark results to benchmarks/benchmark_results.json\n";
    return 0;
}

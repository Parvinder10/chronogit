#include "chronogit/cli/cli_app.hpp"
#include "chronogit/cli/formatter.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/diff/diff_engine.hpp"
#include "chronogit/merge/merge_engine.hpp"
#include "chronogit/maintenance/fsck.hpp"
#include "chronogit/maintenance/gc.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include "chronogit/core/error.hpp"
#include "chronogit/core/atomic_file.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>

namespace chronogit {

void CliApp::print_help() const {
    std::cout << Formatter::bold("ChronoGit — Lightweight Version Control Engine") << "\n\n"
              << "Usage: chronogit <command> [<args>]\n\n"
              << "Core Commands:\n"
              << "  " << Formatter::cyan("init") << " [-b <branch>] [<dir>]      Create an empty repository\n"
              << "  " << Formatter::cyan("add") << " <pathspec>...               Add file contents to the staging area\n"
              << "  " << Formatter::cyan("commit") << " -m <msg> [--author ...]  Record changes to the repository\n"
              << "  " << Formatter::cyan("status") << "                          Show the working tree status\n"
              << "  " << Formatter::cyan("log") << " [--oneline] [--graph] [-n N] Show commit logs\n"
              << "  " << Formatter::cyan("branch") << " [<name>] [-d <name>]     List, create, or delete branches\n"
              << "  " << Formatter::cyan("checkout") << " [-b <name>] <target>   Switch branches or restore working tree files\n"
              << "  " << Formatter::cyan("diff") << " [--staged] [<c1>] [<c2>]   Show changes between commits, commit and working tree, etc\n"
              << "  " << Formatter::cyan("merge") << " [--abort] <branch>        Join two development histories together\n"
              << "  " << Formatter::cyan("show") << " [<object>]                 Show various types of objects (commits, diffs, trees)\n"
              << "  " << Formatter::cyan("fsck") << " [--full]                   Verify the connectivity and validity of objects\n"
              << "  " << Formatter::cyan("gc") << " [--dry-run]                  Cleanup unnecessary files and optimize the local repository\n"
              << "  " << Formatter::cyan("export-graph") << " [<output.json>]    Export commit DAG for interactive visualization\n\n";
}

int CliApp::run(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (command == "-h" || command == "--help" || command == "help") {
        print_help();
        return 0;
    }

    try {
        if (command == "init") return cmd_init(args);
        if (command == "add") return cmd_add(args);
        if (command == "commit") return cmd_commit(args);
        if (command == "status") return cmd_status(args);
        if (command == "log") return cmd_log(args);
        if (command == "branch") return cmd_branch(args);
        if (command == "checkout") return cmd_checkout(args);
        if (command == "diff") return cmd_diff(args);
        if (command == "merge") return cmd_merge(args);
        if (command == "show") return cmd_show(args);
        if (command == "fsck") return cmd_fsck(args);
        if (command == "gc") return cmd_gc(args);
        if (command == "export-graph") return cmd_export_graph(args);

        std::cerr << "chronogit: '" << command << "' is not a chronogit command. See 'chronogit --help'.\n";
        return 1;
    } catch (const ChronoGitException& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "fatal: unhandled exception: " << e.what() << "\n";
        return 2;
    }
}

int CliApp::cmd_init(const std::vector<std::string>& args) {
    std::string default_branch = "main";
    std::filesystem::path target_dir = std::filesystem::current_path();

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-b" && i + 1 < args.size()) {
            default_branch = args[++i];
        } else if (args[i][0] != '-') {
            target_dir = args[i];
        }
    }

    auto repo = Repository::init(target_dir, default_branch);
    std::cout << "Initialized empty ChronoGit repository in " << repo.git_dir().string() << "/\n";
    return 0;
}

int CliApp::cmd_add(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Nothing specified, nothing added.\nMaybe you wanted to say 'chronogit add .'?\n";
        return 1;
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    repo.add(args);
    return 0;
}

int CliApp::cmd_commit(const std::vector<std::string>& args) {
    std::string message;
    std::string author_name;
    std::string author_email;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            message = args[++i];
        } else if (args[i] == "--author" && i + 2 < args.size()) {
            author_name = args[++i];
            author_email = args[++i];
        }
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    MergeEngine merge_engine(repo);

    std::vector<ObjectId> extra_parents;
    if (merge_engine.is_in_merge_state()) {
        auto mhead = merge_engine.get_merge_head();
        if (mhead) {
            extra_parents.push_back(*mhead);
        }
    }

    if (message.empty() && merge_engine.is_in_merge_state()) {
        auto msg_path = repo.git_dir() / "MERGE_MSG";
        if (AtomicFile::exists(msg_path)) {
            message = AtomicFile::read_text(msg_path);
        }
    }

    if (message.empty()) {
        std::cerr << "error: commit message must be specified via -m\n";
        return 1;
    }

    ObjectId commit_id = repo.commit(message, author_name, author_email, extra_parents);

    if (merge_engine.is_in_merge_state()) {
        merge_engine.clear_merge_state();
    }

    auto branch = repo.branch_manager().get_current_branch().value_or("detached HEAD");
    std::cout << "[" << branch << " " << commit_id.substr(0, 7) << "] " << message.substr(0, message.find('\n')) << "\n";
    return 0;
}

int CliApp::cmd_status(const std::vector<std::string>&) {
    auto repo = Repository::find_repo(std::filesystem::current_path());
    auto status = repo.status();

    if (status.is_detached) {
        std::cout << "HEAD detached at " << Formatter::yellow(status.head_commit.substr(0, 7)) << "\n";
    } else {
        std::cout << "On branch " << Formatter::bold(status.branch_name) << "\n";
    }

    MergeEngine merge_engine(repo);
    if (merge_engine.is_in_merge_state()) {
        std::cout << Formatter::yellow("You have unmerged paths. (fix conflicts and run 'chronogit commit')\n")
                  << Formatter::yellow("  (use 'chronogit merge --abort' to abort the merge)\n\n");
    }

    if (status.head_commit.empty()) {
        std::cout << "\nNo commits yet\n";
    }

    bool has_staged = !status.staged_new.empty() || !status.staged_modified.empty() || !status.staged_deleted.empty();
    if (has_staged) {
        std::cout << "\nChanges to be committed:\n"
                  << "  (use \"chronogit checkout <file>...\" to unstage)\n";
        for (const auto& f : status.staged_new) {
            std::cout << "        " << Formatter::green("new file:   " + f) << "\n";
        }
        for (const auto& f : status.staged_modified) {
            std::cout << "        " << Formatter::green("modified:   " + f) << "\n";
        }
        for (const auto& f : status.staged_deleted) {
            std::cout << "        " << Formatter::green("deleted:    " + f) << "\n";
        }
    }

    bool has_unstaged = !status.unstaged_modified.empty() || !status.unstaged_deleted.empty();
    if (has_unstaged) {
        std::cout << "\nChanges not staged for commit:\n"
                  << "  (use \"chronogit add <file>...\" to update what will be committed)\n";
        for (const auto& f : status.unstaged_modified) {
            std::cout << "        " << Formatter::red("modified:   " + f) << "\n";
        }
        for (const auto& f : status.unstaged_deleted) {
            std::cout << "        " << Formatter::red("deleted:    " + f) << "\n";
        }
    }

    if (!status.untracked.empty()) {
        std::cout << "\nUntracked files:\n"
                  << "  (use \"chronogit add <file>...\" to include in what will be committed)\n";
        for (const auto& f : status.untracked) {
            std::cout << "        " << Formatter::red(f) << "\n";
        }
    }

    if (status.is_clean()) {
        std::cout << "nothing to commit, working tree clean\n";
    }

    return 0;
}

int CliApp::cmd_log(const std::vector<std::string>& args) {
    bool oneline = false;
    bool graph = false;
    bool stat = false;
    bool json_output = false;
    size_t limit = 0;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--oneline") oneline = true;
        else if (args[i] == "--graph") graph = true;
        else if (args[i] == "--stat") stat = true;
        else if (args[i] == "--json") json_output = true;
        else if (args[i] == "-n" && i + 1 < args.size()) {
            limit = std::stoul(args[++i]);
        }
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    auto head_id = repo.branch_manager().get_head_commit_id();
    if (!head_id) {
        std::cerr << "fatal: your current branch does not have any commits yet\n";
        return 1;
    }

    auto history = repo.get_commit_history(*head_id, limit);

    // Collect branch head decorations
    std::map<ObjectId, std::vector<std::string>> decorations;
    for (const auto& b : repo.list_branches()) {
        decorations[b.commit_id].push_back(b.is_current ? ("HEAD -> " + b.name) : b.name);
    }

    if (json_output) {
        std::cout << "[\n";
        for (size_t i = 0; i < history.size(); ++i) {
            const auto& [id, commit] = history[i];
            std::cout << "  {\n"
                      << "    \"id\": \"" << id << "\",\n"
                      << "    \"tree_id\": \"" << commit.tree_id() << "\",\n"
                      << "    \"author\": \"" << commit.author().name << "\",\n"
                      << "    \"email\": \"" << commit.author().email << "\",\n"
                      << "    \"timestamp\": " << commit.author().timestamp << ",\n"
                      << "    \"message\": \"" << commit.message() << "\",\n"
                      << "    \"parents\": [";
            for (size_t p = 0; p < commit.parent_ids().size(); ++p) {
                std::cout << "\"" << commit.parent_ids()[p] << "\"" << (p + 1 < commit.parent_ids().size() ? ", " : "");
            }
            std::cout << "]\n  }" << (i + 1 < history.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
        return 0;
    }

    for (const auto& [id, commit] : history) {
        std::string decor_str;
        auto it = decorations.find(id);
        if (it != decorations.end()) {
            decor_str = " (" + Formatter::green(it->second[0]);
            for (size_t k = 1; k < it->second.size(); ++k) {
                decor_str += ", " + Formatter::green(it->second[k]);
            }
            decor_str += ")";
        }

        if (oneline) {
            std::string prefix = graph ? "* " : "";
            std::string first_line = commit.message().substr(0, commit.message().find('\n'));
            std::cout << prefix << Formatter::yellow(id.substr(0, 7)) << decor_str << " " << first_line << "\n";
        } else {
            std::string prefix = graph ? "* " : "";
            std::string continuation = graph ? "| " : "    ";

            std::cout << prefix << Formatter::yellow("commit " + id) << decor_str << "\n";
            if (commit.parent_ids().size() > 1) {
                std::cout << continuation << "Merge: " << commit.parent_ids()[0].substr(0, 7) << " "
                          << commit.parent_ids()[1].substr(0, 7) << "\n";
            }
            std::cout << continuation << "Author: " << commit.author().name << " <" << commit.author().email << ">\n";
            std::cout << continuation << "Date:   " << Formatter::format_timestamp(commit.author().timestamp, commit.author().tz_offset) << "\n\n";

            std::istringstream miss(commit.message());
            std::string mline;
            while (std::getline(miss, mline)) {
                std::cout << continuation << "    " << mline << "\n";
            }
            std::cout << "\n";

            if (stat && !commit.parent_ids().empty()) {
                auto diffs = DiffEngine::diff_commits(repo.db(), commit.parent_ids()[0], id);
                std::cout << DiffEngine::format_stat(diffs) << "\n";
            }
        }
    }

    return 0;
}

int CliApp::cmd_branch(const std::vector<std::string>& args) {
    auto repo = Repository::find_repo(std::filesystem::current_path());

    if (args.empty()) {
        auto branches = repo.list_branches();
        for (const auto& b : branches) {
            if (b.is_current) {
                std::cout << "* " << Formatter::green(b.name) << "\n";
            } else {
                std::cout << "  " << b.name << "\n";
            }
        }
        return 0;
    }

    if (args[0] == "-d") {
        if (args.size() < 2) {
            std::cerr << "fatal: branch name required for -d\n";
            return 1;
        }
        repo.delete_branch(args[1]);
        std::cout << "Deleted branch " << args[1] << ".\n";
        return 0;
    }

    repo.create_branch(args[0]);
    return 0;
}

int CliApp::cmd_checkout(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "fatal: you must specify a branch or commit to checkout\n";
        return 1;
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());

    bool force = false;
    std::string new_branch;
    std::string target;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-f" || args[i] == "--force") {
            force = true;
        } else if (args[i] == "-b" && i + 1 < args.size()) {
            new_branch = args[++i];
        } else if (args[i][0] != '-') {
            target = args[i];
        }
    }

    if (!new_branch.empty()) {
        // Create new branch at target (or HEAD if no target specified) and checkout
        auto head_id = repo.branch_manager().get_head_commit_id();
        ObjectId base_commit = target.empty() ? (head_id ? *head_id : "") : target;
        if (base_commit.empty()) {
            throw ChronoGitException("fatal: cannot create branch from empty reference");
        }
        repo.create_branch(new_branch);
        repo.checkout(new_branch, force);
        std::cout << "Switched to a new branch '" << new_branch << "'\n";
        return 0;
    }

    repo.checkout(target, force);
    if (repo.branch_manager().branch_exists(target)) {
        std::cout << "Switched to branch '" << target << "'\n";
    } else {
        std::cout << "Note: switching to '" << target.substr(0, 7) << "' (detached HEAD)\n";
    }
    return 0;
}

int CliApp::cmd_diff(const std::vector<std::string>& args) {
    auto repo = Repository::find_repo(std::filesystem::current_path());
    bool staged = false;
    std::vector<std::string> positional;

    for (const auto& a : args) {
        if (a == "--staged" || a == "--cached") staged = true;
        else if (a[0] != '-') positional.push_back(a);
    }

    std::vector<FileDiff> diffs;

    if (positional.size() == 2) {
        // Diff between two commits
        diffs = DiffEngine::diff_commits(repo.db(), positional[0], positional[1]);
    } else if (positional.size() == 1) {
        // Diff commit vs working tree
        auto commit = repo.db().read_commit(positional[0]);
        auto commit_files = TreeBuilder::flatten_tree(repo.db(), commit->tree_id());
        auto work_files = repo.scanner().scan_workspace();

        std::set<std::string> all_p;
        for (const auto& [p, _] : commit_files) all_p.insert(p);
        for (const auto& [p, _] : work_files) all_p.insert(p);

        for (const auto& p : all_p) {
            auto it_c = commit_files.find(p);
            auto it_w = work_files.find(p);

            if (it_c != commit_files.end() && it_w == work_files.end()) {
                auto blob = repo.db().read_blob(it_c->second.id);
                diffs.push_back(DiffEngine::diff_content(p, blob->as_string_view(), "", ChangeType::Deleted));
            } else if (it_c == commit_files.end() && it_w != work_files.end()) {
                std::string txt = AtomicFile::read_text(repo.work_dir() / p);
                diffs.push_back(DiffEngine::diff_content(p, "", txt, ChangeType::Added));
            } else if (it_c != commit_files.end() && it_w != work_files.end()) {
                std::string txt = AtomicFile::read_text(repo.work_dir() / p);
                auto blob = repo.db().read_blob(it_c->second.id);
                if (txt != blob->as_string_view()) {
                    diffs.push_back(DiffEngine::diff_content(p, blob->as_string_view(), txt, ChangeType::Modified));
                }
            }
        }
    } else if (staged) {
        diffs = DiffEngine::diff_head_to_index(repo);
    } else {
        diffs = DiffEngine::diff_index_to_workdir(repo);
    }

    std::cout << DiffEngine::format_unified_diff(diffs, Formatter::is_color_enabled());
    return 0;
}

int CliApp::cmd_merge(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "fatal: branch name required for merge\n";
        return 1;
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    MergeEngine engine(repo);

    if (args[0] == "--abort") {
        engine.abort_merge();
        std::cout << "Merge aborted successfully.\n";
        return 0;
    }

    auto result = engine.merge(args[0]);

    if (result.status == MergeResultStatus::AlreadyUpToDate) {
        std::cout << "Already up to date.\n";
        return 0;
    }

    if (result.status == MergeResultStatus::FastForward) {
        std::cout << "Updating " << repo.branch_manager().get_head_commit_id()->substr(0, 7) << "..\n";
        std::cout << "Fast-forward\n";
        return 0;
    }

    if (result.status == MergeResultStatus::Conflict) {
        std::cout << Formatter::yellow("CONFLICT (content): Merge conflict in ") << "\n";
        for (const auto& cf : result.conflict_files) {
            std::cout << "  " << Formatter::red(cf) << "\n";
        }
        std::cout << "Automatic merge failed; fix conflicts and then commit the result.\n";
        return 1;
    }

    std::cout << "Merge made by the 'three-way' strategy.\n";
    return 0;
}

int CliApp::cmd_show(const std::vector<std::string>& args) {
    auto repo = Repository::find_repo(std::filesystem::current_path());
    ObjectId target_id;

    if (args.empty()) {
        auto hid = repo.branch_manager().get_head_commit_id();
        if (!hid) {
            std::cerr << "fatal: your current branch does not have any commits yet\n";
            return 1;
        }
        target_id = *hid;
    } else {
        if (repo.branch_manager().branch_exists(args[0])) {
            target_id = *repo.branch_manager().get_branch_commit_id(args[0]);
        } else {
            target_id = args[0];
        }
    }

    auto [type, payload] = repo.db().read_raw(target_id);

    if (type == ObjectType::Commit) {
        auto commit = Commit::deserialize(payload);
        std::cout << Formatter::yellow("commit " + target_id) << "\n";
        std::cout << "Author: " << commit->author().name << " <" << commit->author().email << ">\n";
        std::cout << "Date:   " << Formatter::format_timestamp(commit->author().timestamp, commit->author().tz_offset) << "\n\n";

        std::istringstream miss(commit->message());
        std::string mline;
        while (std::getline(miss, mline)) {
            std::cout << "    " << mline << "\n";
        }
        std::cout << "\n";

        if (!commit->parent_ids().empty()) {
            auto diffs = DiffEngine::diff_commits(repo.db(), commit->parent_ids()[0], target_id);
            std::cout << DiffEngine::format_unified_diff(diffs, Formatter::is_color_enabled());
        }
    } else if (type == ObjectType::Tree) {
        auto tree = Tree::deserialize(payload);
        for (const auto& entry : tree->entries()) {
            std::cout << file_mode_to_octal_string(entry.mode) << " "
                      << object_type_to_string(entry.type) << " "
                      << entry.id << "    "
                      << entry.name << "\n";
        }
    } else if (type == ObjectType::Blob) {
        auto blob = Blob::deserialize(payload);
        std::cout << blob->as_string_view() << "\n";
    }

    return 0;
}

int CliApp::cmd_fsck(const std::vector<std::string>&) {
    auto repo = Repository::find_repo(std::filesystem::current_path());
    std::cout << "Checking object directories...\n";
    auto result = Fsck::check_repository(repo);

    std::cout << "Checked " << result.total_objects << " objects ("
              << result.valid_objects << " valid)\n";

    if (!result.corrupted_objects.empty()) {
        std::cout << Formatter::red("\nCorrupted objects found:\n");
        for (const auto& [id, err] : result.corrupted_objects) {
            std::cout << "  corrupted " << id << ": " << err << "\n";
        }
    }

    if (!result.missing_objects.empty()) {
        std::cout << Formatter::red("\nMissing objects referenced:\n");
        for (const auto& [id, err] : result.missing_objects) {
            std::cout << "  missing " << id << " (" << err << ")\n";
        }
    }

    if (!result.ref_errors.empty()) {
        std::cout << Formatter::red("\nReference integrity errors:\n");
        for (const auto& err : result.ref_errors) {
            std::cout << "  " << err << "\n";
        }
    }

    if (!result.dangling_objects.empty()) {
        std::cout << Formatter::dim("\nDangling objects (" + std::to_string(result.dangling_objects.size()) + "):\n");
        for (const auto& id : result.dangling_objects) {
            std::cout << "  dangling " << id << "\n";
        }
    }

    if (result.is_healthy()) {
        std::cout << Formatter::green("\nRepository integrity verified: clean.\n");
        return 0;
    } else {
        std::cerr << Formatter::red("\nRepository integrity check failed!\n");
        return 1;
    }
}

int CliApp::cmd_gc(const std::vector<std::string>& args) {
    bool dry_run = false;
    for (const auto& a : args) {
        if (a == "--dry-run") dry_run = true;
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    auto report = GarbageCollector::run(repo, dry_run);

    if (dry_run) {
        std::cout << "[Dry-run] Found " << report.unreachable_count << " unreachable object(s).\n"
                  << "Would free: " << Formatter::format_size(report.bytes_freed) << "\n"
                  << "Retained: " << report.retained_count << " objects\n";
    } else {
        std::cout << "Removed " << report.unreachable_count << " unreachable object(s).\n"
                  << "Space freed: " << Formatter::format_size(report.bytes_freed) << "\n"
                  << "Retained: " << report.retained_count << " objects\n";
    }
    return 0;
}

int CliApp::cmd_export_graph(const std::vector<std::string>& args) {
    std::filesystem::path out_path = "chronogit_graph.json";
    if (!args.empty()) {
        out_path = args[0];
    }

    auto repo = Repository::find_repo(std::filesystem::current_path());
    auto branches = repo.list_branches();

    std::set<ObjectId> all_commits;
    for (const auto& b : branches) {
        auto hist = repo.get_commit_history(b.commit_id);
        for (const auto& [id, _] : hist) {
            all_commits.insert(id);
        }
    }

    auto head_id = repo.branch_manager().get_head_commit_id();
    if (head_id) {
        auto hist = repo.get_commit_history(*head_id);
        for (const auto& [id, _] : hist) {
            all_commits.insert(id);
        }
    }

    std::ostringstream json;
    json << "{\n"
         << "  \"branches\": [\n";
    for (size_t i = 0; i < branches.size(); ++i) {
        json << "    {\"name\": \"" << branches[i].name << "\", \"commit_id\": \""
             << branches[i].commit_id << "\", \"is_current\": "
             << (branches[i].is_current ? "true" : "false") << "}"
             << (i + 1 < branches.size() ? "," : "") << "\n";
    }
    json << "  ],\n"
         << "  \"commits\": [\n";

    size_t c_idx = 0;
    for (const auto& id : all_commits) {
        auto commit = repo.db().read_commit(id);
        json << "    {\n"
             << "      \"id\": \"" << id << "\",\n"
             << "      \"tree_id\": \"" << commit->tree_id() << "\",\n"
             << "      \"author\": \"" << commit->author().name << "\",\n"
             << "      \"email\": \"" << commit->author().email << "\",\n"
             << "      \"timestamp\": " << commit->author().timestamp << ",\n"
             << "      \"message\": \"" << commit->message() << "\",\n"
             << "      \"parents\": [";
        for (size_t p = 0; p < commit->parent_ids().size(); ++p) {
            json << "\"" << commit->parent_ids()[p] << "\"" << (p + 1 < commit->parent_ids().size() ? ", " : "");
        }
        json << "]\n    }" << (++c_idx < all_commits.size() ? "," : "") << "\n";
    }
    json << "  ]\n}\n";

    AtomicFile::write_text(out_path, json.str());
    std::cout << "Exported commit graph DAG to " << out_path.string() << " (" << all_commits.size() << " commits)\n";
    return 0;
}

} // namespace chronogit

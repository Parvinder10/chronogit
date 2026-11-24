#pragma once

#include <vector>
#include <string>

namespace chronogit {

class CliApp {
public:
    int run(int argc, char** argv);

private:
    int cmd_init(const std::vector<std::string>& args);
    int cmd_add(const std::vector<std::string>& args);
    int cmd_commit(const std::vector<std::string>& args);
    int cmd_status(const std::vector<std::string>& args);
    int cmd_log(const std::vector<std::string>& args);
    int cmd_branch(const std::vector<std::string>& args);
    int cmd_checkout(const std::vector<std::string>& args);
    int cmd_diff(const std::vector<std::string>& args);
    int cmd_merge(const std::vector<std::string>& args);
    int cmd_show(const std::vector<std::string>& args);
    int cmd_fsck(const std::vector<std::string>& args);
    int cmd_gc(const std::vector<std::string>& args);
    int cmd_export_graph(const std::vector<std::string>& args);
    void print_help() const;
};

} // namespace chronogit

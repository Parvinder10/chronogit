#pragma once

#include <stdexcept>
#include <string>

namespace chronogit {

class ChronoGitException : public std::runtime_error {
public:
    explicit ChronoGitException(const std::string& message) : std::runtime_error(message) {}
};

class RepositoryNotFoundException : public ChronoGitException {
public:
    explicit RepositoryNotFoundException(const std::string& path)
        : ChronoGitException("fatal: not a chronogit repository (or any of the parent directories): " + path) {}
};

class ObjectCorruptedException : public ChronoGitException {
public:
    explicit ObjectCorruptedException(const std::string& id, const std::string& detail = "")
        : ChronoGitException("fatal: object " + id + " is corrupted" + (detail.empty() ? "" : ": " + detail)) {}
};

class ObjectNotFoundException : public ChronoGitException {
public:
    explicit ObjectNotFoundException(const std::string& id)
        : ChronoGitException("fatal: object not found: " + id) {}
};

class BranchNotFoundException : public ChronoGitException {
public:
    explicit BranchNotFoundException(const std::string& branch)
        : ChronoGitException("error: branch '" + branch + "' not found") {}
};

class UncommittedChangesException : public ChronoGitException {
public:
    explicit UncommittedChangesException(const std::string& msg)
        : ChronoGitException("error: your local changes would be overwritten: " + msg) {}
};

} // namespace chronogit

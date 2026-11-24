cmake_minimum_required(VERSION 3.20)
project(ChronoGit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings and optimizations
if (MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Core library sources
set(LIB_SOURCES
    src/core/types.cpp
    src/core/sha256.cpp
    src/core/compress.cpp
    src/core/atomic_file.cpp
    src/core/path_utils.cpp
    src/storage/object.cpp
    src/storage/blob.cpp
    src/storage/tree.cpp
    src/storage/commit.cpp
    src/storage/object_db.cpp
    src/storage/index.cpp
    src/storage/tree_builder.cpp
    src/repo/branch_manager.cpp
    src/repo/workspace_scanner.cpp
    src/repo/checkout_engine.cpp
    src/repo/repository.cpp
    src/diff/myers_diff.cpp
    src/diff/diff_engine.cpp
    src/merge/lca.cpp
    src/merge/three_way_merge.cpp
    src/merge/merge_engine.cpp
    src/maintenance/fsck.cpp
    src/maintenance/gc.cpp
    src/cli/formatter.cpp
    src/cli/cli_app.cpp
)

add_library(chronogit_lib STATIC ${LIB_SOURCES})
target_include_directories(chronogit_lib PUBLIC include)

if (WIN32)
    target_link_libraries(chronogit_lib PUBLIC bcrypt)
endif()

# CLI Executable
add_executable(chronogit src/main.cpp)
target_link_libraries(chronogit PRIVATE chronogit_lib)

# Tests & Benchmarks
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)

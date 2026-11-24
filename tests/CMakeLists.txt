set(TEST_SOURCES
    main.cpp
    test_sha256.cpp
    test_compress.cpp
    test_object_db.cpp
    test_index.cpp
    test_tree_builder.cpp
    test_branch.cpp
    test_diff.cpp
    test_merge.cpp
    test_fsck_gc.cpp
    test_integration.cpp
)

add_executable(chronogit_tests ${TEST_SOURCES})
target_include_directories(chronogit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chronogit_tests PRIVATE chronogit_lib)

add_test(NAME chronogit_all_tests COMMAND chronogit_tests)

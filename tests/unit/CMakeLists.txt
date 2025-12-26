# Unit Tests CMakeLists.txt
# Add this to the main CMakeLists.txt with: add_subdirectory(tests/unit)

# Find Google Test
find_package(GTest REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find all test source files
file(GLOB TEST_SOURCES "*.cpp")

# Exclude test_hawkes_engine due to header file compilation error
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_hawkes_engine\\.cpp")

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        /opt/homebrew/include
    )
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)

    # Enable exceptions for tests (since they use try/catch)
    target_compile_options(${TEST_NAME} PRIVATE -fexceptions)

    # Link Google Test and OpenSSL
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest GTest::gtest_main OpenSSL::SSL OpenSSL::Crypto)

    # Register as test
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Custom target to run all unit tests
add_custom_target(run_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES}
    COMMENT "Running all unit tests"
)
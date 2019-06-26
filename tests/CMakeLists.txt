add_compile_options("-Wno-unused-variable")

add_library(doctest_main OBJECT "src/test-main.cpp")
target_include_directories(doctest_main PRIVATE "third_party/doctest")

# inspired by https://github.com/nlohmann/json/blob/develop/test/CMakeLists.txt
file(GLOB files "src/unit-*.cpp")
foreach(file ${files})
    get_filename_component(file_basename ${file} NAME_WE)
    string(REGEX REPLACE "unit-([^$]+)" "test-\\1" testcase ${file_basename})
    add_executable(${testcase} $<TARGET_OBJECTS:doctest_main> ${file})

    target_include_directories(${testcase}
        PRIVATE
            third_party/doctest
            ${PRIVATE_INCLUDES}
    )

    target_link_libraries(${testcase} PRIVATE ${STATIC_NAME})

    add_test(NAME "${testcase}"
        COMMAND ${testcase}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    set_tests_properties("${testcase}" PROPERTIES LABELS "default")
endforeach()

# set the appropriate domain environment variable value for the environment tests
set_tests_properties(test-default-environment PROPERTIES ENVIRONMENT LIBCAFFEINE_DOMAIN=)
set_tests_properties(test-custom-environment PROPERTIES ENVIRONMENT LIBCAFFEINE_DOMAIN=custom.environment.net)
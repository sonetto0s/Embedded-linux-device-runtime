cmake_minimum_required(VERSION 3.16)

project(myminishell
    VERSION 1.5.0
    LANGUAGES C
)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINISHELL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(MINISHELL_COMMON_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wformat=2
    -Wstrict-prototypes
)

if(MINISHELL_WARNINGS_AS_ERRORS)
    list(APPEND MINISHELL_COMMON_WARNINGS -Werror)
endif()

set(MINISHELL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

set(MINISHELL_CORE_SOURCES
    src/shell.c
    src/parser.c
    src/executor.c
    src/dispatcher.c
    src/builtin.c
    src/command.c
    src/sig.c
    src/shell_context.c
    src/job.c
    src/event.c
    src/builtin_table.c
    src/system_info.c
    src/device_tree.c
    src/hardware_info.c
    src/sysfs_io.c
    src/led_control.c
    src/terminal.c
    common/utils.c
    common/log.c
    common/error.c
    config/config.c
)

add_library(minishell_core STATIC
    ${MINISHELL_CORE_SOURCES}
)

target_include_directories(minishell_core
    PUBLIC
        ${MINISHELL_INCLUDE_DIRS}
)

target_compile_definitions(minishell_core
    PUBLIC
        _POSIX_C_SOURCE=200809L
)

target_compile_options(minishell_core
    PRIVATE
        ${MINISHELL_COMMON_WARNINGS}
)

add_executable(minishell
    src/main.c
)

target_link_libraries(minishell
    PRIVATE
        minishell_core
)

target_compile_options(minishell
    PRIVATE
        ${MINISHELL_COMMON_WARNINGS}
)

if(BUILD_TESTING)

    set(MINISHELL_UNIT_TEST_SOURCES
        tests/test_main.c
        tests/test_framework.c
        tests/test_parser.c
        tests/test_log.c
        tests/test_config.c
        tests/test_event.c
        tests/test_shell_context.c
        tests/test_builtin_table.c
        tests/test_system_info.c
        tests/test_device_tree.c
        tests/test_hardware_info.c
        tests/test_sysfs_io.c
        tests/test_led_control.c
        tests/test_dispatcher.c
        tests/test_executor.c
        tests/test_job.c
        tests/test_job_control.c
        tests/test_command.c
    )

    add_executable(minishell_tests
        ${MINISHELL_UNIT_TEST_SOURCES}
    )

    target_include_directories(minishell_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(minishell_tests
        PRIVATE
            minishell_core
    )

    target_compile_options(minishell_tests
        PRIVATE
            ${MINISHELL_COMMON_WARNINGS}
    )

    add_test(
        NAME minishell_unit_tests
        COMMAND
            ${CMAKE_COMMAND} -E env
            "MINISHELL_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            $<TARGET_FILE:minishell_tests>
    )

    set(MINISHELL_INTEGRATION_TEST_SOURCES
        tests/integration/test_integration_main.c
        tests/integration/test_shell_runner.c
        tests/integration/test_shell_basic.c
        tests/integration/test_shell_redirect.c
        tests/integration/test_shell_pipeline.c
        tests/integration/test_shell_status.c
        tests/integration/test_shell_background.c
        tests/integration/test_shell_input.c
        tests/integration/test_shell_pty.c
        tests/test_framework.c
    )

    add_executable(minishell_integration_tests
        ${MINISHELL_INTEGRATION_TEST_SOURCES}
    )

    target_include_directories(minishell_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration
    )

    target_compile_definitions(minishell_integration_tests
        PRIVATE
            _POSIX_C_SOURCE=200809L
    )

    target_compile_options(minishell_integration_tests
        PRIVATE
            ${MINISHELL_COMMON_WARNINGS}
    )

    add_dependencies(minishell_integration_tests minishell)

    add_test(
        NAME minishell_integration_tests
        COMMAND
            ${CMAKE_COMMAND} -E env
            "MINISHELL_TEST_BIN=$<TARGET_FILE:minishell>"
            "MINISHELL_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            $<TARGET_FILE:minishell_integration_tests>
    )

    set_tests_properties(
        minishell_unit_tests
        minishell_integration_tests
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            TIMEOUT 30
    )

endif()

install(
    TARGETS minishell
    RUNTIME DESTINATION bin
)

install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/config.conf"
    DESTINATION config
)




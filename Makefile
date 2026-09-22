CROSS_COMPILE ?=
ARCH ?= native

ifeq ($(origin CC), default)
CC := $(CROSS_COMPILE)gcc
endif

VALGRIND ?= valgrind
CPPCHECK ?= cppcheck

ARM64_CROSS_COMPILE ?= aarch64-linux-gnu-
ARM64_CC := $(ARM64_CROSS_COMPILE)gcc

COMMON_CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wformat=2 \
	-Wstrict-prototypes

STRICT_CFLAGS := \
	$(COMMON_CFLAGS) \
	-Werror

CFLAGS ?= $(COMMON_CFLAGS) -g -O0
CPPFLAGS ?=

CPPFLAGS_COMMON := \
	-D_POSIX_C_SOURCE=200809L

LDFLAGS ?=
LDLIBS ?=

VALGRIND_FLAGS := \
	--leak-check=full \
	--show-leak-kinds=all \
	--track-origins=yes \
	--track-fds=yes \
	--error-exitcode=99

CPPCHECK_FLAGS := \
	--std=c11 \
	--language=c \
	--enable=warning,performance,portability \
	--suppress=missingIncludeSystem \
	--error-exitcode=2

SRC_DIR := src
TEST_DIR := tests
INTEGRATION_DIR := $(TEST_DIR)/integration
COMMON_DIR := common
CONFIG_DIR := config

BUILD_DIR ?= build/default
DEPLOY_DIR ?= dist/$(ARCH)

PREFIX ?= /opt/minishell
DESTDIR ?=

INSTALL_BINDIR := $(DESTDIR)$(PREFIX)/bin
INSTALL_CONFIGDIR := $(DESTDIR)$(PREFIX)/config

TARGET := $(BUILD_DIR)/minishell
RUN_TARGET := shell
TEST_TARGET := $(BUILD_DIR)/minishell_tests
INTEGRATION_TARGET := $(BUILD_DIR)/minishell_integration_tests

APP_SRC := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/shell.c \
	$(SRC_DIR)/parser.c \
	$(SRC_DIR)/executor.c \
	$(SRC_DIR)/dispatcher.c \
	$(SRC_DIR)/builtin.c \
	$(SRC_DIR)/command.c \
	$(SRC_DIR)/sig.c \
	$(SRC_DIR)/shell_context.c \
	$(SRC_DIR)/job.c \
	$(SRC_DIR)/event.c \
	$(SRC_DIR)/builtin_table.c \
	$(SRC_DIR)/system_info.c \
	$(SRC_DIR)/commands/cmd_runtime.c \
	$(SRC_DIR)/runtime_monitor.c \
	$(SRC_DIR)/runtime_snapshot.c \
	$(SRC_DIR)/process_monitor.c \
	$(SRC_DIR)/network_monitor.c \
	$(SRC_DIR)/thermal_monitor.c \
	$(SRC_DIR)/device_tree.c \
	$(SRC_DIR)/hardware_info.c \
	$(SRC_DIR)/sysfs_io.c \
	$(SRC_DIR)/led_control.c \
	$(SRC_DIR)/terminal.c \
	$(COMMON_DIR)/utils.c \
	$(COMMON_DIR)/log.c \
	$(COMMON_DIR)/error.c \
	$(CONFIG_DIR)/config.c

TEST_SRC := \
	$(TEST_DIR)/test_main.c \
	$(TEST_DIR)/test_framework.c \
	$(TEST_DIR)/test_parser.c \
	$(TEST_DIR)/test_log.c \
	$(TEST_DIR)/test_config.c \
	$(TEST_DIR)/test_event.c \
	$(TEST_DIR)/test_shell_context.c \
	$(TEST_DIR)/test_builtin_table.c \
	$(TEST_DIR)/test_system_info.c \
	$(TEST_DIR)/test_runtime_monitor.c \
	$(TEST_DIR)/test_process_monitor.c \
	$(TEST_DIR)/test_network_monitor.c \
	$(TEST_DIR)/test_thermal_monitor.c \
	$(TEST_DIR)/test_runtime_snapshot.c \
	$(TEST_DIR)/test_device_tree.c \
	$(TEST_DIR)/test_hardware_info.c \
	$(TEST_DIR)/test_sysfs_io.c \
	$(TEST_DIR)/test_led_control.c \
	$(TEST_DIR)/test_dispatcher.c \
	$(TEST_DIR)/test_executor.c \
	$(TEST_DIR)/test_job.c \
	$(TEST_DIR)/test_job_control.c \
	$(TEST_DIR)/test_command.c \
	$(SRC_DIR)/shell.c \
	$(SRC_DIR)/parser.c \
	$(SRC_DIR)/executor.c \
	$(SRC_DIR)/dispatcher.c \
	$(SRC_DIR)/builtin.c \
	$(SRC_DIR)/builtin_table.c \
	$(SRC_DIR)/command.c \
	$(SRC_DIR)/device_tree.c \
	$(SRC_DIR)/sig.c \
	$(SRC_DIR)/shell_context.c \
	$(SRC_DIR)/job.c \
	$(SRC_DIR)/event.c \
	$(SRC_DIR)/hardware_info.c \
	$(SRC_DIR)/sysfs_io.c \
	$(SRC_DIR)/led_control.c \
	$(SRC_DIR)/system_info.c \
	$(SRC_DIR)/commands/cmd_runtime.c \
	$(SRC_DIR)/runtime_monitor.c \
	$(SRC_DIR)/runtime_snapshot.c \
	$(SRC_DIR)/process_monitor.c \
	$(SRC_DIR)/network_monitor.c \
	$(SRC_DIR)/thermal_monitor.c \
	$(SRC_DIR)/terminal.c \
	$(COMMON_DIR)/utils.c \
	$(COMMON_DIR)/log.c \
	$(COMMON_DIR)/error.c \
	$(CONFIG_DIR)/config.c

INTEGRATION_SRC := \
	$(INTEGRATION_DIR)/test_integration_main.c \
	$(INTEGRATION_DIR)/test_shell_runner.c \
	$(INTEGRATION_DIR)/test_shell_basic.c \
	$(INTEGRATION_DIR)/test_shell_redirect.c \
	$(INTEGRATION_DIR)/test_shell_pipeline.c \
	$(INTEGRATION_DIR)/test_shell_status.c \
	$(INTEGRATION_DIR)/test_shell_background.c \
	$(INTEGRATION_DIR)/test_shell_pty.c \
	$(INTEGRATION_DIR)/test_shell_input.c \
	$(TEST_DIR)/test_framework.c

APP_CPPFLAGS := \
	$(CPPFLAGS) \
	$(CPPFLAGS_COMMON) \
	-Iinclude \
	-I$(COMMON_DIR) \
	-I$(CONFIG_DIR)

TEST_CPPFLAGS := \
	$(APP_CPPFLAGS) \
	-I$(TEST_DIR)

APP_OBJ := $(APP_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_OBJ := $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)
INTEGRATION_OBJ := $(INTEGRATION_SRC:%.c=$(BUILD_DIR)/%.o)

DEPS := \
	$(APP_OBJ:.o=.d) \
	$(TEST_OBJ:.o=.d) \
	$(INTEGRATION_OBJ:.o=.d)

.PHONY: \
	all \
	build \
	binary \
	shell \
	run \
	native \
	arm64 \
	package \
	package-files \
	arm64-package \
	install \
	uninstall \
	print-config \
	test \
	integration \
	check \
	debug \
	release \
	asan \
	valgrind \
	strict \
	cppcheck \
	static \
	clean \
	help

all: shell

build: all

binary: $(TARGET)

shell: $(TARGET)
	@echo "  COPY    $(RUN_TARGET)"
	@cp $(TARGET) $(RUN_TARGET)

run: shell
	./$(RUN_TARGET)

native: shell

arm64:
	@command -v $(ARM64_CC) >/dev/null 2>&1 || { \
		echo "ARM64 cross compiler not found: $(ARM64_CC)"; \
		echo "Install it with: sudo apt install gcc-aarch64-linux-gnu"; \
		exit 1; \
	}
	@$(MAKE) \
		ARCH=arm64 \
		CROSS_COMPILE=$(ARM64_CROSS_COMPILE) \
		CC=$(ARM64_CC) \
		BUILD_DIR=build/arm64 \
		binary
	@echo "  ARM64   build/arm64/minishell"

package:
	@rm -rf build/package/native
	@$(MAKE) \
		ARCH=native \
		BUILD_DIR=build/package/native \
		DEPLOY_DIR=dist/native \
		CFLAGS='$(COMMON_CFLAGS) -O2 -DNDEBUG' \
		package-files

package-files: $(TARGET)
	@echo "  PACKAGE $(DEPLOY_DIR)"
	@rm -rf "$(DEPLOY_DIR)"
	@install -d "$(DEPLOY_DIR)/bin"
	@install -d "$(DEPLOY_DIR)/config"
	@install -m 0755 "$(TARGET)" "$(DEPLOY_DIR)/bin/minishell"
	@install -m 0644 "$(CONFIG_DIR)/config.conf" "$(DEPLOY_DIR)/config/config.conf"

arm64-package:
	@command -v $(ARM64_CC) >/dev/null 2>&1 || { \
		echo "ARM64 cross compiler not found: $(ARM64_CC)"; \
		echo "Install it with: sudo apt install gcc-aarch64-linux-gnu"; \
		exit 1; \
	}
	@rm -rf build/package/arm64
	@$(MAKE) \
		ARCH=arm64 \
		CROSS_COMPILE=$(ARM64_CROSS_COMPILE) \
		CC=$(ARM64_CC) \
		BUILD_DIR=build/package/arm64 \
		DEPLOY_DIR=dist/arm64 \
		CFLAGS='$(COMMON_CFLAGS) -O2 -DNDEBUG' \
		package-files

install: $(TARGET)
	@echo "  INSTALL MiniShell"
	@echo "  Prefix : $(PREFIX)"
	@echo "  Root   : $(DESTDIR)"
	@install -d "$(INSTALL_BINDIR)"
	@install -d "$(INSTALL_CONFIGDIR)"
	@install -m 0755 "$(TARGET)" "$(INSTALL_BINDIR)/minishell"
	@install -m 0644 "$(CONFIG_DIR)/config.conf" "$(INSTALL_CONFIGDIR)/config.conf"
	@echo "  Installed:"
	@echo "    $(INSTALL_BINDIR)/minishell"
	@echo "    $(INSTALL_CONFIGDIR)/config.conf"

uninstall:
	@echo "  UNINSTALL MiniShell"
	@echo "  Prefix : $(PREFIX)"
	@echo "  Root   : $(DESTDIR)"
	@rm -f "$(INSTALL_BINDIR)/minishell"
	@rm -f "$(INSTALL_CONFIGDIR)/config.conf"
	@rmdir "$(INSTALL_BINDIR)" 2>/dev/null || true
	@rmdir "$(INSTALL_CONFIGDIR)" 2>/dev/null || true
	@rmdir "$(DESTDIR)$(PREFIX)" 2>/dev/null || true
	@echo "  MiniShell removed"

print-config:
	@echo "ARCH          = $(ARCH)"
	@echo "CROSS_COMPILE = $(CROSS_COMPILE)"
	@echo "CC            = $(CC)"
	@echo "BUILD_DIR     = $(BUILD_DIR)"
	@echo "TARGET        = $(TARGET)"
	@echo "DEPLOY_DIR    = $(DEPLOY_DIR)"
	@echo "PREFIX        = $(PREFIX)"
	@echo "DESTDIR       = $(DESTDIR)"

$(TARGET): $(APP_OBJ)
	@mkdir -p $(dir $@)
	@echo "  LD      $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_TARGET): $(TEST_OBJ)
	@mkdir -p $(dir $@)
	@echo "  LD      $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(INTEGRATION_TARGET): $(INTEGRATION_OBJ)
	@mkdir -p $(dir $@)
	@echo "  LD      $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(APP_CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/$(INTEGRATION_DIR)/%.o: $(INTEGRATION_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

test: $(TEST_TARGET)
	@echo "  TEST    $(TEST_TARGET)"
	MINISHELL_TEST_DIR="./$(BUILD_DIR)" \
	./$(TEST_TARGET)

integration: $(TARGET) $(INTEGRATION_TARGET)
	@echo "  TEST    $(INTEGRATION_TARGET)"
	MINISHELL_TEST_BIN="./$(TARGET)" \
	MINISHELL_TEST_DIR="./$(BUILD_DIR)" \
	./$(INTEGRATION_TARGET)

check: test integration shell

debug:
	@$(MAKE) \
		BUILD_DIR=build/debug \
		CFLAGS='$(COMMON_CFLAGS) -g -O0' \
		all

release:
	@$(MAKE) \
		BUILD_DIR=build/release \
		CFLAGS='$(COMMON_CFLAGS) -O2 -DNDEBUG' \
		all

asan:
	@ASAN_OPTIONS='detect_leaks=1:halt_on_error=1' \
	UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1' \
	$(MAKE) \
		BUILD_DIR=build/asan \
		CFLAGS='$(COMMON_CFLAGS) -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined' \
		LDFLAGS='-fsanitize=address,undefined' \
		check

valgrind: $(TARGET)
	@command -v $(VALGRIND) >/dev/null 2>&1 || { \
		echo "valgrind not found"; \
		exit 1; \
	}
	@echo "  VALGRIND basic"
	@printf 'echo hello\npwd\ntrue\nfalse\nstatus\nexit\n' | \
		$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)
	@echo "  VALGRIND redirect/pipeline"
	@printf 'echo hello > /tmp/minishell_valgrind.txt\ncat < /tmp/minishell_valgrind.txt\nprintf abc | wc -c\nsleep 1 | false\nstatus\nexit\n' | \
		$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)
	@rm -f /tmp/minishell_valgrind.txt
	@echo "  VALGRIND background shutdown"
	@printf 'sleep 30 &\nsleep 31 &\njobs\nexit\n' | \
		$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)
	@echo "  VALGRIND child stress"
	@{ \
		for i in $$(seq 1 100); do \
			echo "true &"; \
		done; \
		echo "sleep 1"; \
		echo "jobs"; \
		echo "exit"; \
	} | $(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)

strict:
	@$(MAKE) \
		BUILD_DIR=build/strict \
		CFLAGS='$(STRICT_CFLAGS) -g -O0' \
		check

cppcheck:
	@command -v $(CPPCHECK) >/dev/null 2>&1 || { \
		echo "cppcheck not found"; \
		exit 1; \
	}
	@echo "  CPPCHECK"
	$(CPPCHECK) \
		$(CPPCHECK_FLAGS) \
		-D_POSIX_C_SOURCE=200809L \
		-Iinclude \
		-I$(COMMON_DIR) \
		-I$(CONFIG_DIR) \
		-I$(TEST_DIR) \
		$(SRC_DIR) \
		$(COMMON_DIR) \
		$(CONFIG_DIR) \
		$(TEST_DIR)

static: strict cppcheck

clean:
	@echo "  CLEAN"
	rm -rf build
	rm -rf dist
	rm -f $(RUN_TARGET)

help:
	@echo "MiniShell build system"
	@echo ""
	@echo "Build:"
	@echo "  make                 Build MiniShell and create ./shell"
	@echo "  make build           Build MiniShell and create ./shell"
	@echo "  make native          Build native MiniShell"
	@echo "  make arm64           Cross-compile ARM64 MiniShell"
	@echo "  make run             Build and run native MiniShell"
	@echo ""
	@echo "Deployment:"
	@echo "  make package         Create optimized native deployment package"
	@echo "  make arm64-package   Create optimized ARM64 deployment package"
	@echo "  make install         Install MiniShell under PREFIX"
	@echo "  make uninstall       Remove installed MiniShell files"
	@echo "  make print-config    Show current build/deployment configuration"
	@echo ""
	@echo "Tests:"
	@echo "  make test            Build and run unit tests"
	@echo "  make integration     Build and run integration tests"
	@echo "  make check           Run unit + integration tests"
	@echo ""
	@echo "Quality:"
	@echo "  make asan            Run tests with ASan + LSan + UBSan"
	@echo "  make valgrind        Run runtime memory/fd checks"
	@echo "  make strict          Run full tests with warnings as errors"
	@echo "  make cppcheck        Run cppcheck static analysis"
	@echo "  make static          Run strict build + cppcheck"
	@echo ""
	@echo "Profiles:"
	@echo "  make debug           Build debug configuration"
	@echo "  make release         Build release configuration"
	@echo ""
	@echo "Maintenance:"
	@echo "  make clean           Remove build/deployment products"
	@echo "  make help            Show this help"

-include $(DEPS)



# mini-AOSP Build System — Stage 0
# Usage: make -C build [all|c|kotlin|clean|run|stop]

ROOT     ?= $(shell cd .. && pwd)
OUT      := $(ROOT)/out
BIN      := $(OUT)/bin
JAR      := $(OUT)/jar

# C compiler settings
CC       := $(shell command -v gcc 2>/dev/null || echo cc)
CFLAGS   := -std=c11 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS  :=

# Kotlin compiler
KOTLINC  := kotlinc

# Source locations (relative to ROOT)
INIT_SRC   := $(ROOT)/system/core/init/main.c
LOG_SRC    := $(ROOT)/system/core/liblog/log.c
COMMON_SRC := $(ROOT)/system/core/libcommon/common.c
SM_SRC     := $(ROOT)/frameworks/native/cmds/servicemanager/main.c

SYSSERVER_KT := $(ROOT)/frameworks/base/services/core/kotlin/SystemServer.kt
LOG_KT       := $(ROOT)/frameworks/base/core/kotlin/os/Log.kt
HELLOAPP_KT  := $(ROOT)/packages/apps/HelloApp/HelloApp.kt

# Targets
.PHONY: all c kotlin clean run stop

all: c kotlin
	@echo ""
	@echo "=== Build complete ==="
	@echo "  C:      $(BIN)/init, $(BIN)/servicemanager"
	@echo "  Kotlin: $(JAR)/system_server.jar, $(JAR)/HelloApp.jar"

c: $(BIN)/init $(BIN)/servicemanager

kotlin: $(JAR)/system_server.jar $(JAR)/HelloApp.jar

# --- C targets ---

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/init: $(INIT_SRC) $(LOG_SRC) $(COMMON_SRC) | $(BIN)
	@echo "[build] Compiling init..."
	$(CC) $(CFLAGS) -o $@ $(INIT_SRC) $(LOG_SRC) $(COMMON_SRC) $(LDFLAGS)

$(BIN)/servicemanager: $(SM_SRC) $(LOG_SRC) $(COMMON_SRC) | $(BIN)
	@echo "[build] Compiling servicemanager..."
	$(CC) $(CFLAGS) -o $@ $(SM_SRC) $(LOG_SRC) $(COMMON_SRC) $(LDFLAGS)

# --- Kotlin targets ---

$(JAR):
	@mkdir -p $(JAR)

$(JAR)/system_server.jar: $(SYSSERVER_KT) $(LOG_KT) | $(JAR)
	@echo "[build] Compiling system_server..."
	$(KOTLINC) -include-runtime -d $@ $(LOG_KT) $(SYSSERVER_KT) 2>&1 | grep -v "^w:" || true

$(JAR)/HelloApp.jar: $(HELLOAPP_KT) $(LOG_KT) | $(JAR)
	@echo "[build] Compiling HelloApp..."
	$(KOTLINC) -include-runtime -d $@ $(LOG_KT) $(HELLOAPP_KT) 2>&1 | grep -v "^w:" || true

# --- Utility targets ---

clean:
	@echo "[build] Cleaning..."
	rm -rf $(OUT)
	@echo "[build] Done."

run: all
	@echo ""
	$(ROOT)/scripts/start.sh

stop:
	$(ROOT)/scripts/stop.sh

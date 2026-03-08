# ============================================================
# Makefile — Hospital Patient Registration & Follow-up Tracker
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c99 -Iinclude
CFLAGS_D = $(CFLAGS) -g -DDEBUG
CFLAGS_R = $(CFLAGS) -O2 -DNDEBUG

SRC_DIR  = src
INC_DIR  = include
WEB_DIR  = web

SRCS     = $(SRC_DIR)/main.c \
           $(SRC_DIR)/patient.c \
           $(SRC_DIR)/appointment.c \
           $(SRC_DIR)/visit.c \
           $(SRC_DIR)/analytics.c \
           $(SRC_DIR)/validation.c

TARGET   = hospital

# Emscripten settings
EMCC     = emcc
EM_FLAGS = -s WASM=1 \
           -s FORCE_FILESYSTEM=1 \
           -s EXPORTED_FUNCTIONS='["_hs_init","_hs_add_patient","_hs_update_patient","_hs_delete_patient","_hs_search_patient","_hs_get_all_patients_json","_hs_add_visit","_hs_get_visits_json","_hs_add_appointment","_hs_get_appointments_json","_hs_cancel_appointment","_hs_get_dashboard_json","_hs_triage_queue_json","_hs_save_all","_hs_load_all","_hs_backup"]' \
           -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","intArrayFromString"]' \
           -s ALLOW_MEMORY_GROWTH=1 \
           -s INITIAL_MEMORY=33554432

.PHONY: all clean debug release wasm help

## Default: release build
all: release

## Debug build (with symbols)
debug: $(SRCS)
	$(CC) $(CFLAGS_D) $(SRCS) -o $(TARGET)_debug
	@echo "Debug build: ./$(TARGET)_debug"

## Release build
release: $(SRCS)
	$(CC) $(CFLAGS_R) $(SRCS) -o $(TARGET)
	@echo "Release build: ./$(TARGET)"

## WebAssembly build (requires Emscripten: https://emscripten.org)
wasm: $(SRCS)
	$(EMCC) $(SRCS) -Iinclude $(EM_FLAGS) -O2 \
		-o $(WEB_DIR)/hospital.js
	@echo "WASM build complete. Files: web/hospital.js + web/hospital.wasm"
	@echo "Add to index.html: <script src=\"hospital.js\"></script>"

## Run native CLI
run: release
	./$(TARGET)

## Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET)_debug
	rm -f $(WEB_DIR)/hospital.js $(WEB_DIR)/hospital.wasm

## Help
help:
	@echo "Targets:"
	@echo "  make           — Release build (native CLI)"
	@echo "  make debug     — Debug build"
	@echo "  make wasm      — WebAssembly build (needs emcc)"
	@echo "  make run       — Build and run native CLI"
	@echo "  make clean     — Remove build artifacts"

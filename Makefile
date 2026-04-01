BUILD_DIR  := build
CONFIG     := Release
EXE        := $(BUILD_DIR)/$(CONFIG)/scar-test.exe
SCAR_ROOT  := examples/mod/assets/scar
TEST_DIR   := examples/test

.PHONY: all build test clean

all: build test

build: $(EXE)

$(EXE): CMakeLists.txt $(wildcard src/*.c src/*.h vendor/lua/CMakeLists.txt)
	cmake -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --config $(CONFIG)

test: $(EXE)
	$(EXE) --scar-root $(SCAR_ROOT) $(TEST_DIR)

clean:
	cmake --build $(BUILD_DIR) --config $(CONFIG) --target clean

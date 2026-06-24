BUILD_DIR ?= build
BUILD_TYPE ?= Debug

.PHONY: all build run clean rebuild count

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j

run: build
	@./$(BUILD_DIR)/sealisp

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

count:
	./scripts/linecount.sh

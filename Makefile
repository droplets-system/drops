SHELL := /bin/bash
TEST_FILES := $(shell find src -name '*.ts')
BIN := ./node_modules/.bin

build:  | build/dir
	cdt-cpp -abigen -abigen_output=build/drops.abi -o build/drops.wasm src/drops.cpp -R src -I include -D DEBUG

build/dir:
	mkdir -p build

clean:
	rm -rf build

.PHONY: test
test: node_modules build
	bun test

.PHONY: check
check: cppcheck jscheck

.PHONY: cppcheck
cppcheck: 
	clang-format --dry-run --Werror src/*.cpp include/drops/*.hpp

.PHONY: jscheck
jscheck: node_modules
	@${BIN}/eslint src --ext .ts --max-warnings 0 --format unix && echo "Ok"

.PHONY: format
format: cppformat jsformat

.PHONY: cppformat
cppformat:
	clang-format -i src/*.cpp include/drops/*.hpp

.PHONY: jsformat
jsformat: node_modules
	@${BIN}/eslint src --ext .ts --fix

.PHONY: distclean
distclean: clean
	rm -rf node_modules/

node_modules:
	bun install

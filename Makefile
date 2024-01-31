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
check: node_modules
	@${BIN}/eslint src --ext .ts --max-warnings 0 --format unix && echo "Ok"

.PHONY: format
format: node_modules
	@${BIN}/eslint src --ext .ts --fix

.PHONY: distclean
distclean: clean
	rm -rf node_modules/

node_modules:
	bun install

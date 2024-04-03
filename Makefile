SHELL := /bin/bash
TEST_FILES := $(shell find src -name '*.ts')
BIN := ./node_modules/.bin

MAINNET_NODE_URL = https://eos.greymass.com
MAINNET_ACCOUNT_NAME = drops
TESTNET_NODE_URL = https://jungle4.greymass.com
TESTNET_ACCOUNT_NAME = drops
DEVNET_NODE_URL = https://jungle4.greymass.com
DEVNET_ACCOUNT_NAME = drops2.gm
CONTRACT_NAME = drops

build: | build/dir
	cdt-cpp -abigen -abigen_output=build/${CONTRACT_NAME}.abi -o build/${CONTRACT_NAME}.wasm src/drops.cpp -R src -I include -D DEBUG

build/debug: | build/dir
	cdt-cpp -abigen -abigen_output=build/${CONTRACT_NAME}.abi -o build/${CONTRACT_NAME}.wasm src/drops.cpp -R src -I include -D DEBUG

build/production: | build/dir
	cdt-cpp -abigen -abigen_output=build/${CONTRACT_NAME}.abi -o build/${CONTRACT_NAME}.wasm src/drops.cpp -R src -I include

build/system:
	cdt-cpp -o include/eosio.system/eosio.wasm include/eosio.system/eosio.cpp -R src -I include

build/dir:
	mkdir -p build

clean:
	rm -rf build

devnet: build/debug
	cleos -u $(DEVNET_NODE_URL) set contract $(DEVNET_ACCOUNT_NAME) \
		build/ ${CONTRACT_NAME}.wasm ${CONTRACT_NAME}.abi

testnet: build/debug
	cleos -u $(TESTNET_NODE_URL) set contract $(TESTNET_ACCOUNT_NAME) \
		build/ ${CONTRACT_NAME}.wasm ${CONTRACT_NAME}.abi

mainnet: build/production
	cleos -u $(MAINNET_NODE_URL) set contract $(MAINNET_ACCOUNT_NAME) \
		build/ ${CONTRACT_NAME}.wasm ${CONTRACT_NAME}.abi

.PHONY: test
test: build/debug node_modules build/drops.ts init/codegen
	bun test

.PHONY: testprod
testprod: build/production node_modules build/drops.ts init/codegen
	bun test

.PHONY: bench
bench:
	bun **/*.bench.ts

init/codegen: codegen/dir codegen/eosio.ts codegen/eosio.token.ts

build/drops.ts:
	npx @wharfkit/cli generate --json ./build/${CONTRACT_NAME}.abi --file ./build/${CONTRACT_NAME}.ts drops

codegen/dir:
	mkdir -p codegen

codegen/eosio.ts:
	npx @wharfkit/cli generate --url https://jungle4.greymass.com --file ./codegen/eosio.ts eosio

codegen/eosio.token.ts:
	npx @wharfkit/cli generate --url https://jungle4.greymass.com --file ./codegen/eosio.token.ts eosio.token

.PHONY: check
check: cppcheck jscheck

.PHONY: cppcheck
cppcheck:
	clang-format --dry-run --Werror src/*.cpp include/${CONTRACT_NAME}/*.hpp

.PHONY: jscheck
jscheck: node_modules
	@${BIN}/eslint src --ext .ts --max-warnings 0 --format unix && echo "Ok"

.PHONY: format
format: cppformat jsformat

.PHONY: cppformat
cppformat:
	clang-format -i src/*.cpp include/${CONTRACT_NAME}/*.hpp

.PHONY: jsformat
jsformat: node_modules
	@${BIN}/eslint src --ext .ts --fix

.PHONY: distclean
distclean: clean
	rm -rf node_modules/

node_modules:
	bun install

.PHONY: dev/enable
dev/enable:
	cleos -u $(DEVNET_NODE_URL) push action $(DEVNET_ACCOUNT_NAME) enable '{"enabled": true}' -p $(DEVNET_ACCOUNT_NAME)@active

.PHONY: dev/wipe
dev/wipe:
	cleos -u $(DEVNET_NODE_URL) push action $(DEVNET_ACCOUNT_NAME) cleartable '{"table_name": "balances"}' -p $(DEVNET_ACCOUNT_NAME)@active
	cleos -u $(DEVNET_NODE_URL) push action $(DEVNET_ACCOUNT_NAME) cleartable '{"table_name": "drop"}' -p $(DEVNET_ACCOUNT_NAME)@active
	cleos -u $(DEVNET_NODE_URL) push action $(DEVNET_ACCOUNT_NAME) cleartable '{"table_name": "state"}' -p $(DEVNET_ACCOUNT_NAME)@active

.PHONY: testnet/enable
testnet/enable:
	cleos -u $(TESTNET_NODE_URL) push action $(TESTNET_ACCOUNT_NAME) enable '{"enabled": true}' -p $(TESTNET_ACCOUNT_NAME)@active

.PHONY: testnet/wipe
testnet/wipe:
	cleos -u $(TESTNET_NODE_URL) push action $(TESTNET_ACCOUNT_NAME) cleartable '{"table_name": "balances"}' -p $(TESTNET_ACCOUNT_NAME)@active
	cleos -u $(TESTNET_NODE_URL) push action $(TESTNET_ACCOUNT_NAME) cleartable '{"table_name": "drop", "max_rows": 5000}' -p $(TESTNET_ACCOUNT_NAME)@active
	cleos -u $(TESTNET_NODE_URL) push action $(TESTNET_ACCOUNT_NAME) cleartable '{"table_name": "state"}' -p $(TESTNET_ACCOUNT_NAME)@active
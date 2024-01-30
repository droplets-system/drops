import { TimePointSec, Name, UInt64, Asset } from "@greymass/eosio";
import { Blockchain, expectToThrow } from "@proton/vert"
import { describe, expect, test, beforeEach } from "bun:test";

// Vert EOS VM
const blockchain = new Blockchain()
const bob = "bob";
const alice = "alice";
blockchain.createAccounts(bob, alice);

// one-time setup
beforeEach(async () => {
  blockchain.setTime(TimePointSec.from("2024-01-29T00:00:00.000"));
});

const core_contract = "drops"
const contracts = {
  core: blockchain.createContract(core_contract, core_contract, true),
  token: blockchain.createContract('eosio.token', 'include/eosio.token/eosio.token', true),
  system: blockchain.createContract('eosio', 'include/eosio.system/eosio', true),
}

interface State {
  genesis: string;
  bytes_per_drop: number;
  enabled: boolean;
}

function getState() {
  const scope = Name.from(core_contract).value.value;
  return contracts.core.tables.state(scope).getTableRows()[0] as State
}

interface Drop {
  seed: string;
  owner: string;
  created: string;
}

interface Transfer {
  from: Name;
  to: Name;
  quantity: Asset;
  memo: string;
}

function getBalance(account: string) {
  const scope = Name.from(account).value.value;
  const primary_key = Asset.SymbolCode.from("EOS").value.value;
  return Asset.from(contracts.token.tables.accounts(scope).getTableRow(primary_key).balance);
}

function getDrop(seed: bigint) {
  const scope = Name.from(core_contract).value.value;
  return contracts.core.tables.drop(scope).getTableRow(seed) as Drop
}

function getDrops() {
  const scope = Name.from(core_contract).value.value;
  return contracts.core.tables.drop(scope).getTableRows() as Drop[]
}

describe(core_contract, () => {
  test('eosio::init', async () => {
    await contracts.system.actions.init([]).send();
  });

  test('eosio.token::issue', async () => {
    const supply = `1000000000.0000 EOS`;
    await contracts.token.actions.create(["eosio.token", supply]).send();
    await contracts.token.actions.issue(["eosio.token", supply, ""]).send();
    await contracts.token.actions.transfer(["eosio.token", bob, "1000.0000 EOS", ""]).send();
    await contracts.token.actions.transfer(["eosio.token", alice, "1000.0000 EOS", ""]).send();
  });

  test('enable', async () => {
    await contracts.core.actions.enable([false]).send();
    expect(getState().enabled).toBe(false);

    await contracts.core.actions.enable([true]).send();
    expect(getState().enabled).toBe(true);
  });

  test('mint', async () => {
    const before = getBalance(alice);
    await contracts.token.actions.transfer([alice, core_contract, "10.0000 EOS", "10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"]).send(alice);
    const after = getBalance(alice);

    expect(before.units.value - after.units.value).toBe(5847);
    expect(getDrops().length).toBe(10);
    expect(getDrop(6530728038117924388n)).toEqual({
      bound: false,
      seed: "6530728038117924388",
      owner: "alice",
      created: "2024-01-29T00:00:00.000"
    });
  });

  test('destroy', async () => {
    const before = getBalance(alice);
    await contracts.core.actions.destroy([alice, ["6530728038117924388", "8833355934996727321"], "memo"]).send(alice);
    const after = getBalance(alice);
    const transfer: Transfer = blockchain.actionTraces[2].decodedData as any;

    expect(transfer.quantity.units.value.toNumber()).toBe(1157);
    expect(transfer.memo).toBe("Reclaimed RAM value of 2 drops(s)");
    expect(after.units.value - before.units.value).toBe(1157);
    expect(getDrops().length).toBe(8);
    expect(getDrop(6530728038117924388n)).toBeUndefined();
  });

  test('destroy::error - not found', async () => {
    const action = contracts.core.actions.destroy([alice, ["123"], "memo"]).send(alice);
    await expectToThrow(action, "eosio_assert_message: Drop 123 not found");
  });

  test('destroy::error - must belong to owner', async () => {
    const action = contracts.core.actions.destroy([bob, ["17855725969634623351"], "memo"]).send(bob);
    await expectToThrow(action, "eosio_assert_message: Drop 17855725969634623351 does not belong to account.");
  });

  test('destroy::error - missing required authority', async () => {
    const action = contracts.core.actions.destroy([bob, ["17855725969634623351"], "memo"]).send(alice);
    await expectToThrow(action, "missing required authority bob");
  });
});

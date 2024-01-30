import { TimePointSec, Name } from "@greymass/eosio";
import { Blockchain } from "@proton/vert"
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
}

interface State {
    genesis: string;
    bytes_per_drop: number;
    paused: boolean;
}

function getState() {
  const scope = Name.from(core_contract).value.value;
  return contracts.core.tables.state(scope).getTableRows()[0] as State
}

describe(core_contract, () => {
  test('eosio.token::issue', async () => {
    const supply = `1000000000.0000 EOS`;
    await contracts.token.actions.create(["eosio.token", supply]).send();
    await contracts.token.actions.issue(["eosio.token", supply, ""]).send();
    await contracts.token.actions.transfer(["eosio.token", bob, "1000.0000 EOS", ""]).send();
    await contracts.token.actions.transfer(["eosio.token", alice, "1000.0000 EOS", ""]).send();
  });

  test('pause', async () => {
    await contracts.core.actions.pause([true]).send();
    expect(getState().paused).toBe(true);

    await contracts.core.actions.pause([false]).send();
    expect(getState().paused).toBe(false);
  });

  test('mint', async () => {
    await contracts.token.actions.transfer([alice, core_contract, "10.0000 EOS", "10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"]).send(alice);
  });
});

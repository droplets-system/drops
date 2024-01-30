import { TimePointSec, Name } from "@greymass/eosio";
import { Blockchain } from "@proton/vert"
import { describe, expect, test, beforeEach } from "bun:test";

// Vert EOS VM
const blockchain = new Blockchain()

const core_contract = "drops"
blockchain.createAccount(core_contract);

// one-time setup
beforeEach(async () => {
  blockchain.setTime(TimePointSec.from("2024-01-29T00:00:00.000"));
});

const contracts = {
  core: blockchain.createContract(core_contract, core_contract, true),
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
  test('pause', async () => {
    await contracts.core.actions.pause([true]).send();
    expect(getState().paused).toBe(true);

    await contracts.core.actions.pause([false]).send();
    expect(getState().paused).toBe(false);
  });
});

import {Asset, Int64, Name} from '@wharfkit/antelope'
import {TimePointSec} from '@greymass/eosio'
import {Blockchain, expectToThrow} from '@proton/vert'
import {beforeEach, describe, expect, test} from 'bun:test'

import * as DropsContract from '../build/drops.ts'
import * as TokenContract from '../codegen/eosio.token.ts'
import * as SystemContract from '../codegen/eosio.ts'
import {toHash, toSeed} from './drops.ts'

// Vert EOS VM
const blockchain = new Blockchain()
const bob = 'bob'
const alice = 'alice'
const charles = 'charles'
const daniel = 'daniel'
blockchain.createAccounts(bob, alice, charles, daniel)

const core_contract = 'drops'
const contracts = {
    core: blockchain.createContract(core_contract, `build/${core_contract}`, true),
    token: blockchain.createContract('eosio.token', 'include/eosio.token/eosio.token', true),
    fake: blockchain.createContract('fake.token', 'include/eosio.token/eosio.token', true),
    system: blockchain.createContract('eosio', 'include/eosio.system/eosio', true),
}

function getState(): DropsContract.Types.state_row {
    const scope = Name.from(core_contract).value.value
    const row = contracts.core.tables.state(scope).getTableRows()[0]
    if (!row) throw new Error('State not found')
    return DropsContract.Types.state_row.from(row)
}

function getStat() {
    return getBalance(core_contract)
}

function getTokenBalance(account: string) {
    const scope = Name.from(account).value.value
    const primary_key = Asset.SymbolCode.from('EOS').value.value
    const row = contracts.token.tables
        .accounts(scope)
        .getTableRow(primary_key) as TokenContract.Types.account
    if (!row) throw new Error('Balance not found')
    return Asset.from(row.balance)
}

function getBalance(owner: string) {
    const scope = Name.from(core_contract).value.value
    const primary_key = Name.from(owner).value.value
    const row = contracts.core.tables.balances(scope).getTableRow(primary_key)
    if (!row) throw new Error('Balance not found')
    return DropsContract.Types.balances_row.from(row)
}

function getDrop(seed: bigint): DropsContract.Types.drop_row {
    const scope = Name.from(core_contract).value.value
    const row = contracts.core.tables.drop(scope).getTableRow(seed)
    if (!row) throw new Error('Drop not found')
    return DropsContract.Types.drop_row.from(row)
}

function getDrops(owner?: string): DropsContract.Types.drop_row[] {
    const scope = Name.from(core_contract).value.value
    const rows = contracts.core.tables.drop(scope).getTableRows()
    if (!owner) return rows
    return rows.filter((row) => row.owner === owner)
}

function getRamBytes(account: string) {
    const scope = Name.from(account).value.value
    const row = contracts.system.tables
        .userres(scope)
        .getTableRow(scope) as SystemContract.Types.user_resources
    if (!row) return 0
    return Int64.from(row.ram_bytes).toNumber()
}

// standard error messages
const ERROR_INVALID_MEMO = `eosio_assert_message: Invalid transfer memo. (ex: "<receiver>")`
const ERROR_DROP_NOT_FOUND = 'eosio_assert: Drop not found.'
const ERROR_SYSTEM_DISABLED = 'eosio_assert_message: Drops system is disabled.'
const ERROR_OPEN_BALANCE = 'eosio_assert: Account does not have an open balance.'
const ERROR_ACCOUNT_NOT_EXISTS = 'eosio_assert_message: Account does not exist.'
const ERROR_NO_DROPS = 'eosio_assert_message: No drops were provided.'

describe(core_contract, () => {
    // Setup before each test
    beforeEach(async () => {
        blockchain.setTime(TimePointSec.from('2024-01-29T00:00:00.000'))
        await contracts.core.actions.enable([true]).send()
    })

    test('eosio::init', async () => {
        await contracts.system.actions.init([]).send()
    })

    test('eosio.token::issue', async () => {
        const supply = `1000000000.0000 EOS`
        await contracts.token.actions.create(['eosio.token', supply]).send()
        await contracts.token.actions.issue(['eosio.token', supply, '']).send()
        await contracts.token.actions.transfer(['eosio.token', bob, '1000.0000 EOS', '']).send()
        await contracts.token.actions.transfer(['eosio.token', alice, '1000.0000 EOS', '']).send()
        await contracts.token.actions.transfer(['eosio.token', charles, '1000.0000 EOS', '']).send()
    })

    test('eosio::buyrambytes', async () => {
        const before = getRamBytes(alice)
        await contracts.system.actions.buyrambytes([alice, alice, 10000]).send()
        const after = getRamBytes(alice)
        expect(after - before).toBe(10000)
    })

    test('eosio::ramtransfer', async () => {
        const before = getRamBytes(bob)
        await contracts.system.actions.ramtransfer([alice, bob, 5000, '']).send()
        const after = getRamBytes(bob)
        expect(after - before).toBe(5000)
    })

    test('fake.token::issue', async () => {
        const supply = `1000000000.0000 EOS`
        await contracts.fake.actions.create(['fake.token', supply]).send()
        await contracts.fake.actions.issue(['fake.token', supply, '']).send()
        await contracts.fake.actions.transfer(['fake.token', bob, '1000.0000 EOS', '']).send()
        await contracts.fake.actions.transfer(['fake.token', alice, '1000.0000 EOS', '']).send()
    })

    test('enable', async () => {
        await contracts.core.actions.enable([false]).send()
        expect(getState().enabled).toBe(false)

        await contracts.core.actions.enable([true]).send()
        expect(getState().enabled).toBe(true)
    })

    test('open', async () => {
        try {
            getBalance(alice)
        } catch (error) {
            expect(error.message).toBe('Balance not found')
        }
        await contracts.core.actions.open([alice]).send(alice)
        await contracts.core.actions.open([bob]).send(bob)
        const balance = getBalance(alice)
        if (balance) {
            expect(balance.owner.toString()).toBe(alice)
            expect(balance.ram_bytes.toNumber()).toBe(0)
            expect(balance.drops.toNumber()).toBe(0)
        }
    })

    test('on_transfer', async () => {
        const tokenBefore = getTokenBalance(alice)
        await contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', alice])
            .send(alice)
        const after = getBalance(alice)
        const tokenAfter = getTokenBalance(alice)
        expect(after.ram_bytes.toNumber()).toBe(87550)

        // should not receive any EOS refunds on transfer
        expect(tokenAfter.value - tokenBefore.value).toBe(-10)

        // logging
        const logrambytes = DropsContract.Types.logrambytes.from(
            blockchain.actionTraces[4].decodedData
        )
        expect(logrambytes.ram_bytes.toNumber()).toEqual(87550)
        expect(logrambytes.before_ram_bytes.toNumber()).toEqual(0)
        expect(logrambytes.bytes.toNumber()).toEqual(87550)
    })

    test('on_notify::ramtransfer', async () => {
        const before = Int64.from(getBalance(core_contract).ram_bytes).toNumber()
        await contracts.system.actions.ramtransfer([alice, core_contract, 277, alice]).send()
        const after = Int64.from(getBalance(core_contract).ram_bytes).toNumber()
        expect(after - before).toBe(277)
    })

    test('on_transfer::error - contract disabled', async () => {
        await contracts.core.actions.enable([false]).send()
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', alice])
            .send(alice)
        await expectToThrow(action, ERROR_SYSTEM_DISABLED)
    })

    test('on_transfer::error - empty memo', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', ''])
            .send(alice)
        await expectToThrow(action, ERROR_INVALID_MEMO)
    })

    test('on_transfer::error - receiver must be sender', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', bob])
            .send(alice)
        await expectToThrow(action, 'eosio_assert: Receiver must be the same as the sender.')
    })

    test('on_transfer::error - account must exists', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', 'foobar'])
            .send(alice)
        await expectToThrow(action, ERROR_ACCOUNT_NOT_EXISTS)
    })

    test('on_transfer::error - account must have open balance', async () => {
        const action = contracts.token.actions
            .transfer([charles, core_contract, '10.0000 EOS', charles])
            .send(charles)
        await expectToThrow(action, ERROR_OPEN_BALANCE)
    })

    test('generate - bound=false', async () => {
        await contracts.token.actions.transfer([bob, core_contract, '100.0000 EOS', bob]).send(bob)

        const data = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
        const before = getBalance(bob)
        await contracts.core.actions.generate([bob, false, 1, data, bob]).send(bob)
        const after = getBalance(bob)

        // should consume RAM bytes
        expect(after.ram_bytes.toNumber() - before.ram_bytes.toNumber()).toBe(-277)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(1)
        const drop = {
            seed: '343891094750660754',
            owner: 'bob',
            created: '2024-01-29T00:00:00.000',
            bound: false,
        }
        expect(getDrop(343891094750660754n).equals(drop)).toBeTrue()

        // logging drops
        const logdrops = DropsContract.Types.logdrops.from(blockchain.actionTraces[4].decodedData)
        expect(logdrops.amount.toNumber()).toEqual(1)
        expect(logdrops.before_drops.toNumber()).toEqual(0)
        expect(logdrops.drops.toNumber()).toEqual(1)

        // logging generate
        const loggenerate = DropsContract.Types.loggenerate.from(
            blockchain.actionTraces[5].decodedData
        )
        expect(loggenerate.bytes_balance.toNumber()).toEqual(875223)
        expect(loggenerate.bytes_used.toNumber()).toEqual(277)
        expect(loggenerate.generated.toNumber()).toEqual(1)
        expect(loggenerate.drops).toStrictEqual([DropsContract.Types.drop_row.from(drop)])
    })

    test('generate - bound=true', async () => {
        const data = 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
        const before = getBalance(bob)
        await contracts.core.actions.generate([bob, true, 1, data, bob]).send(bob)
        const after = getBalance(bob)

        // should not consume any RAM bytes
        expect(after.ram_bytes.toNumber() - before.ram_bytes.toNumber()).toBe(0)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(1)
        const drop = {
            seed: '11725508947118797007',
            owner: 'bob',
            created: '2024-01-29T00:00:00.000',
            bound: true,
        }
        expect(getDrop(11725508947118797007n).equals(drop)).toBeTrue()

        // seed should be deterministic
        const index = 0
        const sequence = 100
        expect(toSeed([index, sequence, data].join('')).toString()).toBe('3615493820451389612')

        // logging generate
        const loggenerate = DropsContract.Types.loggenerate.from(
            blockchain.actionTraces[3].decodedData
        )
        expect(loggenerate.bytes_balance.toNumber()).toEqual(875223)
        expect(loggenerate.bytes_used.toNumber()).toEqual(277)
        expect(loggenerate.generated.toNumber()).toEqual(1)
        expect(loggenerate.drops).toStrictEqual([DropsContract.Types.drop_row.from(drop)])
    })

    test('toSeed', () => {
        const data = 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
        const index = 0
        const sequence = 100
        const seed = toSeed([index, sequence, data].join(''))
        const hash = toHash(seed)
        expect(seed.toString()).toBe('3615493820451389612')
        expect(hash.toString()).toBe('ac00a05779d02c32')
    })

    test('generate - with unopened balance', async () => {
        const data = 'ffffffffffffffffffffffffffffffff'
        await contracts.core.actions.generate([daniel, true, 1, data]).send(daniel)
        const after = getBalance(daniel)

        // should not consume any RAM bytes
        expect(after.ram_bytes.toNumber()).toBe(0)
        expect(after.drops.toNumber()).toBe(1)
    })

    test.skip('generate::error - already exists', async () => {
        const action = contracts.core.actions
            .generate([bob, false, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
            .send(bob)

        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 already exists.'
        )
    })

    test('generate::error - contract cannot generate', async () => {
        const action = contracts.core.actions
            .generate([core_contract, false, 1, '1bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
            .send(core_contract)

        await expectToThrow(action, 'eosio_assert: Cannot generate drops for contract.')
    })

    test('generate::error - not have enough RAM bytes.', async () => {
        const action = contracts.core.actions
            .generate([alice, false, 1000, '1bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
            .send(alice)

        await expectToThrow(action, 'eosio_assert_message: alice does not have enough RAM bytes.')
    })

    test('generate 1K', async () => {
        const before = getBalance(bob)
        const data = 'cccccccccccccccccccccccccccccccc'
        await contracts.core.actions.generate([bob, true, 1000, data]).send(bob)
        const after = getBalance(bob)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(1000)
        expect(getDrops(bob).length).toBe(1002)
    })

    test('on_transfer::error - invalid contract', async () => {
        const action = contracts.fake.actions
            .transfer([alice, core_contract, '10.0000 EOS', 'buyram,alice'])
            .send(alice)
        await expectToThrow(
            action,
            'eosio_assert: Only the eosio.token contract may send tokens to this contract.'
        )
    })

    test('destroy', async () => {
        const data = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
        await contracts.core.actions.generate([alice, false, 10, data]).send(alice)
        const before = getBalance(alice)
        await contracts.core.actions
            .destroy([alice, ['13991429617541607035', '16719869299757338970'], 'memo', alice])
            .send(alice)
        const after = getBalance(alice)

        // destroy unbound drops should reclaim RAM to owner
        expect(after.ram_bytes.value - before.ram_bytes.value).toBe(277 * 2)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(-2)
        expect(() => getDrop(13991429617541607035n)).toThrow('Drop not found')

        // logging
        const logdestroy = DropsContract.Types.logdestroy.from(
            blockchain.actionTraces[5].decodedData
        )
        expect(logdestroy.bytes_reclaimed.toNumber()).toEqual(554)
        expect(logdestroy.unbound_destroyed.toNumber()).toEqual(2)
        expect(logdestroy.destroyed.toNumber()).toEqual(2)
        expect(logdestroy.drops.map((v) => v.created.toString())).toEqual([
            '2024-01-29T00:00:00.000',
            '2024-01-29T00:00:00.000',
        ])
    })

    test('destroy::error - not found', async () => {
        const action = contracts.core.actions.destroy([alice, ['123'], 'memo']).send(alice)
        await expectToThrow(action, ERROR_DROP_NOT_FOUND)
    })

    test('destroy::error - must belong to owner', async () => {
        const action = contracts.core.actions
            .destroy([bob, ['13913273295484544122'], 'memo'])
            .send(bob)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 13913273295484544122 does not belong to account.'
        )
    })

    test('destroy::error - missing required authority', async () => {
        const action = contracts.core.actions
            .destroy([bob, ['13913273295484544122'], 'memo'])
            .send(alice)
        await expectToThrow(action, 'missing required authority bob')
    })

    test('destroy::error - no drops', async () => {
        const action = contracts.core.actions.destroy([bob, [], '']).send(bob)
        await expectToThrow(action, ERROR_NO_DROPS)
    })

    test('unbind', async () => {
        const before = getBalance(bob)
        const drop_id = 18438477392015167831n
        expect(getDrop(drop_id).bound).toBeTruthy()
        await contracts.core.actions.unbind([bob, [String(drop_id)]]).send(bob)

        // drop must now be unbound
        expect(getDrop(drop_id).bound).toBeFalsy()
        const after = getBalance(bob)

        // deduct RAM bytes to unbind drops
        expect(after.ram_bytes.value - before.ram_bytes.value).toBe(-277)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(0)
    })

    test('unbind::error - not found', async () => {
        const action = contracts.core.actions.unbind([bob, ['123']]).send(bob)
        await expectToThrow(action, ERROR_DROP_NOT_FOUND)
    })

    test('unbind::error - does not belong to account', async () => {
        const action = contracts.core.actions.unbind([alice, ['18438477392015167831']]).send(alice)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 18438477392015167831 does not belong to account.'
        )
    })

    test('unbind::error - is not bound', async () => {
        const action = contracts.core.actions.unbind([bob, ['18438477392015167831']]).send(bob)
        await expectToThrow(action, 'eosio_assert_message: Drop 18438477392015167831 is not bound')
    })

    test('unbind::error - no drops', async () => {
        const action = contracts.core.actions.unbind([bob, []]).send(bob)
        await expectToThrow(action, ERROR_NO_DROPS)
    })

    test('bind', async () => {
        const before = getBalance(bob)
        const drop_id = 18438477392015167831n
        expect(getDrop(drop_id).bound).toBeFalsy()
        await contracts.core.actions.bind([bob, [String(drop_id)]]).send(bob)

        // drop must now be unbound
        expect(getDrop(drop_id).bound).toBeTruthy()
        const after = getBalance(bob)

        // returned for excess RAM bytes
        expect(after.ram_bytes.value - before.ram_bytes.value).toBe(277)
        expect(after.drops.toNumber() - before.drops.toNumber()).toBe(0)
    })

    test('bind::error - not found', async () => {
        const action = contracts.core.actions.bind([bob, ['123']]).send(bob)
        await expectToThrow(action, ERROR_DROP_NOT_FOUND)
    })

    test('bind::error - does not belong to account', async () => {
        const action = contracts.core.actions.bind([alice, ['18438477392015167831']]).send(alice)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 18438477392015167831 does not belong to account.'
        )
    })

    test('bind::error - is not unbound', async () => {
        const drop_id = '18438477392015167831'
        const action = contracts.core.actions.bind([bob, [drop_id]]).send(bob)
        await expectToThrow(action, `eosio_assert_message: Drop ${drop_id} is not unbound`)
    })

    test('bind::error - no drops', async () => {
        const action = contracts.core.actions.bind([bob, []]).send(bob)
        await expectToThrow(action, ERROR_NO_DROPS)
    })

    test('transfer', async () => {
        const before = {
            alice: getBalance(alice),
            bob: getBalance(bob),
            stat: getStat(),
        }
        const drop_id = 13913273295484544122n
        expect(getDrop(drop_id).bound).toBeFalsy()
        await contracts.core.actions.transfer([alice, bob, [String(drop_id)], '']).send(alice)

        // drop should remain unbound
        expect(getDrop(drop_id).bound).toBeFalsy()
        const after = {
            alice: getBalance(alice),
            bob: getBalance(bob),
            stat: getStat(),
        }

        // no RAM bytes should be consumed for either accounts
        expect(after.alice.ram_bytes.value - before.alice.ram_bytes.value).toBe(0)
        expect(after.bob.ram_bytes.value - before.bob.ram_bytes.value).toBe(0)
        expect(after.stat.ram_bytes.value - before.stat.ram_bytes.value).toBe(0)

        // drop should be transferred to bob
        expect(after.alice.drops.toNumber() - before.alice.drops.toNumber()).toBe(-1)
        expect(after.bob.drops.toNumber() - before.bob.drops.toNumber()).toBe(1)
        expect(after.stat.drops.value - before.stat.drops.value).toBe(0)
    })

    // https://github.com/drops-system/drops/issues/15
    test('transfer - transfer to unopened', async () => {
        const before = {
            bob: getBalance(bob),
        }
        const drop_id = '13913273295484544122'
        await contracts.core.actions.transfer([bob, charles, [drop_id], '']).send(bob)
        const after = {
            bob: getBalance(bob),
            charles: getBalance(charles),
        }
        // no RAM bytes should be consumed for either accounts
        expect(after.bob.ram_bytes.value - before.bob.ram_bytes.value).toBe(0)
        expect(after.charles.ram_bytes.value.toNumber()).toBe(0)

        // drop should be transferred to bob
        expect(after.bob.drops.toNumber() - before.bob.drops.toNumber()).toBe(-1)
        expect(after.charles.drops.toNumber()).toBe(1)
    })

    test('transfer::error - account does not exists', async () => {
        const drop_id = 13913273295484544122n
        const action = contracts.core.actions
            .transfer([bob, 'foobar', [String(drop_id)], ''])
            .send(bob)
        await expectToThrow(action, ERROR_ACCOUNT_NOT_EXISTS)
    })

    test('transfer::error - no drops', async () => {
        const action = contracts.core.actions.transfer([bob, alice, [], '']).send(bob)
        await expectToThrow(action, ERROR_NO_DROPS)
    })

    test('transfer::error - can not transfer to contract', async () => {
        const drop_id = 13913273295484544122n
        const action = contracts.core.actions
            .transfer([bob, core_contract, [String(drop_id)], ''])
            .send(bob)
        await expectToThrow(action, 'eosio_assert: Cannot transfer to contract.')
    })
})

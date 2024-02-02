import {Asset, Name} from '@wharfkit/antelope'
import {TimePointSec} from '@greymass/eosio'
import {Blockchain, expectToThrow} from '@proton/vert'
import {beforeEach, describe, expect, test} from 'bun:test'

import * as DropsContract from '../build/drops.ts'
import * as TokenContract from '../codegen/eosio.token.ts'

// Vert EOS VM
const blockchain = new Blockchain()
const bob = 'bob'
const alice = 'alice'
blockchain.createAccounts(bob, alice)

const core_contract = 'drops'
const contracts = {
    core: blockchain.createContract(core_contract, `build/${core_contract}`, true),
    token: blockchain.createContract('eosio.token', 'include/eosio.token/eosio.token', true),
    fake: blockchain.createContract('fake.token', 'include/eosio.token/eosio.token', true),
    system: blockchain.createContract('eosio', 'include/eosio.system/eosio', true),
}

function getState(): DropsContract.Types.state_row {
    const scope = Name.from(core_contract).value.value
    return contracts.core.tables.state(scope).getTableRows()[0]
}

function getTokenBalance(account: string) {
    const scope = Name.from(account).value.value
    const primary_key = Asset.SymbolCode.from('EOS').value.value
    return Asset.from(contracts.token.tables.accounts(scope).getTableRow(primary_key).balance)
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

const ERROR_INVALID_MEMO = `eosio_assert_message: Invalid transfer memo. (ex: "<amount>,<data>")`
const ERROR_SYSTEM_DISABLED = 'eosio_assert_message: Drops system is disabled.'

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
        const before = getBalance(alice)
        await contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', `buyram,${alice}`])
            .send(alice)
        const after = getBalance(alice)
        expect(after.ram_bytes.toNumber() - before.ram_bytes.toNumber()).toBe(87990)
    })

    test('on_transfer::error - contract disabled', async () => {
        await contracts.core.actions.enable([false]).send()
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', `buyram,${alice}`])
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
            .transfer([alice, core_contract, '10.0000 EOS', `buyram,${bob}`])
            .send(alice)
        await expectToThrow(action, 'eosio_assert: Receiver must be the same as the sender.')
    })

    test('generate', async () => {
        await contracts.token.actions
            .transfer([bob, core_contract, '100.0000 EOS', `buyram,${bob}`])
            .send(bob)

        const data = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
        await contracts.core.actions.generate([bob, 1, data]).send(bob)

        expect(getDrops(bob).length).toBe(1)
        expect(
            getDrop(10272988527514872302n).equals({
                seed: '10272988527514872302',
                owner: 'bob',
                created: '2024-01-29T00:00:00.000',
                bound: true,
            })
        ).toBeTrue()
    })

    test('generate::error - already exists', async () => {
        const action = contracts.core.actions
            .generate([bob, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
            .send(bob)

        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 already exists.'
        )
    })

    test('generate 1K', async () => {
        const before = getBalance(bob)
        await contracts.core.actions.generate([bob, 1000, 'cccccccccccccccccccccccccccccccc']).send(bob)
        const after = getBalance(bob)

        expect(getDrops(bob).length).toBe(1001)
        expect(after.ram_bytes.toNumber() - before.ram_bytes.toNumber()).toBe(-512000)
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
        await contracts.core.actions.generate([alice, 10, 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa']).send(alice)
        const before = getBalance(alice)
        await contracts.core.actions
            .destroy([alice, ['6530728038117924388', '8833355934996727321'], 'memo'])
            .send(alice)
        const after = getBalance(alice)
        expect(after.ram_bytes.value - before.ram_bytes.value).toBe(1024)
        expect(getDrops(alice).length).toBe(8)
        expect(() => getDrop(6530728038117924388n)).toThrow('Drop not found')
    })

    test('destroy::error - not found', async () => {
        const action = contracts.core.actions.destroy([alice, ['123'], 'memo']).send(alice)
        await expectToThrow(action, 'eosio_assert: Drop not found.')
    })

    test('destroy::error - must belong to owner', async () => {
        const action = contracts.core.actions
            .destroy([bob, ['17855725969634623351'], 'memo'])
            .send(bob)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 17855725969634623351 does not belong to account.'
        )
    })

    test('destroy::error - missing required authority', async () => {
        const action = contracts.core.actions
            .destroy([bob, ['17855725969634623351'], 'memo'])
            .send(alice)
        await expectToThrow(action, 'missing required authority bob')
    })

    test('unbind', async () => {
        const before = getBalance(bob)
        expect(getDrop(10272988527514872302n).bound).toBeTruthy()
        await contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)

        // drop must now be unbound
        expect(getDrop(10272988527514872302n).bound).toBeFalsy()
        const after = getBalance(bob)

        // EOS returned for excess RAM
        expect(after.ram_bytes.value - before.ram_bytes.value).toBe(-583)
    })

    test('unbind::error - not found', async () => {
        const action = contracts.core.actions.unbind([bob, ['123']]).send(bob)
        await expectToThrow(action, 'eosio_assert: Drop not found.')
    })

    test('unbind::error - does not belong to account', async () => {
        const action = contracts.core.actions.unbind([alice, ['10272988527514872302']]).send(alice)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 does not belong to account.'
        )
    })

    test('unbind::error - is not bound', async () => {
        const action = contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)
        await expectToThrow(action, 'eosio_assert_message: Drop 10272988527514872302 is not bound')
    })

    test('bind', async () => {
        const before = getBalance(bob)
        expect(getDrop(10272988527514872302n).bound).toBeFalsy()
        await contracts.core.actions.bind([bob, ['10272988527514872302']]).send(bob)

        // drop must now be unbound
        expect(getDrop(10272988527514872302n).bound).toBeTruthy()
        const after = getBalance(bob)

        // EOS returned for excess RAM
        expect(after.units.value - before.units.value).toBe(578)
    })

    test('bind::error - not found', async () => {
        const action = contracts.core.actions.bind([bob, ['123']]).send(bob)
        await expectToThrow(action, 'eosio_assert: Drop not found.')
    })

    test('bind::error - does not belong to account', async () => {
        const action = contracts.core.actions.bind([alice, ['10272988527514872302']]).send(alice)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 does not belong to account.'
        )
    })

    test('bind::error - is not unbound', async () => {
        const action = contracts.core.actions.bind([bob, ['10272988527514872302']]).send(bob)
        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 is not unbound'
        )
    })
})

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

function getBalance(account: string) {
    const scope = Name.from(account).value.value
    const primary_key = Asset.SymbolCode.from('EOS').value.value
    return Asset.from(contracts.token.tables.accounts(scope).getTableRow(primary_key).balance)
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

function getUnbind(owner?: string): DropsContract.Types.unbind_row[] {
    const scope = Name.from(core_contract).value.value
    const rows = contracts.core.tables.unbind(scope).getTableRows()
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

    test('on_transfer', async () => {
        const before = getBalance(alice)
        await contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', '10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'])
            .send(alice)
        const after = getBalance(alice)

        expect(after.units.value - before.units.value).toBe(-5847)
        expect(getDrops(alice).length).toBe(10)
        expect(
            getDrop(6530728038117924388n).equals({
                seed: '6530728038117924388',
                owner: 'alice',
                created: '2024-01-29T00:00:00.000',
                bound: false,
            })
        ).toBeTrue()
    })

    test('on_transfer::error - contract disabled', async () => {
        await contracts.core.actions.enable([false]).send()
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', ''])
            .send(alice)
        await expectToThrow(action, ERROR_SYSTEM_DISABLED)
    })

    test('on_transfer::error - empty memo', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', ''])
            .send(alice)
        await expectToThrow(action, ERROR_INVALID_MEMO)
    })

    test('on_transfer::error - invalid number', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', 'foo,bar'])
            .send(alice)
        await expectToThrow(action, 'eosio_assert: invalid number format or overflow')
    })

    test('on_transfer::error - number underflow', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', '-1,bar'])
            .send(alice)
        await expectToThrow(action, 'eosio_assert: number underflow')
    })

    test('on_transfer::error - at least 32 characters', async () => {
        const action = contracts.token.actions
            .transfer([alice, core_contract, '10.0000 EOS', '1,foobar'])
            .send(alice)
        await expectToThrow(
            action,
            'eosio_assert: Drop data must be at least 32 characters in length.'
        )
    })

    test('mint', async () => {
        await contracts.core.actions.mint([bob, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb']).send(bob)

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

    test('mint::error - already exists', async () => {
        const action = contracts.core.actions
            .mint([bob, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
            .send(bob)

        await expectToThrow(
            action,
            'eosio_assert_message: Drop 10272988527514872302 already exists.'
        )
    })

    test('mint 1K', async () => {
        await contracts.core.actions.mint([bob, 1000, 'cccccccccccccccccccccccccccccccc']).send(bob)

        expect(getDrops(bob).length).toBe(1001)
    })

    test('on_transfer::error - invalid contract', async () => {
        const action = contracts.fake.actions
            .transfer([alice, core_contract, '10.0000 EOS', '10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'])
            .send(alice)
        await expectToThrow(
            action,
            'eosio_assert: Only the eosio.token contract may send tokens to this contract.'
        )
    })

    test('destroy', async () => {
        const before = getBalance(alice)
        await contracts.core.actions
            .destroy([alice, ['6530728038117924388', '8833355934996727321'], 'memo'])
            .send(alice)
        const after = getBalance(alice)
        const transfer = TokenContract.Types.transfer.from(blockchain.actionTraces[2].decodedData)

        expect(transfer.quantity.units.value.toNumber()).toBe(1157)
        expect(transfer.memo).toBe('Reclaimed RAM value of 2 drops(s)')
        expect(after.units.value - before.units.value).toBe(1157)
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
        await contracts.token.actions
            .transfer([bob, core_contract, '10.0000 EOS', 'unbind'])
            .send(bob)

        // drop must now be unbound
        expect(getDrop(10272988527514872302n).bound).toBeFalsy()
        const after = getBalance(bob)

        // EOS returned for excess RAM
        expect(after.units.value - before.units.value).toBe(-583)
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

    test('cancelunbind', async () => {
        expect(getUnbind(bob).length).toBe(0)
        await contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)
        expect(getUnbind(bob).length).toBe(1)
        await contracts.core.actions.cancelunbind([bob]).send(bob)
        expect(getUnbind(bob).length).toBe(0)
    })

    test('cancelunbind::error - No unbind request', async () => {
        const action = contracts.core.actions.cancelunbind([bob]).send(bob)
        await expectToThrow(action, 'eosio_assert: No unbind request found for account.')
    })
})

import {baseline, bench, group, run} from 'mitata'
import {toHash, toSeed} from './drops.js'
import {Checksum256, PrivateKey, UInt64} from '@wharfkit/antelope'
import {randomUUID} from 'crypto'

function noop() {
    return 0
}

group('randomString', () => {
    bench('randomUUID', () => {
        randomUUID()
    })
    bench('PrivateKey.generate', () => {
        const randomKey = PrivateKey.generate('K1')
        String(Checksum256.hash(randomKey.data))
    })
})

group('toSeed', () => {
    baseline('baseline', () => {
        noop()
    })
    bench('toSeed', () => toSeed('eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'))
})

group('toHash', () => {
    const seed = UInt64.from(312217830762532995n)
    baseline('baseline', () => {
        noop()
    })
    bench('toHash', () => toHash(seed))
})

await run({
    avg: true, // enable/disable avg column (default: true)
    json: false, // enable/disable json output (default: false)
    colors: true, // enable/disable colors (default: true)
    min_max: true, // enable/disable min/max column (default: true)
    collect: false, // enable/disable collecting returned values into an array during the benchmark (default: false)
    percentiles: false, // enable/disable percentiles column (default: true)
})

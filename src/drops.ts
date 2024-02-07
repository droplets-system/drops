import {Bytes, Checksum256, Serializer, UInt64} from '@wharfkit/antelope'

export function toSeed(data: string) {
    return Serializer.decode({
        data: Checksum256.hash(Bytes.from(data, 'utf8')).array,
        type: 'uint64',
    })
}

export function toHash(seed: UInt64) {
    return String(Bytes.from(seed.byteArray))
}

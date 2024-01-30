## Drops

> Drops Core contract

## TABLE `drops`

### params

- `{uint64_t} seed` - (primary key) unique seed
- `{name} owner` - owner of the drop
- `{block_timestamp} created` - creation time
- `{bool} bound` - whether the drop is bound to an account

### example

```json
{
    "seed": 16355392114041409,
    "owner": "test.gm",
    "created": "2024-01-29T00:00:00.000",
    "bound": false
}
```

## TABLE `state`

### params

- `{block_timestamp} genesis` - genesis time when the contract was created
- `{int64_t} bytes_per_drop` - amount of RAM bytes required per minting drop
- `{bool} enabled` - whether the contract is enabled

### example

```json
{
    "genesis": "2024-01-29T00:00:00",
    "bytes_per_drop": 512,
    "enabled": false
}
```
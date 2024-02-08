## Drops

> Drops Core contract

## TABLE `drops`

### params

-   `{uint64_t} seed` - (primary key) unique seed
-   `{name} owner` - owner of the drop
-   `{block_timestamp} created` - creation time
-   `{bool} bound` - whether the drop is bound to an account

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

-   `{block_timestamp} genesis` - genesis time when the contract was created
-   `{int64_t} bytes_per_drop` - amount of RAM bytes required per minting drop
-   `{bool} enabled` - whether the contract is enabled

### example

```json
{
    "genesis": "2024-01-29T00:00:00",
    "bytes_per_drop": 277,
    "enabled": false
}
```

## TABLE `balances`

### params

-   `{name} owner` - (primary key) owner account
-   `{int64_t} drops` - total amount of drops owned
-   `{int64_t} ram_bytes` - total amount of RAM bytes available by the owner

### example

```json
{
    "owner": "test.gm",
    "drops": 69,
    "ram_bytes": 2048
}
```

## TABLE `stat`

### params

-   `{int64_t} drops` - total supply of drops
-   `{int64_t} ram_bytes` - total available RAM bytes held by the contract

### example

```json
{
    "drops": 88888,
    "ram_bytes": 2048
}
```

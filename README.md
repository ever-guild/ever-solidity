<div align="center">
  <h1><code>ever-solidity</code></h1>
  <p>
    <strong>Solidity compiler for Everscale Virtual Machine</strong>
  </p>
</div>

- [Documentation](API.md)
- [Getting started with smart contracts development](https://docs.everscale.network/develop/smart-contract/getting-started/)
- [Changelog](changelog.md)

## Use via `docker`

```shell
docker pull ghcr.io/everscaleguild/ever-solidity:0.66.0
docker tag ghcr.io/everscaleguild/ever-solidity:0.66.0 sold
docker run --rm -it sold --help
docker run --rm -it -v $(pwd):/src sold --include-path inc Ext.tsol
```

## Build

```shell
make qa
make release
```

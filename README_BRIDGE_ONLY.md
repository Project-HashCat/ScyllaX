# ScyllaX Bridge-only Patch

This build intentionally disables native Scylla process selection/attach.

Allowed data path:

```txt
ScyllaX.exe UI
  -> named pipe
  -> ScyllaXBridgePlugin.dp64/dp32
  -> x64dbg/x32dbg SDK
  -> current debuggee only
```

Behavior changes:

- `ScyllaX.exe` exits if started without `--xdbg-pipe`.
- Process combo box is disabled and display-only.
- Native process enumeration returns an empty list when bridge is not connected.
- `OpenProcess` / `NtOpenProcess` fallback is disabled.
- `Pick DLL` is disabled in bridge mode; target module comes from x64dbg/x32dbg.

Launch only through:

```txt
Plugins -> ScyllaXBridge -> Open ScyllaX UI
```

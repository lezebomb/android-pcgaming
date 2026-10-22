# BCn compute dependency preparation

App 0.1.52 prepares the existing BCn layer and implicit manifest when Compute
decoding requires them, even with ASTC and ETC2 off. Previously extraction depended
on first boot with transcoding enabled or a change to the transcoding switch. A
prefix created with CPU decoding could select Compute without its required files.

On the tablet both files were missing before this change. The installed 0.1.52
compute-only FFXIV launch created them, and its app-owned engine mapped
`libbcn_layer.so` with `ENABLE_BCN_COMPUTE=1` and `BCN_COMPUTE_AUTO=1`. This confirms
dependency setup and actual loading. The corresponding character scene showed
severe white texture errors, so **Compute is not a working graphics/performance
configuration on this sample**. Default CPU decoding remains unchanged.

The settings UI recognizes legacy `cpu` as Software; new defaults use `software`
with ASTC off. Opening the dialog no longer displays the old CPU default as the
first Compute item. Help text describes tradeoffs without promises of a fixed
speedup, zero stutter or guaranteed OOM prevention.

No new prebuilt layer or system driver was introduced. The archive remains the
WinlatorMali asset and hash in [runtime-assets.json](runtime-assets.json). Its
version text identifies Fcharan/WinMali-Dev modifications and a `5c168f4` baseline;
that does not establish reproducibility from the current author's repository.

Reference source:
[leegao/bcn_layer](https://github.com/leegao/bcn_layer/tree/50993a2d51772567de9c36de4d523652773f0899).
Its implicit manifest uses the activation variable above; the source checks
device capabilities before ASTC/ETC2 encoding. The reference source's upstream
MIT notice is retained in [BCn-layer-upstream-LICENSE](licenses/BCn-layer-upstream-LICENSE).
This does not replace missing notices or source material for the bundled modified
archive; the existing [redistribution boundary](licensing.md) still applies.

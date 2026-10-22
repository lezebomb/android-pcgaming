# FFXIV Dawntrail benchmark compatibility

This is a complex DX11 diagnostic sample, not the playable retail game or AAA
acceptance. The official 1.1 archive is available from
[Square Enix](https://na.finalfantasyxiv.com/benchmark/download/). Its binaries and
assets are not redistributed here or modified.

The normal .NET launcher currently exits with `0xffffffff` and Wine reports
`Wine Mono is not installed`. The independent launch script can select the
original `game/ffxiv_dx11.exe` with public engine options also used by
[doitsujin's benchmark launcher](https://github.com/doitsujin/ffxiv-benchmark-launcher/blob/main/ffxiv-benchmark.py)
(source blob `416cebf7e56762e40d5568fbd63cc1277df38004`). This project uses the
option names as interface facts and does not copy that project's implementation.
No login, executable patch, DRM or anti-cheat change is involved.

Run only after the user has obtained and imported the official sample into this
app's `C:/Games/DGFFXIVBench`:

```powershell
$env:ANDROID_SERIAL='your-device'
node scripts/launch-ffxiv-benchmark.mjs --character-creation --wined3d-renderer=gdi
```

`--engine` selects the cutscene test, `--character-creation` selects its interactive
character mode, and no mode argument selects the original .NET launcher. Diagnostic
settings request a 960x540 window, 50 percent internal scale and a 30 FPS cap; the
engine can enforce a larger minimum window. The virtual desktop is 1280x960 to
keep its character editor visible. These are diagnostic requests, not a measured
performance claim.

`--diagnose` requires compiling `scripts/smoke/ffxiv-launch-diagnostic.c` as the
local `artifacts/DG-FFXIVLaunch.exe`. It observes the original child process for at
most 60 seconds, records its actual Windows exit code, then keeps its own process
alive for 15 seconds for log collection. A timeout or session teardown caused by
that observation limit must not be called an engine crash.

On the Mali-G615 tablet with app 0.1.50 and DXVK-Sarek 1.14.1, the initial engine
run exited `0xc0000005`. Wine's own exception log identifies the path through
`dxdiagn`, `ddraw` and `wined3d` into the C runtime. This is an auxiliary legacy
graphics query before any observed 3D scene. WineD3D's Vulkan alternative instead
reached an Android tagged-pointer abort; it is not a working fallback here.

The `gdi` comparison avoided those initial failures. After loading, the original
engine displayed an animated background, a character model and menus. Clicking
Create Character and its confirmation opened the editor; changing to the outdoor
environment displayed terrain, vegetation, buildings and a textured character.
The initial blue environment made the character very dark; its lighting has not
been compared against a native reference. HUD samples varied around 9–20 FPS.
These observations establish real rendering and menu interaction, **not smooth,
stable retail-game acceptance**.
`WINE_D3D_CONFIG=renderer=gdi` controls WineD3D's auxiliary path; the main D3D11 DLL
continues to be DXVK. The option is supported by
[Wine's configuration parser](https://github.com/wine-mirror/wine/blob/wine-10.0/dlls/wined3d/wined3d_main.c).
It must not be described as enabling GDI for general 3D games or as a universal
Mali fix.

App 0.1.51 adds optional `legacyGraphicsQuery=gdi` in direct launch settings. It
applies only with DXVK, preserves other WineD3D settings, and defers to an explicit
custom `renderer`. Clearing it restores the normal path on the next launch.
`--legacy-query` saves this option without an environment override. The preceding
device observations used app 0.1.50 with the explicit environment setting; the
installed product option needs its own corresponding short check.

Private imports, captures and raw logs stay in ignored local directories. Do not
repeat the already successful import or the known failing GL/Vulkan comparisons
without a relevant change.

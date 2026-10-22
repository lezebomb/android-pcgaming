param([string]$SdkPath = $env:ANDROID_HOME)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
if (-not $SdkPath) { throw 'Pass -SdkPath or set ANDROID_HOME.' }
$nodeCommand = Get-Command node -ErrorAction Stop
$cmake = Join-Path $SdkPath 'cmake/3.22.1/bin/cmake.exe'
$ninja = Join-Path $SdkPath 'cmake/3.22.1/bin/ninja.exe'
$toolchain = Join-Path $SdkPath 'ndk/28.2.13676358/build/cmake/android.toolchain.cmake'
& $nodeCommand.Source scripts/prepare-vortek-client.mjs
if ($LASTEXITCODE) { throw 'Vortek source preparation failed.' }
& $cmake -S .tooling/vortek-source -B .tooling/vortek-client-build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DANDROID_PLATFORM=28 -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { throw 'Vortek configure failed.' }
& $cmake --build .tooling/vortek-client-build --parallel 4
if ($LASTEXITCODE) { throw 'Vortek client build failed.' }
$library = '.tooling/vortek-client-build/libvulkan_vortek.so'
$expected = 'cb35c3b0c142601b448a5901f3e9dba0c4d3833a60dadef090fb44eca060e71c'
$actual = (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw 'Client binary differs from the pinned build; review before updating its application hash.' }
New-Item -ItemType Directory -Force -Path app/src/main/assets/vortek,artifacts/vortek | Out-Null
Copy-Item -LiteralPath $library -Destination app/src/main/assets/vortek/libvulkan_vortek.so -Force
Copy-Item -LiteralPath $library -Destination artifacts/vortek/libvulkan_vortek.so -Force
Copy-Item -LiteralPath artifacts/vortek/source-lock.json -Destination app/src/main/assets/vortek/source-lock.json -Force
Write-Output "Built optional Android Vortek client: $actual"

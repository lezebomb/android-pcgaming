param(
    [string]$SdkPath = 'D:\Collect\.tooling\android-sdk',
    [string]$NdkVersion = '28.2.13676358',
    [string]$CmakeVersion = '3.22.1'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceDir = Join-Path $projectRoot '.tooling/mali-compat-source'
$buildDir = Join-Path $projectRoot '.tooling/mali-compat-build'
$outputDir = Join-Path $projectRoot 'artifacts/mali-compat'
if (!(Test-Path (Join-Path $sourceDir 'dimensity-source-lock.json'))) {
    throw 'Run scripts/prepare-mali-compat.mjs with Node first.'
}
$nodeExe = if (Get-Command node -ErrorAction SilentlyContinue) { (Get-Command node).Source } else { 'D:\nodejs\node.exe' }
& $nodeExe (Join-Path $PSScriptRoot 'patch-mali-compat.mjs')
if ($LASTEXITCODE -ne 0) { throw 'Mali local source verification/patch failed.' }
$cmake = Join-Path $SdkPath "cmake/$CmakeVersion/bin/cmake.exe"
$ninja = Join-Path $SdkPath "cmake/$CmakeVersion/bin/ninja.exe"
$ndk = Join-Path $SdkPath "ndk/$NdkVersion"
& $cmake -S $sourceDir -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
    -DANDROID_PLATFORM=26 -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_static `
    -DCMAKE_BUILD_TYPE=Release -DBUILD_VARIANT=android-arm64
if ($LASTEXITCODE -ne 0) { throw 'Mali compat CMake configuration failed.' }
& $cmake --build $buildDir --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Mali compat build failed.' }
New-Item -ItemType Directory -Force $outputDir | Out-Null
Copy-Item (Join-Path $buildDir 'libdxvk_mali_compat_layer.so') $outputDir
Copy-Item (Join-Path $sourceDir 'LICENSE') $outputDir
Copy-Item (Join-Path $sourceDir 'dimensity-patched-source-lock.json') (Join-Path $outputDir 'dimensity-source-lock.json')
Get-FileHash (Join-Path $outputDir 'libdxvk_mali_compat_layer.so') -Algorithm SHA256

param(
    [string]$AdbPath = 'D:\Collect\.tooling\android-sdk\platform-tools\adb.exe',
    [string]$Serial = 'HA2C3HFV',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$packageName = 'org.dimensity.gaming.dev'
$library = 'files/imagefs/usr/lib/libdxvk_mali_compat_layer.so'
$manifest = 'files/imagefs/usr/share/vulkan/explicit_layer.d/dimensity_mali_compat.json'
function Invoke-Adb {
    & $AdbPath -s $Serial @args
    if ($LASTEXITCODE -ne 0) { throw "ADB command failed: $($args[0])" }
}
# Touch only this development application's two optional layer files.
if ($Remove) {
    Invoke-Adb shell run-as $packageName rm -f $manifest $library
    Write-Output 'Experimental Mali layer removed. Restart the game without its VK_INSTANCE_LAYERS override.'
    return
}
$localLibrary = Join-Path $projectRoot 'artifacts/mali-compat/libdxvk_mali_compat_layer.so'
if (!(Test-Path $localLibrary)) { throw 'Build the layer first with scripts/build-mali-compat.ps1.' }
$appDataPath = (& $AdbPath -s $Serial shell run-as $packageName pwd).Trim()
if ($LASTEXITCODE -ne 0 -or $appDataPath -notmatch '^/data/(user/0|data)/org\.dimensity\.gaming\.dev$') {
    throw 'Unexpected development application data directory.'
}
$layerManifest = @{
    file_format_version = '1.0.0'
    layer = @{
        name = 'VK_LAYER_COMPAT_DxvkMaliCompatLayer'
        type = 'GLOBAL'
        library_path = "$appDataPath/files/imagefs/usr/lib/libdxvk_mali_compat_layer.so"
        api_version = '1.3.0'
        implementation_version = '1'
        description = 'Dimensity experimental Mali compatibility (explicit opt-in)'
        functions = @{
            vkGetInstanceProcAddr = 'DxvkMaliCompatLayer_GetInstanceProcAddr'
            vkGetDeviceProcAddr = 'DxvkMaliCompatLayer_GetDeviceProcAddr'
        }
    }
} | ConvertTo-Json -Depth 4
$localManifest = Join-Path $projectRoot 'artifacts/mali-compat/dimensity_mali_compat.json'
[IO.File]::WriteAllText($localManifest, $layerManifest + "`n", [Text.UTF8Encoding]::new($false))
Invoke-Adb push $localLibrary /data/local/tmp/dimensity-mali-compat.so
Invoke-Adb push $localManifest /data/local/tmp/dimensity-mali-compat.json
Invoke-Adb shell run-as $packageName mkdir -p files/imagefs/usr/share/vulkan/explicit_layer.d
Invoke-Adb shell run-as $packageName cp /data/local/tmp/dimensity-mali-compat.so $library
Invoke-Adb shell run-as $packageName cp /data/local/tmp/dimensity-mali-compat.json $manifest
Invoke-Adb shell run-as $packageName chmod 755 $library
Invoke-Adb shell run-as $packageName sha256sum $library
Invoke-Adb shell rm -f /data/local/tmp/dimensity-mali-compat.so /data/local/tmp/dimensity-mali-compat.json
Write-Output 'Installed explicit layer; disabled unless the game selects VK_INSTANCE_LAYERS=VK_LAYER_COMPAT_DxvkMaliCompatLayer.'

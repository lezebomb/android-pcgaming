param(
    [string]$Serial,
    [switch]$Install,
    [switch]$Launch,
    [switch]$CollectLogs
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$adb = if ($env:ANDROID_HOME) { Join-Path $env:ANDROID_HOME 'platform-tools\adb.exe' } else { 'adb' }
& $adb devices -l
$devices = @((& $adb devices) | Select-Object -Skip 1 | Where-Object { $_ -match '^\S+\s+device$' })
if (-not $Serial) {
    if ($devices.Count -ne 1) { throw 'Connect one authorized device, or pass -Serial.' }
    $Serial = ($devices[0] -split '\s+')[0]
}
$adbArgs = @('-s', $Serial)
$report = Join-Path $projectRoot 'artifacts\device-info.txt'
$info = foreach ($property in @('ro.product.manufacturer','ro.product.model','ro.board.platform','ro.build.version.release','ro.build.version.sdk','ro.product.cpu.abilist')) {
    $value = & $adb @adbArgs shell getprop $property
    "$property=$value"
}
$info += & $adb @adbArgs shell cat /proc/meminfo
$info | Set-Content -LiteralPath $report -Encoding utf8
Write-Output $info
# These operations run only when the caller explicitly requests the switches.
if ($Install) {
    $appVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'app/build.gradle')), "versionName\s+'([^']+)'").Groups[1].Value
    if ($appVersion -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw 'Cannot determine a safe APK version from app/build.gradle.' }
    $apk = Join-Path $projectRoot ("artifacts\DimensityGaming-{0}.apk" -f $appVersion)
    & $adb @adbArgs install -r $apk
    if ($LASTEXITCODE) { throw 'Installation failed. No existing app was uninstalled or cleared.' }
}
if ($Launch) {
    & $adb @adbArgs shell am start -n 'org.dimensity.gaming.dev/com.winlator.cmod.MainActivity'
    if ($LASTEXITCODE) { throw 'Launch failed.' }
}
if ($CollectLogs) {
    $appProcessId = (& $adb @adbArgs shell pidof org.dimensity.gaming.dev).Trim()
    if ($appProcessId -match '^\d+$') {
        & $adb @adbArgs logcat -d "--pid=$appProcessId" -t 1200 | Set-Content -LiteralPath (Join-Path $projectRoot 'artifacts\android-logcat.txt') -Encoding utf8
    }
    # No logcat -c, force-stop, uninstall, settings changes, or user-data deletion.
}

param(
    [string]$SdkPath = $env:ANDROID_HOME,
    [string]$GradlePath,
    [switch]$Offline,
    [switch]$FetchRuntime
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $projectRoot
if (-not $SdkPath -or -not (Test-Path -LiteralPath $SdkPath)) { throw 'Set ANDROID_HOME or pass -SdkPath.' }
$env:ANDROID_HOME = $SdkPath
$env:ANDROID_SDK_ROOT = $SdkPath
$env:ANDROID_USER_HOME = Join-Path $projectRoot '.tooling\android'
$env:GRADLE_USER_HOME = Join-Path $projectRoot '.gradle-home'
New-Item -ItemType Directory -Force -Path '.tooling','artifacts' | Out-Null
[IO.File]::WriteAllText((Join-Path $projectRoot 'local.properties'), 'sdk.dir=' + $SdkPath.Replace('\','/'))
if ($FetchRuntime) {
    $nodeCommand = Get-Command node -ErrorAction SilentlyContinue
    $nodeExe = if ($nodeCommand) { $nodeCommand.Source } elseif (Test-Path 'D:\nodejs\node.exe') { 'D:\nodejs\node.exe' } else { throw 'Node.js 20+ is required to fetch runtime archives.' }
    & $nodeExe 'scripts\fetch-runtime.mjs'
    if ($LASTEXITCODE) { throw 'Runtime download failed.' }
}
foreach ($asset in @('imagefs.tzst','proton-10-arm64ec.tzst','proton-10-arm64ec_container_pattern.tzst')) {
    if (-not (Test-Path -LiteralPath ('app\src\main\assets\' + $asset))) { throw "Missing $asset. Run scripts/build.ps1 -FetchRuntime." }
}
if (-not (Test-Path '.tooling\debug.keystore')) {
    & "$env:JAVA_HOME\bin\keytool.exe" -genkeypair -keystore '.tooling/debug.keystore' -alias androiddebugkey -storepass android -keypass android -dname 'CN=Dimensity Gaming Dev,O=Personal,C=CN' -keyalg RSA -keysize 2048 -validity 10000
    if ($LASTEXITCODE) { throw 'Debug key creation failed.' }
}
if (-not $GradlePath) {
    $gradleCommand = Get-Command gradle -ErrorAction SilentlyContinue
    $cached = Get-ChildItem -LiteralPath 'D:\Collect\.tooling\gradle\wrapper\dists\gradle-9.3.1-all' -Recurse -Filter gradle.bat -ErrorAction SilentlyContinue | Select-Object -First 1
    $GradlePath = if ($gradleCommand) { $gradleCommand.Source } elseif ($cached) { $cached.FullName } else { Join-Path $projectRoot 'gradlew.bat' }
}
if (Test-Path 'D:\Collect\.tooling\gradle\caches\modules-2') {
    $env:GRADLE_RO_DEP_CACHE = 'D:\Collect\.tooling\gradle\caches'
}
$gradleArgs = @(':app:assembleDebug', '--console=plain')
if ($Offline) { $gradleArgs += '--offline' }
$savedErrorPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $GradlePath @gradleArgs *> 'artifacts\build.log'
$buildExitCode = $LASTEXITCODE
$ErrorActionPreference = $savedErrorPreference
if ($buildExitCode) { Get-Content 'artifacts\build.log' -Tail 55; throw 'APK build failed; see artifacts/build.log.' }
$appVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'app/build.gradle')), "versionName\s+'([^']+)'").Groups[1].Value
if ($appVersion -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw 'Cannot determine a safe APK version from app/build.gradle.' }
$apk = Join-Path $projectRoot ("artifacts\DimensityGaming-{0}.apk" -f $appVersion)
Copy-Item -LiteralPath 'app\build\outputs\apk\debug\app-debug.apk' -Destination $apk -Force
# aapt expands .gz assets and removes the suffix. Verify the actual installed
# asset name and upstream bytes, not only the source file before packaging.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$dgArchiveLock = Get-Content -LiteralPath 'app/src/main/assets/dxwrapper/sarek-source-lock.json' -Raw | ConvertFrom-Json
$dgZip = [IO.Compression.ZipFile]::OpenRead($apk)
try {
    $dgEntry = $dgZip.GetEntry('assets/dxwrapper/dxvk-1.14.1-sarek.tgz')
    if (!$dgEntry -or $dgEntry.Length -ne $dgArchiveLock.size) { throw 'Sarek asset missing or renamed in final APK.' }
    $dgStream = $dgEntry.Open()
    $dgDigest = [Security.Cryptography.SHA256]::Create()
    try { $dgAssetHash = ([BitConverter]::ToString($dgDigest.ComputeHash($dgStream))).Replace('-','').ToLowerInvariant() }
    finally { $dgStream.Dispose(); $dgDigest.Dispose() }
    if ($dgAssetHash -ne $dgArchiveLock.archiveSha256) { throw 'Sarek asset was transformed during packaging.' }
} finally { $dgZip.Dispose() }
$hash = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($apk + '.sha256', $hash + '  ' + [IO.Path]::GetFileName($apk) + "`n")
Get-Item -LiteralPath $apk | Select-Object FullName,Length
Write-Output "SHA256 $hash"

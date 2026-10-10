param(
    [Parameter(Mandatory = $true)][string]$Adb,
    [Parameter(Mandatory = $true)][string]$Serial,
    [ValidateSet('armeabi-v7a', 'arm64-v8a')]
    [string[]]$Abis = @('armeabi-v7a', 'arm64-v8a')
)
$ErrorActionPreference = 'Stop'
$taskPortRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath $Adb)) { throw "adb missing: $Adb" }
$taskSupported = ((& $Adb -s $Serial shell getprop ro.product.cpu.abilist) -join '').Trim().Split(',')
if ($LASTEXITCODE -ne 0) { throw 'Cannot query device ABIs.' }
$taskApk = Join-Path $taskPortRoot 'android\app\build\outputs\apk\debug\app-debug.apk'
$taskTests = Join-Path $taskPortRoot 'android\app\build\outputs\apk\androidTest\debug\app-debug-androidTest.apk'
foreach ($taskPath in @($taskApk, $taskTests)) {
    if (-not (Test-Path -LiteralPath $taskPath)) { throw "Build the dual-ABI APK/test APK first: $taskPath" }
}
foreach ($taskAbi in $Abis) {
    if ($taskAbi -notin $taskSupported) { throw "Device does not support $taskAbi" }
    & $Adb -s $Serial install -r --abi $taskAbi $taskApk
    if ($LASTEXITCODE -ne 0) { throw "APK installation failed for $taskAbi" }
    & $Adb -s $Serial install -r $taskTests
    if ($LASTEXITCODE -ne 0) { throw 'Test APK installation failed.' }
    $taskPackage = (& $Adb -s $Serial shell dumpsys package org.re4.port) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $taskPackage -notmatch "primaryCpuAbi=$taskAbi") {
        throw "Installed application ABI is not $taskAbi"
    }
    $taskBits = if ($taskAbi -eq 'armeabi-v7a') { 32 } else { 64 }
    & $Adb -s $Serial shell input keyevent KEYCODE_WAKEUP
    if ($LASTEXITCODE -ne 0) { throw 'Cannot wake display for the actual Surface test.' }
    $taskOutput = & $Adb -s $Serial shell am instrument -w `
        -e re4.expectedPointerBits $taskBits org.re4.port.test/androidx.test.runner.AndroidJUnitRunner 2>&1
    $taskExit = $LASTEXITCODE
    @("Serial=$Serial; ABI=$taskAbi; ExpectedPointerBits=$taskBits", "primaryCpuAbi=$taskAbi",
        "ApkSHA256=$((Get-FileHash -LiteralPath $taskApk -Algorithm SHA256).Hash)") + $taskOutput |
        Set-Content -LiteralPath (Join-Path $taskPortRoot "build\instrumentation-$taskAbi.txt") -Encoding UTF8
    $taskOutput | Write-Output
    # adb may return zero even when JUnit failed; inspect the instrumentation result.
    if ($taskExit -ne 0 -or ($taskOutput -join "`n") -notmatch 'OK \(20 tests\)') {
        throw "Actual app-process verification failed for $taskAbi"
    }
    $taskGraphics = (& $Adb -s $Serial shell run-as org.re4.port cat files/graphics-bootstrap.json) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read actual GPU verification evidence.' }
    $taskGpu = $taskGraphics | ConvertFrom-Json
    if ($taskGpu.graphics.pointer_bits -ne $taskBits -or $taskGpu.original_runtime.failed -ne 0 -or
        $taskGpu.graphics.verified_gpu_clears -lt 12 -or $taskGpu.graphics.presents -lt 12) {
        throw 'Actual GPU readback/presentation evidence does not match this app ABI.'
    }
    $taskGraphics | Set-Content -LiteralPath (Join-Path $taskPortRoot "build\graphics-$taskAbi.json") -Encoding UTF8
}

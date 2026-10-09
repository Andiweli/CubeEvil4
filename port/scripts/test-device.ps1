param(
    [Parameter(Mandatory = $true)][string]$Adb,
    [Parameter(Mandatory = $true)][string]$Serial,
    [ValidateSet('armeabi-v7a', 'arm64-v8a')]
    [string[]]$Abis = @('armeabi-v7a', 'arm64-v8a')
)
$ErrorActionPreference = 'Stop'
$taskPortRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath $Adb)) { throw "adb missing: $Adb" }
$taskSupportedText = & $Adb -s $Serial shell getprop ro.product.cpu.abilist
if ($LASTEXITCODE -ne 0) { throw 'Cannot query device ABIs; check device authorization.' }
$taskSupported = ($taskSupportedText -join '').Trim().Split(',')
foreach ($taskAbi in $Abis) {
    if ($taskAbi -notin $taskSupported) { throw "Device does not support requested ABI $taskAbi" }
    $taskExecutable = Join-Path $taskPortRoot "build\android-$taskAbi\re4_core_tests"
    if (-not (Test-Path -LiteralPath $taskExecutable)) {
        throw "Build the native test executable first: $taskExecutable"
    }
    $taskRemote = "/data/local/tmp/re4-port-tests/$taskAbi"
    & $Adb -s $Serial shell mkdir -p $taskRemote
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create device test directory.' }
    & $Adb -s $Serial push $taskExecutable "$taskRemote/re4_core_tests"
    if ($LASTEXITCODE -ne 0) { throw "Cannot deploy $taskAbi executable." }
    & $Adb -s $Serial shell chmod 755 "$taskRemote/re4_core_tests"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot set test executable permissions.' }
    $taskOutput = & $Adb -s $Serial shell "$taskRemote/re4_core_tests" 2>&1
    $taskResult = $LASTEXITCODE
    $taskLog = Join-Path $taskPortRoot "build\device-$taskAbi.txt"
    $taskOutput | Set-Content -LiteralPath $taskLog -Encoding UTF8
    $taskOutput | Write-Output
    if ($taskResult -ne 0 -or ($taskOutput -join "`n") -notmatch 'TOTAL: \d+ checks, 0 failures') {
        throw "Actual device verification failed for $taskAbi; see $taskLog"
    }
}

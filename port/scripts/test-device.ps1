param(
    [Parameter(Mandatory = $true)][string]$Adb,
    [Parameter(Mandatory = $true)][string]$Serial,
    [ValidateSet('armeabi-v7a', 'arm64-v8a')]
    [string[]]$Abis = @('armeabi-v7a', 'arm64-v8a'),
    [ValidateRange(1, 100)][int]$Repeat = 1
)
$ErrorActionPreference = 'Stop'
$taskPortRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath $Adb)) { throw "adb missing: $Adb" }
$taskSupportedText = & $Adb -s $Serial shell getprop ro.product.cpu.abilist
if ($LASTEXITCODE -ne 0) { throw 'Cannot query device ABIs; check device authorization.' }
$taskSupported = ($taskSupportedText -join '').Trim().Split(',')
$taskModel = (& $Adb -s $Serial shell getprop ro.product.model) -join ''
$taskVersion = (& $Adb -s $Serial shell getprop ro.build.version.release) -join ''
$taskApi = (& $Adb -s $Serial shell getprop ro.build.version.sdk) -join ''
if ($LASTEXITCODE -ne 0) { throw 'Cannot query device version.' }
$taskReferenceTrace = $null
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
    for ($taskRound = 1; $taskRound -le $Repeat; ++$taskRound) {
        $taskOutput = & $Adb -s $Serial shell "$taskRemote/re4_core_tests" 2>&1
        $taskResult = $LASTEXITCODE
        $taskLogName = if ($Repeat -eq 1) { "device-$taskAbi.txt" } else { "device-$taskAbi-round$taskRound.txt" }
        $taskLog = Join-Path $taskPortRoot "build\$taskLogName"
        @("Device=$taskModel; Serial=$Serial; Android=$taskVersion; API=$taskApi; ABI=$taskAbi",
            "ExecutableSHA256=$((Get-FileHash -LiteralPath $taskExecutable -Algorithm SHA256).Hash)") + $taskOutput |
            Set-Content -LiteralPath $taskLog -Encoding UTF8
        $taskOutput | Write-Output
        if ($taskResult -ne 0 -or ($taskOutput -join "`n") -notmatch 'TOTAL: \d+ checks, 0 failures') {
            throw "Actual device verification failed for $taskAbi; see $taskLog"
        }
        $taskBits = if ($taskAbi -eq 'armeabi-v7a') { 32 } else { 64 }
        if (($taskOutput -join "`n") -notmatch "pointer=$taskBits bits;") {
            throw "Wrong actual process pointer width for $taskAbi"
        }
        $taskTrace = @($taskOutput | Where-Object { $_ -match '^\s*TRACE (scheduler_|background|yz2)' } | ForEach-Object { $_.Trim() })
        if ($taskTrace.Count -ne 7) { throw 'Scheduler/background/decoder traces missing from native suite.' }
        $taskTraceText = $taskTrace -join "`n"
        if ($null -ne $taskReferenceTrace -and $taskTraceText -cne $taskReferenceTrace) {
            throw 'Repeated ARM32/ARM64 scheduler/background/decoder semantic traces differ.'
        }
        $taskReferenceTrace = $taskTraceText
    }
}
if ($Abis.Count -gt 1) { Write-Output 'PASS: scheduler semantic traces are identical across requested ABIs.' }

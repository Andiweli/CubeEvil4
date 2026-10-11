param(
    [switch]$Broker,
    [ValidateRange(0, 300000)][int]$TimeoutMs = 60000
)

# Keep this file byte-identical in BioShock and RE4-Fixed. Mutex ownership is
# thread-affine: WaitOne and ReleaseMutex both run on this PowerShell thread.
function Invoke-RetroidAdbExclusive {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Sequence,
        [ValidateRange(0, 300000)][int]$LockTimeoutMs = 60000,
        [switch]$ThrowOnUnavailable
    )
    $taskMutex = $null
    $taskAcquired = $false
    try {
        $taskMutex = [System.Threading.Mutex]::new($false, 'Global\Retroid_ADB_Exclusive')
        try {
            $taskAcquired = $taskMutex.WaitOne($LockTimeoutMs)
        } catch [System.Threading.AbandonedMutexException] {
            # WaitOne grants ownership when reporting abandonment. An old device
            # workload might survive the owner: release, skip, and require audit.
            $taskAcquired = $true
            [Console]::Error.WriteLine('RETROID_LOCK_ABANDONED: hardware skipped; inspect surviving device tests before retry.')
            if ($ThrowOnUnavailable) { throw 'RETROID_LOCK_ABANDONED: hardware pending' }
            return 74
        }
        if (-not $taskAcquired) {
            [Console]::Error.WriteLine('RETROID_LOCK_TIMEOUT: hardware skipped; no ADB command executed.')
            if ($ThrowOnUnavailable) { throw 'RETROID_LOCK_TIMEOUT: hardware pending' }
            return 73
        }
        & $Sequence
    } finally {
        if ($taskAcquired) { $taskMutex.ReleaseMutex() }
        if ($null -ne $taskMutex) { $taskMutex.Dispose() }
    }
}

if ($Broker) {
    # The Python runner owns stdin. EOF (including a dead runner) unwinds the
    # try/finally. During normal operation it sends release only after all ADB,
    # log validation and project-specific cleanup have finished.
    $taskResult = Invoke-RetroidAdbExclusive -LockTimeoutMs $TimeoutMs -Sequence {
        [Console]::Out.WriteLine('{"status":"acquired","mutex":"Global\\Retroid_ADB_Exclusive","owner_pid":' + $PID + '}')
        [Console]::Out.Flush()
        $null = [Console]::ReadLine()
        return 0
    }
    exit $taskResult
}

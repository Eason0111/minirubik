param(
    [ValidateSet('control', 'large')]
    [string]$Case = 'control'
)

$ErrorActionPreference = 'Stop'

function Read-SharedLog {
    param([string]$Path)
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite
    )
    $reader = $null
    try {
        $reader = [System.IO.StreamReader]::new($stream)
        return $reader.ReadToEnd()
    } finally {
        if ($null -ne $reader) { $reader.Dispose() }
        else { $stream.Dispose() }
    }
}

$ripesExe = 'C:\Users\User\Desktop\Ripes-v2.2.6-106-g5b8a616-win-x86_64\Ripes.exe'
$sourcePath = Join-Path $PSScriptRoot ("memory-{0}.s" -f $Case)
$sourceText = Get-Content -LiteralPath $sourcePath -Raw
$wordMatch = [regex]::Match($sourceText, 'li\s+t1,\s*(\d+)')
if (-not $wordMatch.Success) { throw 'Could not identify the write count.' }
$wordCount = [long]$wordMatch.Groups[1].Value
$guestBytes = 4 * $wordCount
# A retired count past the write loop establishes that sampling is in hold.
$readyThreshold = 4 * $wordCount + 8
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$runStem = Join-Path $PSScriptRoot ("{0}-{1}" -f $Case, $runStamp)
$stdoutPath = "$runStem-stdout.txt"
$stderrPath = "$runStem-stderr.txt"
$arguments = @('--mode', 'cli', '--proc', 'RV32_ISS', '--src', ('"' + $sourcePath + '"'), '-t', 'asm', '--timeout', '8000', '-v')
$samples = [System.Collections.Generic.List[object]]::new()
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$ripesProcess = Start-Process -FilePath $ripesExe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
try {
    while ($timer.ElapsedMilliseconds -lt 15000) {
        Start-Sleep -Milliseconds 250
        $ripesProcess.Refresh()
        if ($ripesProcess.HasExited) { break }
        $runLog = Read-SharedLog -Path $stdoutPath
        $retiredMatches = [regex]::Matches($runLog, 'retired:\s*(\d+)')
        if ($retiredMatches.Count -eq 0) { continue }
        $retired = [long]$retiredMatches[$retiredMatches.Count - 1].Groups[1].Value
        if ($retired -lt $readyThreshold) { continue }
        $ripesProcess.Refresh()
        if ($ripesProcess.HasExited) { break }
        $samples.Add([pscustomobject]@{
            Case = $Case
            ProcessId = $ripesProcess.Id
            ElapsedMs = $timer.ElapsedMilliseconds
            GuestBytes = $guestBytes
            LoggedRetired = $retired
            PrivateBytes = $ripesProcess.PrivateMemorySize64
            WorkingSetBytes = $ripesProcess.WorkingSet64
        })
    }
    if (-not $ripesProcess.HasExited) { throw 'Ripes exceeded the outer time limit.' }
    $ripesProcess.WaitForExit()
    $stdoutLog = Get-Content -LiteralPath $stdoutPath -Raw
    $stderrLog = Get-Content -LiteralPath $stderrPath -Raw
    if (($stdoutLog + $stderrLog) -notmatch 'Simulation did not finish within the specified timeout \(8000 ms\)') {
        throw ("Expected the hold-loop timeout. Inspect {0} and {1}." -f $stdoutPath, $stderrPath)
    }
    if ($samples.Count -lt 5) { throw 'Fewer than five samples after completion of the writes.' }
    $samples | Export-Csv -LiteralPath "$runStem-samples.csv" -NoTypeInformation -Encoding UTF8
    $lastFive = @($samples | Select-Object -Last 5)
    $privateStats = $lastFive | Measure-Object -Property PrivateBytes -Average -Minimum -Maximum
    $summary = [pscustomobject]@{
        Case = $Case
        RipesVersion = 'v2.2.6-106-g5b8a616'
        Processor = 'RV32_ISS'
        GuestBytes = $guestBytes
        SampleCount = $samples.Count
        Last5PrivateBytesMean = [long][Math]::Round($privateStats.Average)
        Last5PrivateBytesMin = [long]$privateStats.Minimum
        Last5PrivateBytesMax = [long]$privateStats.Maximum
        ExpectedTimeoutObserved = $true
        SamplesFile = "$runStem-samples.csv"
        SourceSha256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    }
    $summary | ConvertTo-Json | Set-Content -LiteralPath "$runStem-summary.json" -Encoding UTF8
    $summary | Format-List
} finally {
    $timer.Stop()
    $ripesProcess.Refresh()
    if (-not $ripesProcess.HasExited) { $ripesProcess.Kill(); $ripesProcess.WaitForExit() }
    $ripesProcess.Dispose()
}

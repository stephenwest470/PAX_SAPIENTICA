# Live progress of a PAXS_BatchSimulator batch: one line per model plus recent batch events.
# Usage (from the repository root):  .\Scripts\WatchBatch.ps1 SimulationResults/Yayoi-Sim [-Runs 5] [-Seconds 30]
param(
    [Parameter(Mandatory = $true)][string]$BatchDir,
    [int]$Runs = 5,
    [int]$Seconds = 30
)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root $BatchDir
$totalSteps = 13800

while ($true) {
    $lines = @()
    $lines += "PAXS batch progress  $BatchDir   $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')   (refreshes every $Seconds s, Ctrl+C to stop)"
    $lines += ""
    $lines += "{0,-3} {1,-9} {2}" -f "@", "complete", "running"
    $doneAll = 0
    $models = Import-Csv (Join-Path $dir "Batch.tsv") -Delimiter "`t"
    foreach ($m in $models) {
        $modelDir = Join-Path $dir $m.id
        $complete = 0
        $running = @()
        if (Test-Path $modelDir) {
            foreach ($run in Get-ChildItem $modelDir -Directory | Where-Object Name -match '^\d{4}-') {
                if (Test-Path (Join-Path $run.FullName "RunSummary.tsv")) { $complete++; continue }
                $log = Get-ChildItem $modelDir -Filter "run_*.log" | Sort-Object LastWriteTime | Select-Object -Last 1
                $last = if ($log) { Get-Content $log.FullName -Tail 1 } else { "" }
                if ($last -match 'step (\d+)/(\d+)\s+(.+?)\s+population (\d+)\s+elapsed ([\d.]+) min') {
                    $pct = [math]::Round(100 * [int]$Matches[1] / [int]$Matches[2])
                    $running += ("{0,3}%  {1,-8} pop {2,9:N0}  {3,6} min" -f $pct, $Matches[3], [int]$Matches[4], $Matches[5])
                } else {
                    $running += "starting"
                }
            }
        }
        $doneAll += [math]::Min($complete, $Runs)
        $bar = ('#' * [math]::Min($complete, $Runs)) + ('.' * [math]::Max(0, $Runs - $complete))
        $lines += "{0,-3} {1,-9} {2}" -f $m.id, "$bar $complete/$Runs", ($running -join "; ")
    }
    $lines += ""
    $lines += "Total: $doneAll / $($models.Count * $Runs) runs complete"
    $lines += ""
    $lines += "Recent events:"
    $batchLog = Join-Path $dir "batch.log"
    if (Test-Path $batchLog) {
        $lines += Get-Content $batchLog | Select-String -Pattern '\] (start|done|FAILED)' | Select-Object -Last 8 | ForEach-Object { "  " + $_.Line }
    }
    $running = Get-Process PAXS_BatchSimulator -ErrorAction SilentlyContinue
    if (-not $running) { $lines += ""; $lines += "No PAXS_BatchSimulator process is running." }
    Clear-Host
    $lines | ForEach-Object { Write-Host $_ }
    Start-Sleep -Seconds $Seconds
}

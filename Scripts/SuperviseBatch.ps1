# Unattended supervisor for the Yayoi-Sim-v2 replicate experiment.
# - keeps Windows from idle-sleeping while it runs (SetThreadExecutionState; no settings are changed)
# - runs the phases in order, retrying each until the runner reports nothing left to run
# - kills a run whose log has not grown for $StallMinutes of awake time (the runner then redoes it with the same seed)
# - runs the Table 1 analysis at the end
param(
    [string]$Root = (Split-Path $PSScriptRoot -Parent),   # repository root (this script lives in Scripts/)
    [string]$Out = "SimulationResults/Yayoi-Sim-v2",
    [string]$Def = "Data/Simulations/Yayoi-Sim-v2",
    [string]$SeedBase = "20261105",
    [string]$MaxPopulation = "5000000",
    [int]$StallMinutes = 45,
    [int]$MaxAttempts = 4,
    [datetime]$Deadline = (Get-Date).AddHours(70),
    [int]$Steps = 0,                 # testing only: passed to the runner as --steps
    [string[]]$OnlyPhases = @()      # testing only: run just these phase names
)
$ErrorActionPreference = "Continue"
Set-Location $Root
$exe = Join-Path $Root "build\BatchSimulator\PAXS_BatchSimulator.exe"
$outDir = Join-Path $Root ($Out -replace "/", "\")
New-Item -ItemType Directory -Force $outDir | Out-Null
$status = Join-Path $outDir "supervisor.log"
function Log($msg) { $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg; Add-Content -Path $status -Value $line -Encoding UTF8 }

Add-Type -Namespace Win32 -Name Power -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint esFlags);
'@
# ES_CONTINUOUS | ES_SYSTEM_REQUIRED: hold the system awake until this process exits
[void][Win32.Power]::SetThreadExecutionState([uint32]"0x80000001")

$phases = @()
foreach ($k in 1..5) {
    $phases += @{ name = "main-round$k"; file = "Main.tsv"; runs = $k; jobs = 2 }
    $phases += @{ name = "P-round$k"; file = "P.tsv"; runs = $k; jobs = 1 }
}
$phases += @{ name = "E-all"; file = "E.tsv"; runs = 5; jobs = 1 }

function RunChildren {
    Get-CimInstance Win32_Process -Filter "Name='PAXS_BatchSimulator.exe'" | Where-Object { $_.CommandLine -match ' run ' -and $_.CommandLine -match '--seed (\d+)' }
}

Log "supervisor started (pid $PID); deadline for starting new phases: $Deadline"
$OnlyPhases = @($OnlyPhases | ForEach-Object { $_ -split "," } | Where-Object { $_ })
if ($OnlyPhases.Count -gt 0) { $phases = @($phases | Where-Object { $OnlyPhases -contains $_.name }) }
$stepArg = if ($Steps -gt 0) { " --steps $Steps" } else { "" }
$defDir = Join-Path $Root ($Def -replace "/", "\")
foreach ($ph in $phases) {
    if ((Get-Date) -gt $Deadline) { Log "deadline passed; not starting $($ph.name)"; break }
    $done = $false
    for ($attempt = 1; $attempt -le $MaxAttempts -and -not $done; $attempt++) {
        $stdout = Join-Path $outDir ("batch_{0}_try{1}.out" -f $ph.name, $attempt)
        $stderr = Join-Path $outDir ("batch_{0}_try{1}.err" -f $ph.name, $attempt)
        $argList = "batch $Def/$($ph.file) --runs $($ph.runs) --jobs $($ph.jobs) --seed-base $SeedBase --max-population $MaxPopulation --no-analyze --out $Out$stepArg"
        $p = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $Root -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        $null = $p.Handle   # keep the handle so ExitCode is available later
        Log "start $($ph.name) attempt $attempt (batch pid $($p.Id))"
        $track = @{}; $last = Get-Date; $noChildSince = Get-Date
        while (-not $p.HasExited) {
            Start-Sleep -Seconds 60
            $now = Get-Date
            if (($now - $last).TotalMinutes -gt 5) {   # machine was asleep: restart all stall clocks
                Log ("resumed after a {0:N0}-minute gap; stall clocks reset" -f ($now - $last).TotalMinutes)
                foreach ($key in @($track.Keys)) { $track[$key].changed = $now }
            }
            $last = $now
            $children = @(RunChildren)
            if ($children.Count -gt 0) { $noChildSince = $now }
            elseif (($now - $noChildSince).TotalMinutes -gt 30) {
                Log "batch pid $($p.Id) has had no running simulation for 30 min; stopping it for a retry"
                Stop-Process -Id $p.Id -Force; break
            }
            foreach ($c in $children) {
                $seed = [regex]::Match($c.CommandLine, '--seed (\d+)').Groups[1].Value
                $logFile = Get-Item -Path (Join-Path $outDir "*un_*_seed_$seed.log") -ErrorAction SilentlyContinue | Select-Object -First 1
                if (-not $logFile) { continue }
                $key = "$($c.ProcessId)"
                if (-not $track.ContainsKey($key)) { $track[$key] = @{ size = $logFile.Length; changed = $now } }
                elseif ($logFile.Length -ne $track[$key].size) { $track[$key].size = $logFile.Length; $track[$key].changed = $now }
                elseif (($now - $track[$key].changed).TotalMinutes -ge $StallMinutes) {
                    $tail = (Get-Content $logFile.FullName -Tail 1)
                    Log "HUNG: $($logFile.Directory.Name) seed $seed (pid $($c.ProcessId)) log unchanged for $StallMinutes min at '$tail'; killing it for a same-seed retry"
                    Stop-Process -Id $c.ProcessId -Force
                    $track.Remove($key)
                }
            }
        }
        $p.WaitForExit()
        $text = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
        $failed = ([regex]::Matches("$text", 'FAILED')).Count
        Log "end $($ph.name) attempt $attempt (exit $($p.ExitCode), $failed failed)"
        # done when every model in this phase has the required number of finished runs (result folders with RunSummary.tsv)
        $remaining = 0
        $counts = @{}
        foreach ($row in (Get-Content (Join-Path $defDir $ph.file) | Select-Object -Skip 1)) {
            $id = ($row -split "`t")[0]
            $n = @(Get-ChildItem (Join-Path $outDir $id) -Directory -ErrorAction SilentlyContinue | Where-Object { Test-Path (Join-Path $_.FullName "RunSummary.tsv") }).Count
            $counts[$id] = $n
            if ($n -lt $ph.runs) { $remaining += ($ph.runs - $n) }
        }
        Log ("complete runs: " + (($counts.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join " ") + "; remaining $remaining")
        if ($remaining -eq 0) { $done = $true }
    }
    if (-not $done) { Log "WARNING: $($ph.name) still incomplete after $MaxAttempts attempts; moving on" }
}

Log "running analysis"
Start-Process -FilePath $exe -ArgumentList "analyze $Out" -WorkingDirectory $Root -WindowStyle Hidden -Wait `
    -RedirectStandardOutput (Join-Path $outDir "analyze.out") -RedirectStandardError (Join-Path $outDir "analyze.err")
Log "ALL DONE"

# Holds the system AND display awake (needed on Modern Standby laptops, where a system-only request does not stop
# screen-off standby) for as long as the given process is alive. No settings are changed; the hold ends when this exits.
param([int]$WatchPid, [string]$LogFile)
Add-Type -Namespace Win32 -Name Power2 -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint esFlags);
'@
# ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
[void][Win32.Power2]::SetThreadExecutionState([uint32]"0x80000003")
if ($LogFile) { Add-Content $LogFile ("[{0}] keep-awake helper (pid {1}) holding system+display awake while pid {2} runs" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $PID, $WatchPid) }
while (Get-Process -Id $WatchPid -ErrorAction SilentlyContinue) {
    Start-Sleep -Seconds 60
    [void][Win32.Power2]::SetThreadExecutionState([uint32]"0x80000003")
}
if ($LogFile) { Add-Content $LogFile ("[{0}] keep-awake helper exiting (pid {1} ended)" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $WatchPid) }

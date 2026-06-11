# dev-qemu-session.ps1 — robust automated QEMU monitor session over a named pipe.
#
# QEMU's win32 pipe chardev BLOCKS the whole VM if nobody drains the monitor
# output. This script connects immediately, drains continuously using a
# NON-blocking check (PeekNamedPipe), and executes a timed list of commands.
# Non-blocking matters: a pending blocking read + a write on the same pipe
# handle corrupts the connection.
#
# $Script entries: @{ at = <seconds since start>; cmd = '<monitor command>' }
param(
    [array]$Script,
    [string]$PipeName = 'kamikaze',
    [int]$TotalSeconds = 20,
    [switch]$ShowOutput
)

Add-Type -Name Native -Namespace Kmz -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true)]
public static extern bool PeekNamedPipe(Microsoft.Win32.SafeHandles.SafePipeHandle h,
    byte[] buffer, uint bufferSize, ref uint bytesRead,
    ref uint totalAvail, ref uint bytesLeft);
'@

$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', $PipeName, [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
$enc = [System.Text.Encoding]::ASCII
$buf = New-Object byte[] 65536
$start = Get-Date
$pending = @($Script | Sort-Object { $_.at })
$i = 0
$collected = ''

while (((Get-Date) - $start).TotalSeconds -lt $TotalSeconds) {
    $elapsed = ((Get-Date) - $start).TotalSeconds

    # Send any commands whose scheduled time has come.
    while ($i -lt $pending.Count -and $pending[$i].at -le $elapsed) {
        $bytes = $enc.GetBytes($pending[$i].cmd + "`n")
        $pipe.Write($bytes, 0, $bytes.Length)
        $pipe.Flush()
        $i++
    }

    # Drain only if data is actually available (non-blocking peek).
    $read = 0; $avail = 0; $left = 0
    [void][Kmz.Native]::PeekNamedPipe($pipe.SafePipeHandle, $null, 0,
                                      [ref]$read, [ref]$avail, [ref]$left)
    if ($avail -gt 0) {
        $n = $pipe.Read($buf, 0, [Math]::Min($avail, $buf.Length))
        if ($n -gt 0) { $collected += $enc.GetString($buf, 0, $n) }
    } else {
        Start-Sleep -Milliseconds 50
    }
}
$pipe.Close()
if ($ShowOutput) {
    # Strip ANSI escape noise from the monitor's line editor.
    $collected -replace '\x1b\[[0-9;]*[A-Za-z]', '' -replace '[^\x09\x0a\x0d\x20-\x7e]', ''
}
'session done'

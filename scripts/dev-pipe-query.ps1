# dev-pipe-query.ps1 — send monitor commands AND read back the responses.
# All commands are written in one shot, then we drain the pipe until quiet
# (a named pipe handle can't have a pending read while writing).
param(
    [string[]]$Commands,
    [string]$PipeName = 'kamikaze',
    [int]$SettleMs = 1500
)
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', $PipeName, [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(3000)
$enc = [System.Text.Encoding]::ASCII

$payload = ($Commands -join "`n") + "`n"
$bytes = $enc.GetBytes($payload)
$pipe.Write($bytes, 0, $bytes.Length)
$pipe.Flush()
Start-Sleep -Milliseconds $SettleMs

$buf = New-Object byte[] 65536
$out = ''
while ($true) {
    $task = $pipe.ReadAsync($buf, 0, $buf.Length)
    if (-not $task.Wait(700)) { break }
    $n = $task.Result
    if ($n -le 0) { break }
    $out += $enc.GetString($buf, 0, $n)
}
# Strip the ANSI cursor-movement noise QEMU's line editor echoes back.
$out -replace '\x1b\[[0-9;]*[A-Za-z]', '' -replace '[^\x09\x0a\x0d\x20-\x7e]', ''
$pipe.Close()

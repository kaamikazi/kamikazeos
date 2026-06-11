# dev-pipe-monitor.ps1 — drive QEMU's monitor over a Windows named pipe.
# Start QEMU with:  -monitor pipe:kamikaze
# Usage: .\dev-pipe-monitor.ps1 -Commands @('info status', 'screendump x.png -f png')
param(
    [string[]]$Commands,
    [string]$PipeName = 'kamikaze',
    [int]$DelayMs = 350
)
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', $PipeName, [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(3000)
$enc = [System.Text.Encoding]::ASCII
foreach ($cmd in $Commands) {
    $bytes = $enc.GetBytes($cmd + "`n")
    $pipe.Write($bytes, 0, $bytes.Length)
    $pipe.Flush()
    Start-Sleep -Milliseconds $DelayMs
}
$pipe.Close()

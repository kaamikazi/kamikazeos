# dev-monitor.ps1 — send commands to a running QEMU monitor over TCP.
# Used for automated boot testing (screenshots, injected keystrokes).
# Usage: .\dev-monitor.ps1 -Commands @('screendump shot.png -f png')
param(
    [string[]]$Commands,
    [int]$Port = 45454,
    [int]$DelayMs = 400
)
$client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $Port)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.NewLine = "`n"
$writer.AutoFlush = $true
Start-Sleep -Milliseconds 300   # let the monitor banner go by
foreach ($cmd in $Commands) {
    $writer.WriteLine($cmd)
    Start-Sleep -Milliseconds $DelayMs
}
$writer.Close(); $client.Close()

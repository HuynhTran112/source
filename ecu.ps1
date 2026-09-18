# Tool tuong tac truc tiep voi Zephyr Shell ECU tren COM4 tu PowerShell
param (
    [string]$Command = ""
)

$portName = "COM4"
$baudRate = 115200

function Send-EcuCommand($cmd) {
    $port = New-Object System.IO.Ports.SerialPort $portName, $baudRate, "None", 8, "One"
    $port.ReadTimeout = 1000
    try {
        $port.Open()
        Start-Sleep -Milliseconds 80
        $port.WriteLine($cmd)
        Start-Sleep -Milliseconds 350
        $out = $port.ReadExisting()
        Write-Host $out
    } catch {
        Write-Host "Loi ket noi $portName : $($_.Exception.Message)" -ForegroundColor Red
    } finally {
        if ($port.IsOpen) { $port.Close() }
    }
}

if ($Command -ne "") {
    Send-EcuCommand $Command
} else {
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "   CONG DONG LENH CHAN DOAN ZEPHYR ECU (COM4)     " -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "Goi y lenh: 'vehicle status', 'dtc read', 'help'..." -ForegroundColor Yellow
    Write-Host "(Go 'exit' de thoat ve PowerShell)`n" -ForegroundColor Yellow

    while ($true) {
        $inputCmd = Read-Host "ecu:~$"
        if ($inputCmd -eq "exit" -or $inputCmd -eq "quit") { break }
        if ($inputCmd.Trim() -ne "") {
            Send-EcuCommand $inputCmd
        }
    }
}

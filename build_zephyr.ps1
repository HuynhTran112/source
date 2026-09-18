# Script build & flash nhanh du an Zephyr CAN Gateway
param (
    [string]$Action = "flash"
)

$west = "D:\zephyrproject\.venv\Scripts\west.exe"
$projDir = Join-Path $PSScriptRoot "zephyr_project"
$buildDir = Join-Path $projDir "build"
$programmer = "D:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe"

if ($Action -eq "clean") {
    Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    Write-Host "Clean completed."
    exit 0
}

Write-Host "Building Zephyr CAN Gateway (stm32f746g_disco)..."
& $west build -b stm32f746g_disco $projDir -d $buildDir
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

Write-Host "BUILD SUCCESS: zephyr.hex generated."

if ($Action -eq "flash") {
    Write-Host "Flashing to STM32F7 via ST-LINK..."
    $hex = Join-Path $buildDir "zephyr\zephyr.hex"
    & $programmer -c port=SWD mode=UR -w $hex -v -rst
}

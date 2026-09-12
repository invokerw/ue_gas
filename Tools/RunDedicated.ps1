[CmdletBinding()]
param(
    [int]$Port = 7859,
    [int]$TimeoutSeconds = 90,
    [string]$PythonExe = 'python'
)

$ErrorActionPreference = 'Stop'
$dedicatedRepoRoot = Split-Path $PSScriptRoot -Parent
$dedicatedProjectFile = Join-Path $dedicatedRepoRoot 'ue_gas.uproject'
$dedicatedEnvironmentTool = Join-Path $PSScriptRoot 'ue_environment.py'
$dedicatedOutputRoot = Join-Path $dedicatedRepoRoot 'Saved/UEEnvironment/Dedicated'
$dedicatedServerLog = Join-Path $dedicatedOutputRoot 'DedicatedServer.log'
$dedicatedProcesses = @()

if (-not (Test-Path -LiteralPath $dedicatedProjectFile -PathType Leaf)) {
    throw "项目文件不存在：$dedicatedProjectFile"
}
if (-not (Test-Path -LiteralPath $dedicatedEnvironmentTool -PathType Leaf)) {
    throw "环境检查工具不存在：$dedicatedEnvironmentTool"
}

$dedicatedCheckJson = & $PythonExe $dedicatedEnvironmentTool check --repo-root $dedicatedRepoRoot --require dedicated --json 2>&1
$dedicatedCheckExitCode = $LASTEXITCODE
if ($dedicatedCheckExitCode -ne 0) {
    $dedicatedDiagnostics = ($dedicatedCheckJson -join [Environment]::NewLine)
    throw "Dedicated Server smoke 未运行：UE_SOURCE_EDITOR 或源码引擎 Build.bat 未通过检查。`n$dedicatedDiagnostics"
}
$dedicatedEnvironment = ($dedicatedCheckJson -join [Environment]::NewLine) | ConvertFrom-Json
$dedicatedEngineExe = [string]$dedicatedEnvironment.source_editor

New-Item -ItemType Directory -Path $dedicatedOutputRoot -Force | Out-Null
$dedicatedCommonArgs = @(
    '-unattended', '-NoSplash', '-NullRHI', '-NoSound', '-NoP4',
    '-CombatHUDSmoke', '-CombatSAMMovementSmoke', '-ini:Engine:[ConsoleVariables]:t.MaxFPS=120'
)

function Quote-DedicatedArgument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

try {
    $dedicatedServerArgs = @(
        (Quote-DedicatedArgument $dedicatedProjectFile),
        '/Game/Combat/Tests/L_CombatTest?game=/Game/Combat/Demo/Framework/BP_CombatDemoGameMode.BP_CombatDemoGameMode_C',
        '-server', "-port=$Port", '-CombatM7CapacitySmoke', '-ModelContextProtocolPort=8041'
    ) + $dedicatedCommonArgs + @("-AbsLog=$(Quote-DedicatedArgument $dedicatedServerLog)")
    $dedicatedServer = Start-Process -FilePath $dedicatedEngineExe -ArgumentList $dedicatedServerArgs -WindowStyle Hidden -PassThru
    $dedicatedProcesses += $dedicatedServer

    $dedicatedDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Seconds 2
        $dedicatedServer.Refresh()
        if ($dedicatedServer.HasExited) { throw 'Dedicated Server 提前退出' }
        $dedicatedReady = (Test-Path -LiteralPath $dedicatedServerLog) -and ((Get-Content -LiteralPath $dedicatedServerLog -Raw -ErrorAction SilentlyContinue) -match "listening on port $Port")
    } until ($dedicatedReady -or (Get-Date) -gt $dedicatedDeadline)
    if (-not $dedicatedReady) { throw 'Dedicated Server 监听超时' }

    foreach ($dedicatedIndex in 1..2) {
        $dedicatedClientLog = Join-Path $dedicatedOutputRoot ("DedicatedClient$dedicatedIndex.log")
        $dedicatedClientArgs = @(
            (Quote-DedicatedArgument $dedicatedProjectFile),
            "127.0.0.1:$Port", '-game', "-ModelContextProtocolPort=$([int](8040 + $dedicatedIndex))"
        ) + $dedicatedCommonArgs + @("-AbsLog=$(Quote-DedicatedArgument $dedicatedClientLog)")
        $dedicatedProcesses += Start-Process -FilePath $dedicatedEngineExe -ArgumentList $dedicatedClientArgs -WindowStyle Hidden -PassThru
    }
    $dedicatedProcesses.Id | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dedicatedOutputRoot 'DedicatedProcessIds.json')

    $dedicatedDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Seconds 3
        $dedicatedLogs = @($dedicatedServerLog, (Join-Path $dedicatedOutputRoot 'DedicatedClient1.log'), (Join-Path $dedicatedOutputRoot 'DedicatedClient2.log'))
        $dedicatedReports = foreach ($dedicatedLogPath in $dedicatedLogs) {
            if (Test-Path -LiteralPath $dedicatedLogPath) {
                Get-Content -LiteralPath $dedicatedLogPath | Where-Object { $_ -match 'HUDNetworkSnapshot|SAMCollisionServerResult|M7ScenarioReady' }
            }
        }
        $dedicatedFinished = @($dedicatedReports | Where-Object { $_ -match 'HUDNetworkSnapshot' }).Count -ge 3
    } until ($dedicatedFinished -or (Get-Date) -gt $dedicatedDeadline)
    $dedicatedReports | Set-Content -LiteralPath (Join-Path $dedicatedOutputRoot 'DedicatedSummary.txt')
    $dedicatedReports
    if (-not $dedicatedFinished) { throw 'HUD 联机快照等待超时' }
    if (@($dedicatedReports | Where-Object { $_ -match 'Result=Fail' }).Count -gt 0) { throw 'HUD 或移动联机检查失败' }
}
finally {
    foreach ($dedicatedProcess in $dedicatedProcesses) {
        $dedicatedProcess.Refresh()
        if (-not $dedicatedProcess.HasExited) {
            Stop-Process -Id $dedicatedProcess.Id -ErrorAction SilentlyContinue
        }
    }
}

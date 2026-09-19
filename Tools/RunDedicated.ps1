[CmdletBinding()]
param(
    [int]$Port = 7859,
    [int]$TimeoutSeconds = 90,
    [string]$PythonExe = 'python',
    [switch]$Items,
    [switch]$Economy,
    [switch]$InstalledEditor
)

$ErrorActionPreference = 'Stop'
if ($Items -and $Economy) { throw '物品争抢会切换主控单位；请分别运行 -Items 和 -Economy。' }
if ($Economy -and -not $PSBoundParameters.ContainsKey('TimeoutSeconds')) { $TimeoutSeconds = 390 }
$dedicatedRepoRoot = Split-Path $PSScriptRoot -Parent
$dedicatedProjectFile = Join-Path $dedicatedRepoRoot 'ue_gas.uproject'
$dedicatedEnvironmentTool = Join-Path $PSScriptRoot 'ue_environment.py'
$dedicatedOutputRoot = Join-Path $dedicatedRepoRoot $(if ($InstalledEditor) { 'Saved/UEEnvironment/Dedicated-Installed' } else { 'Saved/UEEnvironment/Dedicated' })
if ($Economy) { $dedicatedOutputRoot += '-Economy' }
$dedicatedServerLog = Join-Path $dedicatedOutputRoot 'DedicatedServer.log'
$dedicatedProcesses = @()

if (-not (Test-Path -LiteralPath $dedicatedProjectFile -PathType Leaf)) {
    throw "项目文件不存在：$dedicatedProjectFile"
}
if (-not (Test-Path -LiteralPath $dedicatedEnvironmentTool -PathType Leaf)) {
    throw "环境检查工具不存在：$dedicatedEnvironmentTool"
}

$dedicatedRequirement = if ($InstalledEditor) { 'editor' } else { 'dedicated' }
$dedicatedCheckJson = & $PythonExe $dedicatedEnvironmentTool check --repo-root $dedicatedRepoRoot --require $dedicatedRequirement --json 2>&1
$dedicatedCheckExitCode = $LASTEXITCODE
if ($dedicatedCheckExitCode -ne 0) {
    $dedicatedDiagnostics = ($dedicatedCheckJson -join [Environment]::NewLine)
    throw "Dedicated Server smoke 未运行：所选 UE Editor 入口未通过检查。`n$dedicatedDiagnostics"
}
$dedicatedEnvironment = ($dedicatedCheckJson -join [Environment]::NewLine) | ConvertFrom-Json
$dedicatedEngineExe = if ($InstalledEditor) { [string]$dedicatedEnvironment.installed_editor } else { [string]$dedicatedEnvironment.source_editor }

New-Item -ItemType Directory -Path $dedicatedOutputRoot -Force | Out-Null
$dedicatedCommonArgs = @(
    '-unattended', '-NoSplash', '-NullRHI', '-NoSound', '-NoP4',
    '-CombatHUDSmoke', '-CombatSAMMovementSmoke', '-ini:Engine:[ConsoleVariables]:t.MaxFPS=120'
)
if ($Items) { $dedicatedCommonArgs += '-CombatItemsSmoke' }
if ($Economy) { $dedicatedCommonArgs += '-CombatEconomySmoke'; $dedicatedCommonArgs += '-CombatEconomySoak' }

function Quote-DedicatedArgument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

try {
    $dedicatedRunStartedUtc = [DateTime]::UtcNow
    $dedicatedServerArgs = @(
        (Quote-DedicatedArgument $dedicatedProjectFile),
        '/Game/Combat/Tests/L_CombatTest?game=/Game/Combat/Demo/Framework/BP_CombatDemoGameMode.BP_CombatDemoGameMode_C',
        '-server', "-port=$Port", '-ModelContextProtocolPort=8040'
    ) + $dedicatedCommonArgs + @("-AbsLog=$(Quote-DedicatedArgument $dedicatedServerLog)")
    if (-not $Economy) { $dedicatedServerArgs += '-CombatM7CapacitySmoke' }
    $dedicatedServer = Start-Process -FilePath $dedicatedEngineExe -ArgumentList $dedicatedServerArgs -WindowStyle Hidden -PassThru
    $dedicatedProcesses += $dedicatedServer

    $dedicatedDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Seconds 2
        $dedicatedServer.Refresh()
        if ($dedicatedServer.HasExited) { throw 'Dedicated Server 提前退出' }
        $dedicatedReady = (Test-Path -LiteralPath $dedicatedServerLog) -and
            ((Get-Item -LiteralPath $dedicatedServerLog).LastWriteTimeUtc -ge $dedicatedRunStartedUtc) -and
            ((Get-Content -LiteralPath $dedicatedServerLog -Raw -ErrorAction SilentlyContinue) -match "listening on port $Port")
    } until ($dedicatedReady -or (Get-Date) -gt $dedicatedDeadline)
    if (-not $dedicatedReady) { throw 'Dedicated Server 监听超时' }

    foreach ($dedicatedIndex in 1..2) {
        $dedicatedClientLog = Join-Path $dedicatedOutputRoot ("DedicatedClient$dedicatedIndex.log")
        $dedicatedClientArgs = @(
            (Quote-DedicatedArgument $dedicatedProjectFile),
            "127.0.0.1:$Port", '-game', "-ModelContextProtocolPort=$([int](8040 + $dedicatedIndex))"
        ) + $dedicatedCommonArgs + @("-AbsLog=$(Quote-DedicatedArgument $dedicatedClientLog)")
        if ($Economy) { $dedicatedClientArgs += "-CombatEconomyReport=DedicatedClient$dedicatedIndex" }
        $dedicatedProcesses += Start-Process -FilePath $dedicatedEngineExe -ArgumentList $dedicatedClientArgs -WindowStyle Hidden -PassThru
    }
    $dedicatedProcesses.Id | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dedicatedOutputRoot 'DedicatedProcessIds.json')

    $dedicatedDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Seconds 3
        $dedicatedLogs = @($dedicatedServerLog, (Join-Path $dedicatedOutputRoot 'DedicatedClient1.log'), (Join-Path $dedicatedOutputRoot 'DedicatedClient2.log'))
        $dedicatedReports = foreach ($dedicatedLogPath in $dedicatedLogs) {
            if ((Test-Path -LiteralPath $dedicatedLogPath) -and
                ((Get-Item -LiteralPath $dedicatedLogPath).LastWriteTimeUtc -ge $dedicatedRunStartedUtc)) {
                Get-Content -LiteralPath $dedicatedLogPath | Where-Object { $_ -match 'HUDNetworkSnapshot|SAMCollisionServerResult|M7ScenarioReady|ItemNetworkSmoke|ItemNetworkContention|EconomyNetworkSmoke|EconomyNetworkCycle|M7Performance' }
            }
        }
        $dedicatedFinished = @($dedicatedReports | Where-Object { $_ -match 'HUDNetworkSnapshot' }).Count -ge 3
        if ($Items) { $dedicatedFinished = $dedicatedFinished -and @($dedicatedReports | Where-Object { $_ -match 'ItemNetworkSmoke' }).Count -ge 3 }
        if ($Economy) { $dedicatedFinished = $dedicatedFinished -and @($dedicatedReports | Where-Object { $_ -match 'EconomyNetworkSmoke' }).Count -ge 2 }
    } until ($dedicatedFinished -or (Get-Date) -gt $dedicatedDeadline)
    $dedicatedReports | Set-Content -LiteralPath (Join-Path $dedicatedOutputRoot 'DedicatedSummary.txt')
    $dedicatedReports
    if (-not $dedicatedFinished) { throw 'HUD 或物品联机快照等待超时' }
    if (@($dedicatedReports | Where-Object { $_ -match 'Result=Fail|Budget=Fail|CapacityFixture=Invalid' }).Count -gt 0) { throw 'HUD、物品、移动或容量联机检查失败' }
    if ($Items -and (@($dedicatedReports | Where-Object { $_ -match 'ItemNetworkContention.*Outcome=Won' }).Count -ne 1 -or
                    @($dedicatedReports | Where-Object { $_ -match 'ItemNetworkContention.*Outcome=Lost' }).Count -ne 1)) {
        throw '物品竞争必须恰好产生一个胜者和一个失败回执'
    }
    if ($Economy -and @($dedicatedReports | Where-Object { $_ -match 'EconomyNetworkSmoke.*Result=Pass' }).Count -ne 2) {
        throw '经济联机 soak 没有产生通过结果'
    }
}
finally {
    foreach ($dedicatedProcess in $dedicatedProcesses) {
        $dedicatedProcess.Refresh()
        if (-not $dedicatedProcess.HasExited) {
            Stop-Process -Id $dedicatedProcess.Id -ErrorAction SilentlyContinue
        }
    }
}

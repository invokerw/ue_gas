param([ValidateRange(1,2)][int]$Players = 1, [switch]$Defeat, [switch]$Choices, [switch]$TeamShop, [switch]$Reinforcement, [switch]$Siege, [switch]$Assassin, [switch]$Guardian, [switch]$Events, [switch]$Equipment, [switch]$UtilityEquipment, [switch]$MerchantSeal, [switch]$ReturnBeacon, [string]$EvidenceFolder = "DEF-010", [int]$Port = 7880, [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$Repo = Split-Path $PSScriptRoot -Parent
$EnvironmentInfo = python (Join-Path $PSScriptRoot 'ue_environment.py') check --json | ConvertFrom-Json
if (-not $EnvironmentInfo.editor_ready) { throw 'Installed UE editor unavailable' }
$Editor = $EnvironmentInfo.installed_editor
$Project = Join-Path $Repo 'ue_gas.uproject'
$Case = if ($Defeat) { "defeat-$Players" } else { "victory-$Players" }
$TaskFolder = $EvidenceFolder
if ($TaskFolder -notmatch '^[A-Za-z0-9_-]+$') { throw 'EvidenceFolder must be a single folder name' }
if ($ReturnBeacon) { $Case = "return-beacon-$Case" } elseif ($MerchantSeal) { $Case = "merchant-seal-$Case" } elseif ($UtilityEquipment) { $Case = "utility-equipment-$Case" } elseif ($Equipment) { $Case = "equipment-$Case" } elseif ($Events) { $Case = "events-$Case" } elseif ($Guardian) { $Case = "guardian-$Case" } elseif ($Assassin) { $Case = "assassin-$Case" } elseif ($Siege) { $Case = "siege-$Case" } elseif ($Reinforcement) { $Case = "reinforcement-$Case" } elseif ($TeamShop) { $Case = "team-shop-$Case" }
$OutputDir = Join-Path $Repo "Saved/$TaskFolder/network-$Case"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
# 清除本用例的旧日志，避免新进程启动前误读上次 Pass。
foreach ($OldName in @('server.log','client-1.log','client-2.log','report.json')) {
    $OldPath = Join-Path $OutputDir $OldName
    if (Test-Path -LiteralPath $OldPath) { Remove-Item -LiteralPath $OldPath }
}
$Processes = @()
$Logs = @()
try {
    $ServerLog = Join-Path $OutputDir 'server.log'
    $Flags = @('-unattended', '-nop4', '-nosplash', '-NullRHI', '-NoSound', '-DefenseSmoke', "-DefenseSmokePlayers=$Players")
    if ($Defeat) { $Flags += '-DefenseSmokeDefeat' }
    if ($Choices) { $Flags += @('-DefenseChoiceSmoke','-DefenseSeed=831907') }
    if ($TeamShop) { $Flags += @('-DefenseTeamShopSmoke','-DefenseSeed=831907') }
    if ($Reinforcement) { $Flags += @('-DefenseReinforcementSmoke','-DefenseSeed=831907') }
    if ($Siege) { $Flags += @('-DefenseSiegeSmoke','-DefenseSeed=831907') }
    if ($Assassin) { $Flags += @('-DefenseAssassinSmoke','-DefenseSeed=831907') }
    if ($Guardian) { $Flags += @('-DefenseGuardianSmoke','-DefenseSeed=831907') }
    if ($Equipment) { $Flags += @('-DefenseEquipmentSmoke','-DefenseSeed=831907') }
    if ($MerchantSeal) { $Flags += @('-DefenseMerchantSealSmoke','-DefenseSeed=831907') }
    if ($ReturnBeacon) { $Flags += @('-DefenseReturnBeaconSmoke','-DefenseSeed=831907') }
    if ($UtilityEquipment) { $Flags += @('-DefenseUtilityEquipmentSmoke','-DefenseSeed=831907') }
    if ($Events) { $Flags += @('-DefenseEventSmoke','-DefenseSeed=831907') }
    $Arguments = @(('"' + $Project + '"'), '/Game/Combat/Demo/Defense/L_Defense', '-server', "-port=$Port", '-ModelContextProtocolPort=8070', ('-abslog="' + $ServerLog + '"')) + $Flags
    $Processes += Start-Process -FilePath $Editor -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    $Logs += $ServerLog
    $Start = Get-Date
    $Ready = $false
    while (((Get-Date) - $Start).TotalSeconds -lt 45) {
        if ((Test-Path $ServerLog) -and (Select-String -LiteralPath $ServerLog -Pattern 'listening on port' -Quiet)) { $Ready = $true; break }
        if ($Processes[0].HasExited) { throw 'Server exited before listen' }
        Start-Sleep -Milliseconds 300
    }
    if (-not $Ready) { throw 'Server listen timeout' }
    for ($Index = 1; $Index -le $Players; $Index++) {
        $ClientLog = Join-Path $OutputDir "client-$Index.log"
        $Arguments = @(('"' + $Project + '"'), "127.0.0.1:$Port", '-game', "-DefenseSmokeSeat=$Index", "-ModelContextProtocolPort=$([int](8070+$Index))", '-windowed', '-ResX=960', '-ResY=540', ('-abslog="' + $ClientLog + '"')) + $Flags
        $Processes += Start-Process -FilePath $Editor -ArgumentList $Arguments -WindowStyle Hidden -PassThru
        $Logs += $ClientLog
    }
    $Passed = $false
    while (((Get-Date) - $Start).TotalSeconds -lt $TimeoutSeconds) {
        $Complete = 0
        foreach ($Log in $Logs) {
            if (-not (Test-Path $Log)) { continue }
            if (Select-String -LiteralPath $Log -Pattern 'DefenseSmoke Result=Fail|Fatal error:|Assertion failed:|DefenseConfigurationRejected|DefenseCoreSpawnFailed|DefenseInitializationFailed' -Quiet) { throw "Smoke failed: $Log" }
            if (Select-String -LiteralPath $Log -Pattern 'DefenseSmoke Result=Pass' -Quiet) { $Complete++ }
        }
        if ($Complete -eq $Logs.Count) { $Passed = $true; break }
        Start-Sleep -Milliseconds 500
    }
    if (-not $Passed) { throw "Defense smoke timeout: $OutputDir" }
    $Results = @($Logs | ForEach-Object { (Select-String -LiteralPath $_ -Pattern 'DefenseSmoke Result=Pass').Line })
    @{ passed=$true; players=$Players; defeat=[bool]$Defeat; logs=$Logs; results=$Results } | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'report.json')
    $Results | Write-Output
} finally {
    # 只关闭本次启动的具体进程，不触碰用户编辑器。
    foreach ($Process in $Processes) {
        if (-not $Process.HasExited) { Stop-Process -Id $Process.Id -ErrorAction SilentlyContinue }
    }
}

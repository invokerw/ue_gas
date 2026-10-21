[CmdletBinding()]
param([string]$ArchiveRoot,[switch]$SkipBuild,[string]$PythonExe='python')
# 构建来自配置中的源码引擎；Cook使用事件资产已验证的同版本安装版Editor。
$ErrorActionPreference='Stop'
$targetRepo=Split-Path $PSScriptRoot -Parent
if(!$ArchiveRoot){$ArchiveRoot=Join-Path $targetRepo 'Saved/DEF-013/targets-04/Archive'}
$targetArchive=[IO.Path]::GetFullPath($ArchiveRoot)
$targetEvidence=Split-Path $targetArchive -Parent
New-Item -ItemType Directory -Force -Path $targetEvidence | Out-Null
$targetEnvLines=& $PythonExe (Join-Path $PSScriptRoot 'ue_environment.py') --require all --json
if($LASTEXITCODE -ne 0){throw 'Both installed and source UE environments must be ready'}
$targetEnv=($targetEnvLines -join [Environment]::NewLine)|ConvertFrom-Json
$targetProject=Join-Path $targetRepo 'ue_gas.uproject'
if(!$SkipBuild){
 foreach($targetName in @('ue_gasServer','ue_gasClient')){
  & $targetEnv.build_script $targetName Win64 Development $targetProject -WaitMutex -NoHotReloadFromIDE *> (Join-Path $targetEvidence "$targetName-build.log")
  if($LASTEXITCODE -ne 0){throw "Target build failed: $targetName"}
 }
}
# 缺少正式Target或receipt时不能用Game/Editor补位。
foreach($targetName in @('ue_gasServer','ue_gasClient')){
 $targetBinary=Join-Path $targetRepo "Binaries/Win64/$targetName.exe"
 $targetReceipt=Join-Path $targetRepo "Binaries/Win64/$targetName.target"
 if(!(Test-Path -LiteralPath $targetBinary) -or !(Test-Path -LiteralPath $targetReceipt)){throw "Missing built Target: $targetName"}
 $targetMetadata=Get-Content -LiteralPath $targetReceipt -Raw|ConvertFrom-Json
 $targetExpected=if($targetName -eq 'ue_gasServer'){'Server'}else{'Client'}
 if($targetMetadata.TargetType -ne $targetExpected){throw "Wrong Target receipt: $targetName"}
}
$targetInstalledEngine=Split-Path (Split-Path (Split-Path $targetEnv.installed_editor -Parent) -Parent) -Parent
$targetUat=Join-Path $targetInstalledEngine 'Build/BatchFiles/RunUAT.bat'
$targetArgs=@('BuildCookRun',"-project=$targetProject",'-noP4','-server','-client','-platform=Win64','-serverplatform=Win64','-serverconfig=Development','-clientconfig=Development','-skipbuild','-cook','-stage','-pak','-archive',"-archivedirectory=$targetArchive",'-map=/Game/Combat/Demo/Defense/L_Defense','-utf8output','-unattended','-nocompileeditor','-skipbuildeditor','-AdditionalCookerOptions=-ModelContextProtocolPort=8110')
& $targetUat @targetArgs *> (Join-Path $targetEvidence 'package-targets.log')
if($LASTEXITCODE -ne 0){throw "Server/Client Cook/Stage failed; see $targetEvidence/package-targets.log"}
@{source_editor=$targetEnv.source_editor;installed_editor=$targetEnv.installed_editor;archive=$targetArchive;server_receipt='ue_gasServer.target';client_receipt='ue_gasClient.target'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $targetEvidence 'package-environment.json') -Encoding utf8
Write-Output "Dedicated Server and Client staged: $targetArchive"

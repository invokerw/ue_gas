#!/usr/bin/env python3
"""仅从匹配平台的 Cook/Stage 归档运行真实 Server/Client，阻止 raw Content 启动与伪通过。"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid

class SmokeFailure(RuntimeError):
    """表示归档不完整、进程失效或网络证据不符合本次验收。"""

ERRORS=re.compile(r'Assertion failed:|Fatal error:|Critical error:|DefenseSmoke Result=Fail|Failed to load premade asset registry|DefenseConfigurationRejected|DefenseCoreSpawnFailed|DefenseInitializationFailed|EconomyPlayerInitializationFailed|DefenseTeamItemsConfigurationFailed|Shop category invalid',re.I)
ROLES=('server','client-1','client-2')


def staged_executable(archive:Path,role:str)->Path:
    """只接受正确平台归档中的独立 Target，缺少平台容器时禁止启动。"""
    if role not in ('server','client'):raise SmokeFailure('Unknown target role: '+role)
    platform='WindowsServer' if role=='server' else 'WindowsClient'
    target='ue_gasServer' if role=='server' else 'ue_gasClient'
    content=archive/platform/'ue_gas'
    exe=content/'Binaries'/'Win64'/f'{target}.exe'
    paks=content/'Content'/'Paks'
    required=[exe,*[paks/name for name in [f'ue_gas-{platform}.pak',f'ue_gas-{platform}.utoc',f'ue_gas-{platform}.ucas','global.utoc','global.ucas']]]
    missing=[str(p) for p in required if not p.is_file() or p.stat().st_size==0]
    if missing:raise SmokeFailure('Missing staged target/content; do not launch raw Binaries: '+', '.join(missing))
    return exe


def require_live_process(process:subprocess.Popen,role:str)->None:
    """进程早退立即失败，避免把Crash误当监听或业务超时。"""
    code=process.poll()
    if code is not None:raise SmokeFailure(f'{role} exited before completion with code {code}')


def read_report(output:Path)->dict|None:
    """验证三端本轮日志，任何Crash优先于Pass；NetMode、座位、schema及RunId必须匹配。"""
    rows={}
    for role in ROLES:
        path=output/f'{role}.log'
        if not path.exists():continue
        text=path.read_text(encoding='utf-8',errors='replace')
        error=ERRORS.search(text)
        if error:raise SmokeFailure(f'{role}: {error.group(0)}; see {path}')
        matches=re.findall(r'DefenseSmoke Result=Pass[^\r\n]*',text)
        if not matches:continue
        if len(matches)!=1:raise SmokeFailure(f'{role} emitted multiple completion receipts')
        row=matches[0]; fields=dict(re.findall(r'(\w+)=([^\s]+)',row))
        if fields.get('Schema')!='4' or fields.get('NetMode')!=('1' if role=='server' else '3'):raise SmokeFailure(f'{role} invalid schema/NetMode: {row}')
        run=fields.get('Run','')
        if not re.fullmatch('[0-9A-Fa-f]{32}',run) or int(run,16)==0:raise SmokeFailure(f'{role} missing valid run identity')
        if not fields.get('StateMask','').isdigit():raise SmokeFailure(f'{role} invalid event transition mask')
        mask=int(fields['StateMask'])
        if mask&54!=54:raise SmokeFailure(f'{role} missed event transitions')
        if role=='server':
            if fields.get('Players')!='2' or fields.get('Gold')!='1940' or fields.get('OwnerCheck')!='ForeignRejected':raise SmokeFailure('Server reward/owner evidence invalid: '+row)
        elif fields.get('Seat')!=role[-1] or fields.get('ClientAuthority')!='0' or fields.get('Receipt')!='1' or fields.get('Step')!='4':raise SmokeFailure(f'{role} invalid client receipt: {row}')
        rows[role]={'log':str(path),'result':row,'run_id':run.upper()}
    if len(rows)!=3:return None
    runs={row['run_id'] for row in rows.values()}
    if len(runs)!=1:raise SmokeFailure('Server and clients completed different runs')
    return {'passed':True,'run_id':runs.pop(),'roles':rows}


def hidden_flags()->int:
    """Windows 子进程不打开额外控制台窗口。"""
    return subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0


def file_digest(path:Path)->str:
    """摘要指认本次实际运行的 exe，避免只报Target名字却运行旧文件。"""
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def run_smoke(archive:Path,output:Path,port:int,timeout:float)->dict:
    """监听就绪才连入两个真实客户端；目录必须全新，任何端早退或超时都失败并清理本轮进程。"""
    server=staged_executable(archive,'server');client=staged_executable(archive,'client')
    if output.exists():raise SmokeFailure('Output directory must be new; stale logs are forbidden: '+str(output))
    output.mkdir(parents=True)
    processes={};started=time.monotonic();success=False
    common=['-unattended','-NoSplash','-NullRHI','-NoSound','-DefenseSmoke','-DefenseEventSmoke','-DefenseSmokePlayers=2','-DefenseSeed=831907']
    def launch(role,exe,args):
        command=[str(exe),*args,*common,f'-AbsLog={output/str(role+".log")}']
        processes[role]=subprocess.Popen(command,cwd=exe.parent,creationflags=hidden_flags(),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        launch('server',server,['/Game/Combat/Demo/Defense/L_Defense',f'-port={port}'])
        while time.monotonic()-started<min(timeout,45):
            require_live_process(processes['server'],'server');read_report(output)
            log=output/'server.log'
            if log.exists() and f'listening on port {port}' in log.read_text(encoding='utf-8',errors='replace'):break
            time.sleep(.25)
        else:raise SmokeFailure('Dedicated server listen timeout')
        for seat in (1,2):launch(f'client-{seat}',client,[f'127.0.0.1:{port}',f'-DefenseSmokeSeat={seat}'])
        (output/'processes.json').write_text(json.dumps({role:p.pid for role,p in processes.items()},indent=2),encoding='utf-8')
        while time.monotonic()-started<timeout:
            for role,p in processes.items():require_live_process(p,role)
            report=read_report(output)
            if report:
                report.update({'archive':str(archive),'server_exe':str(server),'client_exe':str(client),'server_sha256':file_digest(server),'client_sha256':file_digest(client),'duration_seconds':round(time.monotonic()-started,2)})
                (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');success=True;return report
            time.sleep(.5)
        raise SmokeFailure('Dedicated event smoke timed out')
    except Exception as exc:
        (output/'failure.json').write_text(json.dumps({'passed':False,'error':str(exc)},ensure_ascii=False,indent=2),encoding='utf-8');raise
    finally:
        for p in processes.values():
            if p.poll() is None:p.terminate()
        for p in processes.values():
            try:p.wait(timeout=10)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=10)
        (output/'cleanup.json').write_text(json.dumps({'all_exited':all(p.poll() is not None for p in processes.values()),'passed':success}),encoding='utf-8')


def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    repo=Path(__file__).resolve().parents[1]
    parser.add_argument('--archive',type=Path,default=repo/'Saved/DEF-013/targets-04/Archive')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--port',type=int,default=7905)
    parser.add_argument('--timeout',type=float,default=240)
    parser.add_argument('--package',action='store_true',help='先运行平台正确的Build/Cook/Stage/Archive')
    parser.add_argument('--skip-build',action='store_true',help='打包时复用现有源码Server/Client构建；仍Cook/Stage')
    args=parser.parse_args()
    if not 1024<=args.port<=65535 or args.timeout<=0:parser.error('Invalid port/timeout')
    archive=args.archive.resolve();output=args.output.resolve() if args.output else repo/'Saved/DEF-013/targets-04'/f'smoke-{time.strftime("%Y%m%d-%H%M%S")}-{uuid.uuid4().hex[:6]}'
    try:
        if args.package:
            shell=shutil.which('pwsh') or shutil.which('powershell')
            if shell is None:raise SmokeFailure('PowerShell is required to package Windows Targets')
            command=[shell,'-NoProfile','-NonInteractive','-File',str(repo/'Tools/package_defense_targets.ps1'),'-ArchiveRoot',str(archive)]
            if args.skip_build:command.append('-SkipBuild')
            subprocess.run(command,check=True,creationflags=hidden_flags())
        report=run_smoke(archive,output,args.port,args.timeout)
        print(json.dumps(report,ensure_ascii=False,indent=2));return 0
    except (SmokeFailure,OSError,subprocess.CalledProcessError) as exc:
        print('Defense Target validation failed: '+str(exc),file=sys.stderr);return 1

if __name__=='__main__':sys.exit(main())

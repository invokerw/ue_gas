"""独立 Target 启动器的失败准入与真实三端回执测试。"""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from run_defense_targets import staged_executable, read_report, require_live_process, run_smoke, SmokeFailure

RUN='0123456789ABCDEF0123456789ABCDEF'

def success(role):
    mode=1 if role=='server' else 3
    detail='Players=2 StateMask=54 Gold=1940 OwnerCheck=ForeignRejected' if role=='server' else f'StateMask=63 Receipt=1 Step=4 Seat={role[-1]} ClientAuthority=0'
    return f'DefenseSmoke Result=Pass NetMode={mode} EventNetwork Schema=4 {detail} Run={RUN}\n'

class DefenseTargetToolTests(unittest.TestCase):
    def stage(self,root,role):
        platform='WindowsServer' if role=='server' else 'WindowsClient'
        target='ue_gasServer' if role=='server' else 'ue_gasClient'
        content=root/platform/'ue_gas'
        exe=content/'Binaries'/'Win64'/f'{target}.exe';exe.parent.mkdir(parents=True,exist_ok=True);exe.write_bytes(b'EXE')
        paks=content/'Content'/'Paks';paks.mkdir(parents=True,exist_ok=True)
        for name in [f'ue_gas-{platform}.pak',f'ue_gas-{platform}.utoc',f'ue_gas-{platform}.ucas','global.utoc','global.ucas']:(paks/name).write_bytes(b'data')
        return exe
    def test_raw_binaries_are_not_staged(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);p=root/'Binaries'/'Win64'/'ue_gasServer.exe';p.parent.mkdir(parents=True);p.write_bytes(b'EXE')
            with self.assertRaises(SmokeFailure):staged_executable(root,'server')
    def test_platform_content_required_even_with_executable(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);exe=self.stage(root,'server')
            self.assertEqual(staged_executable(root,'server'),exe)
            (root/'WindowsServer'/'ue_gas'/'Content'/'Paks'/'ue_gas-WindowsServer.utoc').unlink()
            with self.assertRaises(SmokeFailure):staged_executable(root,'server')
    def test_server_content_cannot_be_used_for_client(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.stage(root,'server')
            with self.assertRaises(SmokeFailure):staged_executable(root,'client')
    def test_raw_pass_without_correct_netmode_or_run_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for role in ['server','client-1','client-2']:(root/f'{role}.log').write_text(success(role),encoding='utf-8')
            report=read_report(root)
            self.assertTrue(report['passed']);self.assertEqual(report['run_id'],RUN)
            (root/'server.log').write_text(success('server').replace('NetMode=1','NetMode=0'),encoding='utf-8')
            with self.assertRaises(SmokeFailure):read_report(root)
    def test_mixed_runs_and_client_authority_leaks_fail(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for role in ['server','client-1','client-2']:(root/f'{role}.log').write_text(success(role),encoding='utf-8')
            (root/'client-1.log').write_text(success('client-1').replace(RUN,'F'*32),encoding='utf-8')
            with self.assertRaises(SmokeFailure):read_report(root)
            (root/'client-1.log').write_text(success('client-1').replace('ClientAuthority=0','ClientAuthority=1'),encoding='utf-8')
            with self.assertRaises(SmokeFailure):read_report(root)
    def test_crash_cannot_be_masked_by_later_or_old_pass(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for role in ['server','client-1','client-2']:(root/f'{role}.log').write_text(success(role),encoding='utf-8')
            (root/'server.log').write_text(success('server')+'Assertion failed: ReaderPos + Num <= ReaderSize\n',encoding='utf-8')
            with self.assertRaises(SmokeFailure):read_report(root)
    def test_existing_output_rejects_stale_pass_before_process_launch(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.stage(root,'server');self.stage(root,'client')
            output=root/'old-run';output.mkdir()
            for role in ['server','client-1','client-2']:(output/f'{role}.log').write_text(success(role),encoding='utf-8')
            with self.assertRaisesRegex(SmokeFailure,'must be new'):run_smoke(root,output,7905,5)
    def test_crash_in_partial_run_fails_without_waiting_for_other_clients(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'server.log').write_text('Failed to load premade asset registry. LoadResult == 1.',encoding='utf-8')
            with self.assertRaises(SmokeFailure):read_report(root)
    def test_missing_receipts_remain_pending(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertIsNone(read_report(Path(d)))
    def test_early_exit_fails_instead_of_waiting_for_timeout(self):
        class Exited:
            def poll(self):return 3
        with self.assertRaises(SmokeFailure):require_live_process(Exited(),'server')

if __name__=='__main__':unittest.main()

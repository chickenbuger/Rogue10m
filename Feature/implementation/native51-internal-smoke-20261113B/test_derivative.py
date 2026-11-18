"""Synthetic data only; no engine, build, game, metadata QA or GUI calls."""
import contextlib
from datetime import datetime, timezone
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import internal_smoke as m


class DerivativeTests(unittest.TestCase):
    def setUp(self):
        self.c=m.helpers(); scratch=m.HERE/'scratch';scratch.mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=scratch,prefix='derivative-synthetic-');self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name)

    def pin(self,name,obj):
        p=self.base/name;p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes(obj if isinstance(obj,bytes) else json.dumps(obj).encode())
        return self.c.file_pin(p)

    def test_actual_cook_fix_helper_is_byte_identical(self):
        self.assertEqual(self.c.file_pin(m.HERE/'strict_primitives.py')['sha256'],'D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF')

    def test_default_unsealed_has_no_executable_arguments(self):
        old=sys.argv;sys.argv=['internal_smoke.py','--seal',str(m.HERE/'smoke.UNSEALED.json'),'--seal-sha256',self.c.file_pin(m.HERE/'smoke.UNSEALED.json')['sha256'],'--plan']
        out=io.StringIO()
        try:
            with contextlib.redirect_stdout(out):code=m.main()
        finally:sys.argv=old
        result=json.loads(out.getvalue())
        self.assertEqual(code,2);self.assertFalse(result['executed']);self.assertNotIn('bcr_args',result)
        self.assertEqual(result['acceptance_flags']['UI7'],'UNVERIFIED');self.assertFalse(result['release_eligible'])

    def test_old_native43_seal_rejected(self):
        p=self.pin('old.json',{'schema':'R10-internal-smoke-seal-v1','status':'sealed'})
        with self.assertRaisesRegex(ValueError,'UNSEALED'):m.validate(self.c,p,{},'owner')

    def test_stamp_never_upgrades_ui_or_release(self):
        r=m.stamp({'release_eligible':True,'acceptance_flags':{'UI7':'PASSED'}})
        self.assertFalse(r['release_eligible']);self.assertEqual(r['acceptance_flags'],m.FLAGS)
        self.assertNotIn('npc_placement',r['known_limitations'])

    def freeze(self):
        state={'status':'OWNER_RECORDED_FINAL','map_extent_m':[1800,1800],'npc_placement':'SYNTHETIC placement snapshot','default_map':'SYNTHETIC retained menu'}
        s={'project':str(self.base/'Game.uproject'),'active_destination':'/Game/Map','input_manifest':{'sha256':'A'*64},'map_manifest':{'sha256':'B'*64},'content_state':state}
        dll={'sha256':'C'*64}
        f={'schema':'R10-native51-content-freeze-v1','status':'frozen_for_internal_smoke','project':s['project'],'active_destination':s['active_destination'],
           'input_manifest_sha256':'A'*64,'map_manifest_sha256':'B'*64,'editor_dll_sha256':'C'*64,'content_state':state,
           'recorded_utc':datetime.now(timezone.utc).isoformat(),'recorder':'SYNTHETIC TEST ONLY','evidence':[self.pin('fake-layout.txt',b'SYNTHETIC content data')]}
        s['content_freeze']=self.pin('freeze.json',f)
        return s,dll,f

    def test_content_freeze_binds_input_map_dll_and_state(self):
        s,dll,f=self.freeze();m.content_freeze(self.c,s,dll)
        for key in ('input_manifest_sha256','map_manifest_sha256','editor_dll_sha256'):
            changed={**f,key:'0'*64};s['content_freeze']=self.pin('different.json',changed)
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'different inputs'):m.content_freeze(self.c,s,dll)

    def test_in_progress_content_is_never_final(self):
        s,dll,f=self.freeze();s['content_state']={**s['content_state'],'status':'UNSEALED_PENDING_FINAL_CONTENT'}
        f['content_state']=s['content_state'];s['content_freeze']=self.pin('pending.json',f)
        with self.assertRaisesRegex(ValueError,'still UNSEALED'):m.content_freeze(self.c,s,dll)

    def test_freeze_raw_evidence_tamper_blocks(self):
        s,dll,f=self.freeze();Path(f['evidence'][0]['path']).write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'SHA mismatch'):m.content_freeze(self.c,s,dll)

    def test_cook_windows_foo_and_rebuild_are_exact(self):
        s={'root':str(self.base/'Project'),'project':str(self.base/'Project/Game.uproject'),'engine':str(self.base/'Engine'),
           'output_root':str(self.base),'outputs':{k:str(self.base/k) for k in self.c.OUTPUTS},'run_tag':'Synthetic51','runtime':{},'tool_pins':[],
           'cook_maps':['/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'],'active_destination':'/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A',
           'prior_game_products':[],'content_state':{'status':'SYNTHETIC'},'content_freeze':{},'preserved_artifacts':[]}
        p=m.plan(self.c,s,'A'*64,'B'*64,{'tests':51,'executed_here':False})
        self.assertEqual(p['schema'],'R10-native51-smoke-plan-v1')
        self.assertIn('-CookOutputDir='+str(self.base/'cook/Windows'),p['bcr_args'])
        ubt=next(x for x in p['bcr_args'] if x.startswith('-UbtArgs='));self.assertEqual(ubt.count('-Rebuild'),1)
        cook=next(x for x in p['bcr_args'] if x.startswith('-AdditionalCookerOptions='));self.assertIn('-FOOBASEDIR="'+str(self.base/'cook_user')+'"',cook)
        self.assertNotIn('-clean',p['bcr_args']);self.assertFalse(p['release_eligible']);self.assertEqual(p['native51']['tests'],51)

    def test_archive_and_staged_exe_chain(self):
        s={'outputs':{'archive':str(self.base/'archive'),'stage':str(self.base/'stage')}}
        data=b'SYNTHETIC EXE NOT EXECUTABLE';sha=self.c.digest(data)
        stage=self.base/'stage/Windows/Rogue10m/Binaries/Win64/Rogue10m.exe';stage.parent.mkdir(parents=True);stage.write_bytes(data)
        built={'game':{'sha256':sha},'archive_game':{'sha256':sha}}
        log='Parsing command line: BuildCookRun -archivedirectory='+s['outputs']['archive']+'\n'
        m.stage_archive_chain(self.c,s,built,log)
        with self.assertRaisesRegex(ValueError,'archive path'):m.stage_archive_chain(self.c,s,built,log.replace('archive','wrong'))
        with self.assertRaisesRegex(ValueError,'archive path'):m.stage_archive_chain(self.c,s,built,log.rstrip()+' -archivedirectory='+s['outputs']['archive']+'\n')
        stage.write_bytes(b'OTHER EXE')
        with self.assertRaisesRegex(ValueError,'identity differs'):m.stage_archive_chain(self.c,s,built,log)

    def test_validators_do_not_launch_processes_or_gui(self):
        import ast
        for name in ('internal_smoke.py','native51_validation.py'):
            tree=ast.parse((m.HERE/name).read_text(encoding='utf-8-sig'))
            for n in ast.walk(tree):
                if isinstance(n,ast.Import):self.assertFalse({i.name for i in n.names}&{'subprocess','pyautogui','pywinauto'})
                if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute):self.assertNotIn(n.func.attr,{'Popen','system','startfile','kill','terminate'})


if __name__=='__main__':unittest.main(verbosity=2)

"""No editor/network use: test that the native MCP orchestration refuses unsafe writes."""
from pathlib import Path
import copy
import importlib.util
import json
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Scripts'))
sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import BindMagicSkillVfxMCP20261111 as binding


class FakeNative:
    def __init__(self,plan):
        self.calls=[];self.dirty=set();self.collateral=False;self.setter_ok=True;self.save_ok=True
        self.plan=plan;self.props={};self.compiling=False;self.pie=False
        for row in plan['targets']:
            self.props[row['asset']]={
                'bEnableAttackEffects':True,'castEffect':{'refPath':'/Game/OldCast.OldCast'},
                'impactEffect':{'refPath':'/Game/OldImpact.OldImpact'},'chargeEffect':'None',
                'castEffectScale':.17,'impactEffectScale':.23,'attackEffectTimeDilation':3.,
                'castEffectEmissionDuration':.08,'impactEffectEmissionDuration':.09,
                'firstPersonEmissionDurationMultiplier':.7,'damage':13.6,
                'attackMontage':{'refPath':'/Game/OriginalMontage.OriginalMontage'},
                'hitStartDelaySeconds':.21,'resourceCosts':[{'type':'Mana','amount':2.}],
                'effectAttachSocket':'hand_r','firstPersonCastScaleMultiplier':.42,
                'animationPlayRate':1.08,'chargeEffectScale':.24,
                'firstPersonImpactScaleMultiplier':.72,'firstPersonChargeScaleMultiplier':.34,
                'firstPersonEffectOffset':{'x':8.,'y':20.,'z':-18.}}
        self.props[plan['profile_asset']]={'weaponType':'Staff','skillTreeSkills':[{'refPath':binding.object_path(row['asset'])} for row in plan['targets']],
            'defaultSkillBindings':{'primary':{'refPath':binding.object_path(plan['pilot_asset'])}}}
        self.props[plan['character_asset']]={'weaponSkillProfiles':[{'refPath':binding.object_path(plan['profile_asset'])}]}

    def call(self,ts,name,args):
        self.calls.append((ts,name,copy.deepcopy(args)))
        if name=='IsPIERunning':return self.pie
        if name=='ListSkills':return {'skill://project/testing':{'description':'Review test contract'}}
        if name=='GetSkills':return {'skill://project/testing':'Preserve damage and timing'}
        if name=='is_dirty':return args['asset_path'] in self.dirty
        if name=='can_edit_asset':return True
        if name=='get_asset_class':
            path=args['asset_path']
            if path in self.plan['candidate_assets']:return 'NiagaraSystem'
            if path==self.plan['profile_asset']:return 'Rogue10mWeaponSkillProfileDataAsset'
            if path==self.plan['character_asset']:return 'Rogue10mCharacterDataAsset'
            return 'Rogue10mAttackSkillData'
        if name=='GetSystemCompileState':return {'bIsCompiling':self.compiling,'bIsStale':False,'bHasErrors':False,'aggregateStatus':'UpToDate'}
        if name=='GetStackIssues':return {'numErrors':0,'numWarnings':0}
        if name in ('list_properties','get_properties','set_properties'):
            path=args['instance']['refPath'].split('.',1)[0]
            values=self.props[path]
            if name=='list_properties':return {key:{'type':'test'} for key in values}
            if name=='get_properties':return copy.deepcopy({key:values[key] for key in args['properties']})
            values.update(json.loads(args['values']));self.dirty.add(path)
            if self.collateral:values['damage']=999.
            return self.setter_ok
        if name=='save_assets':
            if self.save_ok:self.dirty.difference_update(args['asset_paths'])
            return self.save_ok
        raise AssertionError('Unexpected tool '+name)


class NativeBindingTests(unittest.TestCase):
    def setUp(self):
        self.plan=binding.build_binding_plan(ROOT);self.client=FakeNative(self.plan)
        self.snapshot=binding.inspect_state(self.client,self.plan)
        self.client.calls.clear();self.report={}
    def mutations(self):return [c for c in self.client.calls if c[1] in ('set_properties','save_assets')]
    def apply(self):return binding.apply_inspected_state(self.client,self.plan,self.snapshot,self.report)
    def test_success_saves_exactly_one_asset_and_preserves_every_other_field(self):
        report=self.apply();writes=self.mutations()
        self.assertEqual([w[1] for w in writes],['set_properties','save_assets'])
        self.assertEqual(writes[-1][2],{'asset_paths':[self.plan['pilot_asset']]})
        self.assertTrue(report['nonvisual_fields_preserved']);self.assertTrue(report['asset_saved'])
        self.assertFalse(report['runtime_gameplay_validated'])
    def test_live_half_first_person_duration_is_preserved_and_never_dispatched(self):
        pilot=self.client.props[self.plan['pilot_asset']]
        pilot['firstPersonEmissionDurationMultiplier']=.5
        self.snapshot=binding.inspect_state(self.client,self.plan);self.client.calls.clear()
        report=self.apply()
        self.assertEqual(pilot['firstPersonEmissionDurationMultiplier'],.5)
        self.assertTrue(report['nonvisual_fields_preserved'])
        setter=next(c for c in self.mutations() if c[1]=='set_properties')
        self.assertNotIn('firstPersonEmissionDurationMultiplier',json.loads(setter[2]['values']))
        self.assertEqual(len(self.plan['targets'][0]['updates']),8)
    def test_collateral_first_person_multiplier_drift_stops_before_save(self):
        original_call=self.client.call
        def drift(ts,name,args):
            result=original_call(ts,name,args)
            if name=='set_properties':
                self.client.props[self.plan['pilot_asset']]['firstPersonEmissionDurationMultiplier']=1.
            return result
        self.client.call=drift
        with self.assertRaisesRegex(ValueError,'Unplanned property changed'):self.apply()
        self.assertEqual([w[1] for w in self.mutations()],['set_properties'])
    def test_stale_damage_or_montage_snapshot_cannot_write(self):
        self.client.props[self.plan['pilot_asset']]['damage']+=1
        with self.assertRaisesRegex(RuntimeError,'Stale'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_dirty_nonpilot_cannot_write(self):
        self.client.dirty.add(next(r['asset'] for r in self.plan['targets'] if r['role']=='protected'))
        with self.assertRaisesRegex(RuntimeError,'Unsaved'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_pie_cannot_write(self):
        self.client.pie=True
        with self.assertRaisesRegex(RuntimeError,'PIE'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_compiling_candidate_cannot_write(self):
        self.client.compiling=True
        with self.assertRaisesRegex(RuntimeError,'compile'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_nonnull_charge_is_rejected_before_setter(self):
        self.client.props[self.plan['pilot_asset']]['chargeEffect']={'refPath':'/Game/OldCharge.OldCharge'}
        self.snapshot=binding.inspect_state(self.client,self.plan);self.client.calls.clear()
        with self.assertRaisesRegex(ValueError,'charge visual'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_setter_false_is_never_retried_or_saved(self):
        self.client.setter_ok=False
        with self.assertRaisesRegex(RuntimeError,'Setter'):self.apply()
        self.assertEqual([w[1] for w in self.mutations()],['set_properties'])
        self.assertTrue(self.report['mutation_attempted']);self.assertIsNone(self.report['data_asset_connected'])
    def test_collateral_damage_change_stops_before_save(self):
        self.client.collateral=True
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual([w[1] for w in self.mutations()],['set_properties'])
    def test_save_failure_is_not_retried_or_reported_saved(self):
        self.client.save_ok=False
        with self.assertRaisesRegex(RuntimeError,'save'):self.apply()
        self.assertEqual([w[1] for w in self.mutations()],['set_properties','save_assets'])
        self.assertFalse(self.report['asset_saved']);self.assertTrue(self.report['data_asset_connected_in_memory'])
    def test_already_matching_state_has_no_set_or_save_dispatch(self):
        updates=next(r['updates'] for r in self.plan['targets'] if r['role']=='pilot')
        self.client.props[self.plan['pilot_asset']].update(binding.native_updates(updates))
        self.snapshot=binding.inspect_state(self.client,self.plan);self.client.calls.clear()
        result=self.apply()
        self.assertTrue(result['already_matches']);self.assertEqual(self.mutations(),[])
    def test_profile_primary_must_resolve_pilot(self):
        self.client.props[self.plan['profile_asset']]['defaultSkillBindings']['primary']={'refPath':'/Game/Different.Different'}
        with self.assertRaisesRegex(RuntimeError,'default Primary'):self.apply()
        self.assertEqual(self.mutations(),[])
    def test_actual_official_lowercamel_primary_serialization(self):
        # Exact struct from MagicStaffInspect1111A/mcp/0030-get_properties.json.
        actual=json.loads('{"chargedPrimary": {"refPath": "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Charged.DA_Attack_Staff_Charged"}, "chargedSpecial": {"refPath": "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Charged.DA_Attack_Staff_Charged"}, "primary": {"refPath": "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01.DA_Attack_Staff_Primary01"}, "special": {"refPath": "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Special.DA_Attack_Staff_Special"}}')
        self.client.props[self.plan['profile_asset']]['defaultSkillBindings']=actual
        live=binding.inspect_state(self.client,self.plan)
        self.assertEqual(live['properties'][self.plan['profile_asset']]['defaultSkillBindings'],actual)
        self.assertEqual(self.mutations(),[])
    def test_legacy_primary_alias_still_requires_exact_reference(self):
        bindings=self.client.props[self.plan['profile_asset']]['defaultSkillBindings']
        bindings['Primary']=bindings.pop('primary')
        binding.inspect_state(self.client,self.plan)
        self.assertEqual(self.mutations(),[])
    def test_primary_alias_collision_or_missing_key_cannot_write(self):
        original=copy.deepcopy(self.client.props[self.plan['profile_asset']]['defaultSkillBindings'])
        for invalid in ({}, {'Primary':original['primary'],'primary':original['primary']},
                        {'PRIMARY':original['primary']}, {'primary':original['primary'],'PRIMARY':original['primary']}):
            with self.subTest(bindings=invalid):
                self.client.props[self.plan['profile_asset']]['defaultSkillBindings']=invalid
                with self.assertRaisesRegex(RuntimeError,'default Primary'):self.apply()
                self.assertEqual(self.mutations(),[])
    def test_primary_nested_multiple_or_wrong_object_reference_cannot_write(self):
        correct={'refPath':binding.object_path(self.plan['pilot_asset'])}
        for invalid in ([correct], {'nested':correct}, {'refPath':correct['refPath'],'extra':correct},
                        {'refPath':self.plan['pilot_asset']+'.Different'}, None):
            with self.subTest(reference=invalid):
                self.client.props[self.plan['profile_asset']]['defaultSkillBindings']['primary']=invalid
                with self.assertRaisesRegex(RuntimeError,'default Primary'):self.apply()
                self.assertEqual(self.mutations(),[])
    def test_safe_package_and_identity_parsing(self):
        for value in ['/Game/../Other','/Engine/Shared','/Game/A.A','/Game/A;quit']:
            with self.assertRaises(ValueError):binding.package_file(value)
        self.assertEqual(binding.normalized_guid({'A':1,'B':2,'C':3,'D':-1}),'000000010000000200000003ffffffff')
        self.assertEqual(binding.normalized_guid('{0D4EBF5B-4242-817C-04F5-51B299D16975}'),'0d4ebf5b4242817c04f551b299d16975')
    def test_verify_pins_saved_artifact_and_inspection_lineage(self):
        applied={'phase':'apply','status':'pilot_visual_binding_saved_same_session_readback_pending_gameplay_qa',
                 'data_asset_connected':True,'plan':self.plan,'inspect_snapshot_sha256':'inspected-file-hash',
                 'fingerprints_after':{'pilot':'saved-B','protected':'unchanged'}}
        binding.validate_applied_artifact(self.plan,'inspected-file-hash',applied,{'pilot':'saved-B','protected':'unchanged'})
        with self.assertRaisesRegex(RuntimeError,'artifact changed'):
            binding.validate_applied_artifact(self.plan,'inspected-file-hash',applied,{'pilot':'external-C','protected':'unchanged'})
        with self.assertRaisesRegex(RuntimeError,'does not belong'):
            binding.validate_applied_artifact(self.plan,'different-inspection',applied,applied['fingerprints_after'])
        failed=copy.deepcopy(applied);failed['data_asset_connected']=False
        with self.assertRaisesRegex(RuntimeError,'Successful apply'):
            binding.validate_applied_artifact(self.plan,'inspected-file-hash',failed,applied['fingerprints_after'])
    def test_post_readback_file_change_and_save_hash_change_are_rejected(self):
        binding.validate_final_fingerprints({'pilot':'A','protected':'P'},{'pilot':'B','protected':'P'},'pilot','B')
        with self.assertRaisesRegex(RuntimeError,'file changed'):
            binding.validate_final_fingerprints({'pilot':'B','protected':'P'},{'pilot':'C','protected':'P'},'pilot')
        with self.assertRaisesRegex(RuntimeError,'file changed'):
            binding.validate_final_fingerprints({'pilot':'A','protected':'P'},{'pilot':'C','protected':'P'},'pilot','B')


if __name__=='__main__':unittest.main(verbosity=2)

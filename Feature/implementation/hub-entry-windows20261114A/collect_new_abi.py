"""Collect fresh native type/PE evidence if the rebuilt EXE/PDB differ from D."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
import sys

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/hub-entry-windows20261114A'
B = R / 'tmp/PackageAbiVerification20261113C'
H = B / 'verify_package_abi.py'
LLVM = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/llvm-pdbutil.exe')
RESUME = sys.argv[1:] == ['--resume-recorded-queries']
assert RESUME or not sys.argv[1:]

def pin(path):
    path=Path(path); before=path.stat(); raw=path.read_bytes(); after=path.stat()
    assert (before.st_ino,before.st_size,before.st_mtime_ns)==(after.st_ino,after.st_size,after.st_mtime_ns)
    return {'path':str(path),'sha256':hashlib.sha256(raw).hexdigest().upper(),'size':len(raw)}

def write(path,value):
    with path.open('x',encoding='utf-8') as f:json.dump(value,f,indent=2)

def invoke(name,argv,output):
    receipt = output/(name+'.process.json')
    if RESUME and receipt.exists():
        recorded=json.loads(receipt.read_text(encoding='utf-8-sig'))
        assert recorded['args']==argv and recorded['exit_code']==0
        for key in ('stdout','stderr'): assert pin(recorded[key]['path'])==recorded[key]
        assert not Path(recorded['stderr']['path']).read_bytes().strip()
        return Path(recorded['stdout']['path']).read_text(encoding='utf-8-sig')
    assert not receipt.exists()
    started=datetime.now(timezone.utc).isoformat()
    p=subprocess.run(argv,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW,timeout=60)
    (output/(name+'.txt')).write_bytes(p.stdout);(output/(name+'.stderr.txt')).write_bytes(p.stderr)
    write(output/(name+'.process.json'),{'args':argv,'exit_code':p.returncode,'started_utc':started,'ended_utc':datetime.now(timezone.utc).isoformat(),'stdout':pin(output/(name+'.txt')),'stderr':pin(output/(name+'.stderr.txt')),'Game_or_UE_executions':0,'process_attachment':False})
    assert p.returncode==0 and not p.stderr.strip(),name
    return p.stdout.decode('utf-8-sig')

owner=json.loads((E/'owner-result.json').read_text(encoding='utf-8-sig'))
assert owner['exit_code']==0 and owner['execute_requested']
seal=json.loads(Path(owner['seal']['path']).read_text(encoding='utf-8-sig'))
assert pin(owner['seal']['path'])['sha256']==owner['seal']['sha256']
archive=Path(seal['outputs']['archive'])/'Windows/Rogue10m/Binaries/Win64'
exe=pin(archive/'Rogue10m.exe');pdb=pin(archive/'Rogue10m.pdb')
definitions=pin(R/'Intermediate/Build/Win64/x64/Rogue10m/Development/Engine/SharedDefinitions.Engine.Project.ValApi.ValExpApi.Cpp20.h')
assert pin(H)['sha256']=='269A5ADA0B95750F985DEAE0210E1E7192ECBA8668922FFF95DF307CD687FB8F'
abi_path=B/'Hub20261114A-actual-archive-01.json'
output=E/'new-native-types'
if RESUME:
    assert abi_path.is_file() and output.is_dir() and (E/'abi-parser-failure-A.json').is_file()
else:
    assert not abi_path.exists() and not output.exists();output.mkdir()
argv=[sys.executable,'-I','-B','-S','-X','utf8',str(H),'--exe',exe['path'],'--pdb',pdb['path'],'--expected-sha',exe['sha256'],'--expected-pdb-sha',pdb['sha256'],'--definitions',definitions['path'],'--expected-definitions-sha',definitions['sha256'],'--output',str(abi_path)]
invoke('offline-abi',argv,output)
abi=json.loads(abi_path.read_text(encoding='utf-8-sig'))
assert abi['status']=='ABI_48_PAIR_AND_PINNED_DEFINITIONS_VERIFIED'
assert abi['exe']=={**abi['exe'],'path':exe['path'],'sha256':exe['sha256'],'size':exe['size']}
assert abi['pdb']=={**abi['pdb'],'path':pdb['path'],'sha256':pdb['sha256'],'size':pdb['size']}
tool=pin(LLVM); queries=[]

def query(name,global_name=None,type_id=None):
    argv=[str(LLVM),'dump']
    if global_name:argv+=['--globals','--global-name='+global_name]
    elif type_id:argv+=['--types','--type-index='+type_id]
    else:argv+=['--summary']
    argv+=[pdb['path']]
    text=invoke(name,argv,output);queries.append(pin(output/(name+'.process.json')))
    return text

def one(pattern,text):
    matches=re.findall(pattern,text)
    assert len(matches)==1,(pattern,matches,text)
    return matches[0]

cache={}
def type_text(tid,label):
    if tid not in cache:cache[tid]=query(label,type_id=tid)
    return cache[tid]

def resolved(tid,label):
    visited=set()
    for i in range(8):
        assert tid not in visited;visited.add(tid)
        text=type_text(tid,label+'-'+str(i))
        forward=re.search(r'forward ref \(-> (0x[0-9A-Fa-f]+)\)',text)
        if forward:tid=forward[1];continue
        if 'LF_MODIFIER' in text or 'LF_POINTER' in text:
            tid=one(r'referent\s*=\s*(0x[0-9A-Fa-f]+)',text);continue
        return tid,text
    raise AssertionError('Type indirection depth exceeded')

summary=query('summary')
assert abi['pdb_identity']['guid'].casefold() in summary.casefold()
global_text=query('array-global',global_name=abi['producer']['name'])
array_tid=one(r'type = (0x[0-9A-Fa-f]+)',global_text)
array=type_text(array_tid,'array-type')
assert 'LF_ARRAY' in array and re.search(r'\bsize: 96,',array)
element_tid=one(r'element type: (0x[0-9A-Fa-f]+)',array)
element_id,element=resolved(element_tid,'row-type')
assert '`FStructRegisterCompiledInInfo`' in element and 'sizeof 48' in element
fields=type_text(one(r'field list: (0x[0-9A-Fa-f]+)',element),'row-fields')
members={name:(tid,int(offset)) for name,tid,offset in re.findall(r'LF_MEMBER \[name = `([^`]+)`, Type = (0x[0-9A-Fa-f]+), offset = (\d+),',fields)}
assert {k:v[1] for k,v in members.items()}=={'OuterRegister':0,'CreateCppStructOps':8,'Name':16,'Info':24,'VersionInfo':32}
info_ids=[]
for label,row in zip(('unlock','cost'),abi['producer']['rows']):
    text=query('info-'+label+'-global',global_name='Z_Registration_Info_UScriptStruct_F'+row['name'])
    info_ids.append(one(r'type = (0x[0-9A-Fa-f]+)',text))
assert len(set(info_ids))==1
info_id,info=resolved(info_ids[0],'info-type')
assert '`TRegistrationInfo<UScriptStruct,FStructReloadVersionInfo>`' in info and 'sizeof 32' in info
pointer_id,pointer_type=resolved(members['Info'][0],'row-info-pointer')
assert pointer_id==info_id
info_fields=type_text(one(r'field list: (0x[0-9A-Fa-f]+)',info),'info-fields')
info_members={name:(tid,int(offset)) for name,tid,offset in re.findall(r'LF_MEMBER \[name = `([^`]+)`, Type = (0x[0-9A-Fa-f]+), offset = (\d+),',info_fields)}
assert {k:v[1] for k,v in info_members.items()}=={'InnerSingleton':0,'OuterSingleton':8,'ReloadVersionInfo':16}
version_id,version=resolved(members['VersionInfo'][0],'version-type')
assert '`FStructReloadVersionInfo`' in version and 'sizeof 16' in version
assert resolved(info_members['ReloadVersionInfo'][0],'info-version-type')[0]==version_id
version_fields=type_text(one(r'field list: (0x[0-9A-Fa-f]+)',version),'version-fields')
version_members={name:int(offset) for name,offset in re.findall(r'LF_MEMBER \[name = `([^`]+)`, Type = 0x[0-9A-Fa-f]+(?: \([^\r\n]*\))?, offset = (\d+),',version_fields)}
assert version_members=={'Size':0,'Hash':8}
assert pin(archive/'Rogue10m.exe')==exe and pin(archive/'Rogue10m.pdb')==pdb
assert pin(LLVM)==tool
collection=B/'Hub20261114A-disassembly-01';assert not collection.exists()
argv=[sys.executable,'-I','-B','-S','-X','utf8',str(R/'Feature/implementation/npc-intro-compact20261113B/collect_D_abi_disassembly.py'),'--abi-report',str(abi_path),'--expected-abi-report-sha',pin(abi_path)['sha256'],'--output-directory',str(collection)]
invoke('bounded-disassembly',argv,output)
result={'status':'NEW_NATIVE_TYPE_LINKS_AND_BOUNDED_DISASSEMBLY_COLLECTED_MANUAL_REVIEW_REQUIRED','recorded_utc':datetime.now(timezone.utc).isoformat(),'exe':exe,'pdb':pdb,'definitions':definitions,'abi_report':pin(abi_path),'llvm':tool,'query_receipts':queries,'disassembly_collection':pin(collection/'collection.json'),'row':48,'info':32,'version_info':16,'consumer_fingerprint':48,'Game_or_UE_executions':0,'startup_or_global_ABI_certification':False}
write(E/'new-binary-abi-collection.json',result)
print(json.dumps({k:result[k] for k in ('status','exe','pdb','row','info','version_info','consumer_fingerprint')},indent=2))

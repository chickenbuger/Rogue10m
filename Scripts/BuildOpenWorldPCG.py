"""Serialized official Unreal MCP discovery for OpenRunMap vegetation authoring."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'Feature/doc/evidence/open-world-20261005/pcg'
EVIDENCE.mkdir(exist_ok=True)

def call(tool, arguments, name):
    argsfile=EVIDENCE/(name+'-args.json')
    output=EVIDENCE/(name+'.json')
    argsfile.write_text(json.dumps(arguments,ensure_ascii=False,indent=2),encoding='utf-8')
    run=subprocess.run([sys.executable,str(ROOT/'Scripts/InvokeUnrealMCP.py'),
        '--tool',tool,'--arguments-file',str(argsfile),'--output',str(output),'--timeout','120'],
        capture_output=True,text=True,encoding='utf-8')
    if run.returncode:
        raise RuntimeError(name+': '+run.stdout[-3000:]+run.stderr[-1000:])
    data=json.loads(output.read_text(encoding='utf-8'))
    bodies=[json.loads(c['text']) for c in data['result'].get('content',[]) if c.get('type')=='text']
    if len(bodies)!=1:
        raise RuntimeError('Unexpected MCP result '+name)
    value=bodies[0]
    print('MCP',name,'completed',flush=True)
    return value.get('returnValue',value)

def pcg(tool,args,name):
    return call('call_tool',{'toolset_name':'PCGToolset.PCGToolset','tool_name':tool,'arguments':args},name)

def ref(path):
    return {'refPath':path if '.' in path else path+'.'+path.rsplit('/',1)[-1]}

def inspect():
    examples={
        'forest':'/PCGPrimitives/Examples/Forest/Forest_Square_200m',
        'clearing':'/PCGPrimitives/Examples/Forest/Edits/Forest_Square_200m_With_Circled_Clearing',
        'road':'/PCGPrimitives/Examples/Forest/Edits/Forest_Square_200m_Crossed_by_a_15m_Road',
        'asset-parameter':'/PCGPrimitives/Examples/Basics/Basics_WireParameterToAssetArrayPin',
        'distance-filter':'/PCGPrimitives/Examples/Basics/Basics_FilterPointsBasedOnDistanceFrom'}
    for name,path in examples.items():
        pcg('GetGraphStructure',{'graph':ref(path)},'structure-'+name)
        pcg('GetGraphSchema',{'graph':ref(path)},'schema-'+name)
    for name,path in {
        'place':'/PCGPrimitives/Primitives/Place/Place_Assets_OnAGrid',
        'project':'/PCGPrimitives/Primitives/Transform/Transform_Project',
        'spawn':'/PCGPrimitives/Primitives/Spawn/Spawn_Assets',
        'get-actor':'/PCGPrimitives/Primitives/Get/Get_ActorData',
        'distance':'/PCGPrimitives/Primitives/Write/Write_DistanceFrom',
        'filter':'/PCGPrimitives/Primitives/Filter/Filter_AttributeValue'}.items():
        pcg('GetGraphSchema',{'graph':ref(path)},'primitive-'+name)
    call('describe_toolset',{'toolset_name':'editor_toolset.toolsets.programmatic.ProgrammaticToolset'},'programmatic')

def checkpoint():
    command="import unreal; assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Rogue10m/Maps/OpenRunMap.'); assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()"
    run=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',command,'--timeout','12'],
        capture_output=True,text=True,encoding='utf-8')
    if run.returncode:
        raise RuntimeError('Map checkpoint: '+run.stdout[-3000:]+run.stderr[-1000:])

def bake(resume=False):
    sys.path.insert(0,str(ROOT/'Scripts/Editor'))
    import OpenWorldLayout as layout
    graph=ref('/Game/Rogue10m/World/OpenWorld/PCG/PCG_OpenWorldWoodland')
    centers=[(x,y) for x in range(-80000,80001,10000) for y in range(-80000,80001,10000)
             if all(layout.legal_foliage(x+dx,y+dy,road_buffer=2500,clearing_margin=2000)
                    for dx in [-4500,-2250,0,2250,4500] for dy in [-4500,-2250,0,2250,4500])]
    manifest=EVIDENCE/'tiles.json'
    if manifest.exists() and not resume:
        raise RuntimeError('A tile manifest already exists. Inspect its exact completion before resuming.')
    first=json.loads((EVIDENCE/'spawn-first-tile.json').read_text(encoding='utf-8'))
    first=json.loads(first['result']['content'][0]['text'])['returnValue']
    rows=[{'index':0,'center':list(centers[0]),'actor':first['actor'],'messages':[],'generated':True}]
    if resume:
        rows=json.loads(manifest.read_text(encoding='utf-8'))['tiles']
        for row in rows[1:]:
            path=EVIDENCE/('tile-%03d-execute.json'%row['index'])
            raw=json.loads(path.read_text(encoding='utf-8'))
            messages=json.loads(raw['result']['content'][0]['text'])['returnValue']
            if messages:
                raise RuntimeError('Cannot resume after a PCG execution message')
            row.update(generated=True,messages=[])
    manifest.write_text(json.dumps({'status':'generating','tiles':rows},indent=2),encoding='utf-8')
    for index in range(len(rows),len(centers)):
        x,y=centers[index]
        result=pcg('SpawnGraphInstance',{'graph':graph,'name':'OW_ForestTile_%03d'%index,
              'transform':{'location':{'x':x,'y':y,'z':8000},'rotation':{'pitch':0,'yaw':0,'roll':0},
                           'scale':{'x':50,'y':50,'z':80}},
              'jsonParams':json.dumps({'ForestCenter':{'x':x,'y':y,'z':0}})},'tile-%03d-spawn'%index)
        row={'index':index,'center':[x,y],'actor':result['actor'],'generated':False}
        rows.append(row)
        manifest.write_text(json.dumps({'status':'generating','tiles':rows},indent=2),encoding='utf-8')
        messages=pcg('ExecuteGraphInstance',{'pCGVolume':result['actor']},'tile-%03d-execute'%index)
        row['messages']=messages
        if messages:
            manifest.write_text(json.dumps({'status':'execution-message','tiles':rows},indent=2),encoding='utf-8')
            raise RuntimeError('PCG reported messages; preserve graph and stop for review: '+str(messages))
        row['generated']=True
        manifest.write_text(json.dumps({'status':'generating','tiles':rows},indent=2),encoding='utf-8')
        if index%20==0:
            checkpoint()
    checkpoint()
    manifest.write_text(json.dumps({'status':'generated','tiles':rows},indent=2),encoding='utf-8')
    print('Completed',len(rows),'forest tiles',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--inspect',action='store_true')
    parser.add_argument('--bake',action='store_true')
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    if args.inspect:
        inspect()
    if args.bake:
        bake(args.resume)

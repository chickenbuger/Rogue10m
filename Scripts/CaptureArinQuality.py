"""Save actual official Unreal MCP viewport captures without logging pixels."""
import argparse
import base64
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/"Feature/doc/evidence/arin-quality-20261007"
VIEWS={
 "village-front":{"location":{"x":440,"y":-1950,"z":125},"rotation":{"pitch":-8,"yaw":90,"roll":0}},
 "village-face":{"location":{"x":440,"y":-1730,"z":160},"rotation":{"pitch":-3,"yaw":90,"roll":0}},
 "village-oblique":{"location":{"x":620,"y":-1850,"z":135},"rotation":{"pitch":-10,"yaw":126.87,"roll":0}},
}


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--phase",choices=["baseline","refined"],required=True)
    parser.add_argument("--lod",type=int,choices=[0,1,2])
    parser.add_argument("--settled",action="store_true",help="Keep the camera for real editor frames before capturing")
    args=parser.parse_args()
    phase=args.phase
    if args.lod is not None and phase!='refined':
        raise ValueError('Forced LOD review requires refined phase')
    out=EVIDENCE/"unreal-captures"/phase
    if args.lod is not None:
        out=out/("lod-"+str(args.lod))
    out.mkdir(parents=True,exist_ok=True)
    def remote(expression):
        command="import sys,json,importlib; sys.path.insert(0,'D:/Project/Rogue10m/Scripts/Editor'); import ReviewRefinedArinLOD as review; review=importlib.reload(review); print(json.dumps("+expression+"))"
        result=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',command,'--timeout','12'],
                              capture_output=True,text=True,encoding='utf-8')
        body=json.loads(result.stdout)
        if result.returncode or not body.get('success'):
            raise RuntimeError(str(body)+result.stderr[-1000:])
        return json.loads(body['output'][-1]['output'])
    previous=None
    if args.lod is not None:
        previous=remote('review.request('+str(args.lod+1)+',True,False)')
    try:
        capture_views(out,remote,args.lod,args.settled)
    finally:
        if previous is not None:
            restore=remote('review.request('+str(previous['previous_forced_value'])+','+str(previous['previous_update_animation_in_editor'])+','+str(previous['previous_background_throttle'])+')')
            (out/'restored-component.json').write_text(json.dumps(restore,indent=2),encoding='utf-8')


def official(out,stem,name,args):
    argument_file=out/(stem+'-args.json')
    response_file=out/(stem+'-response.json')
    argument_file.write_text(json.dumps({'toolset_name':'EditorToolset.EditorAppToolset','tool_name':name,'arguments':args}),encoding='utf-8')
    process=subprocess.run([sys.executable,str(ROOT/'Scripts/InvokeUnrealMCP.py'),'--tool','call_tool',
        '--arguments-file',str(argument_file),'--output',str(response_file),'--timeout','30'],
        capture_output=True,text=True,encoding='utf-8')
    if process.returncode:
        raise RuntimeError(process.stdout[-1000:]+process.stderr[-1000:])
    response=json.loads(response_file.read_text(encoding='utf-8'))
    if response.get('error') or response['result'].get('isError'):
        raise RuntimeError('Official tool rejected: '+name)
    body=json.loads(next(c['text'] for c in response['result']['content'] if c['type']=='text'))
    return body.get('returnValue')


def capture_views(out,remote,lod,settled=False):
    saved_camera=official(out,'saved-camera','GetCameraTransform',{}) if settled else None
    try:
        _capture_views(out,remote,lod,settled)
    finally:
        if saved_camera is not None:
            official(out,'restored-camera','SetCameraTransform',{'transform':saved_camera})


def _capture_views(out,remote,lod,settled):
    manifest={"source":"actual Epic official Unreal MCP CaptureViewport, editor world", "views":VIEWS,"captures":{},
              'settled_camera':settled,'seconds_before_capture':2 if settled else 0}
    for name,view in VIEWS.items():
        if settled:
            official(out,name+'-camera','SetCameraTransform',{'transform':{**view,'scale':{'x':1,'y':1,'z':1}}})
            time.sleep(2)
        arguments={"toolset_name":"EditorToolset.EditorAppToolset","tool_name":"CaptureViewport",
             "arguments":{"captureTransform":None if settled else {**view,"scale":{"x":1,"y":1,"z":1}},"annotations":None,"bShowUI":False}}
        argument_file=out/(name+"-args.json")
        response_file=out/(name+"-response.json")
        argument_file.write_text(json.dumps(arguments),encoding="utf-8")
        process=subprocess.run([sys.executable,str(ROOT/"Scripts/InvokeUnrealMCP.py"),"--tool","call_tool",
            "--arguments-file",str(argument_file),"--output",str(response_file),"--timeout","30"],
            capture_output=True,text=True,encoding="utf-8")
        if process.returncode:
            raise RuntimeError(process.stdout[-1000:]+process.stderr[-1000:])
        response=json.loads(response_file.read_text(encoding="utf-8"))
        if response.get("error") or response["result"].get("isError"):
            raise RuntimeError("Official capture rejected: "+name)
        body=json.loads(next(c["text"] for c in response["result"]["content"] if c["type"]=="text"))["returnValue"]
        image=body["image"]
        if image["mimeType"]!="image/png":
            raise RuntimeError("Not a PNG")
        path=out/(name+".png")
        path.write_bytes(base64.b64decode(image["data"],validate=True))
        # Store metadata without duplicating megabytes of encoded pixels.
        body["image"]={"mimeType":image["mimeType"],"saved":str(path)}
        response_file.write_text(json.dumps(body,indent=2),encoding="utf-8")
        manifest["captures"][name]=str(path)
        print("Saved actual Unreal capture: "+name,flush=True)
    if lod is not None:
        state=remote('review.inspect()')
        manifest['lod_component']=state
        if state['forced_value']!=lod+1 or state['predicted_lod_index']!=lod:
            raise RuntimeError('Forced LOD did not actually reach the reviewed index: '+str(state))
    (out/"manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")


if __name__=="__main__":
    main()

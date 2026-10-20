"""Read compile, stack, lifecycle, and rendering contracts of all authored effects."""
import json
from pathlib import Path
from AuthorMonsterEffectsMCP import Client,CONFIGS,FXROOT,ref
ROOT=Path(__file__).resolve().parents[1]
def main():
 c=Client('http://127.0.0.1:8002/mcp');report={}
 for name,*_ in CONFIGS:
  p=FXROOT+'/NS_Monster_'+name+'.NS_Monster_'+name
  state=c.tool('GetSystemCompileState',{'system':{'refPath':p}},'compile-'+name)
  issues=c.tool('GetStackIssues',{'system':{'refPath':p}},'issues-'+name)
  summary=c.tool('GetSystemSummary',{'system':{'refPath':p}})
  lifecycle=c.tool('GetModuleInputValues',{'moduleRef':ref(p,'SystemUpdateScript','SystemState',emitter='')})
  renderer=c.tool('GetRendererData',{'rendererRef':{**ref(p),'rendererIndex':0}})
  report[name]={'compile':state,'issues':issues,'summary':summary,'lifecycle':lifecycle,'renderer':renderer}
  print(name,json.dumps({'compile':state,'issues':issues},ensure_ascii=False),flush=True)
 out=ROOT/'Feature/doc/evidence/monster-effects-chase-20261011/asset-validation.json'
 out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if __name__=='__main__':main()

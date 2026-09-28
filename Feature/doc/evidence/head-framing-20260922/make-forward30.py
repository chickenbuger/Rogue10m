from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import re,json
r=Path('D:/Project/Rogue10m');log=(r/'tmp/head-framing/forward30.log').read_text(encoding='utf-8-sig',errors='replace')
assert 'RESULT=BOXING_DIRECT_PREVIEW_PASSED' in log
m=re.search(r'fast=\[(\d+),(\d+)\) slow=\[(\d+),(\d+)\) frames=(\d+)',log);a,b,c,d,total=map(int,m.groups())
out=r/'Feature/doc/images/head-framing-20260922';out.mkdir(parents=True,exist_ok=True);tmp=r/'tmp/head-framing/labeled-forward30';tmp.mkdir(parents=True,exist_ok=True)
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',21)
small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',16)
for n in range(total):
 im=Image.open(r/f'Saved/Screenshots/WindowsEditor/BoxingDirectPreviewForward30/frame_{n:04d}.png').convert('RGB');assert im.size==(1280,720)
 label='원본 속도' if a<=n<b else '0.5배속' if c<=n<d else '기본 Idle 복귀' if n>=d+30 else '원본 끝 자세' if n>=d else '원본 준비 자세'
 dr=ImageDraw.Draw(im);dr.rectangle((0,0,1279,37),fill=(15,20,27));dr.text((13,4),'Boxing.fbx · 머리 카메라 · 아래로 30도 · HUD 숨김 / '+label,font=font,fill=(240,224,190))
 im.save(tmp/f'frame_{n:04d}.png')
samples=[15,a+8,a+13,a+18,a+28,b-1,c+26,total-10]
sheet=Image.new('RGB',(960,1160),(15,20,27))
for i,n in enumerate(samples):
 im=Image.open(tmp/f'frame_{n:04d}.png');im.save(out/f'frame_{n:04d}.jpg',quality=88)
 x=i%2*480;y=i//2*290;sheet.paste(im.resize((480,270)),(x,y+20));ImageDraw.Draw(sheet).text((x+5,y),f'Frame {n}',font=small,fill='white')
sheet.save(out/'gameplay-sheet.jpg',quality=83);sheet.thumbnail((640,800));sheet.save(out/'review-sheet.jpg',quality=63)
(out/'capture.json').write_text(json.dumps(dict(fast=[a,b],slow=[c,d],frames=total,fps=30,duration=total/30),indent=2))
print('Captured frames',total)

from pathlib import Path
import re,json
from PIL import Image,ImageDraw,ImageFont
root=Path('D:/Project/Rogue10m')
log=(root/'tmp/relaxed-idle/preview.log').read_text(encoding='utf-8-sig',errors='replace')
assert 'RESULT=BRAWLER_IDLE_TRANSITION_PASSED' in log
source=root/'Saved/Screenshots/WindowsEditor/BrawlerIdleTransitionLookDown'
out=root/'Feature/doc/images/relaxed-idle-20260922'
frames=root/'tmp/relaxed-idle/labeled';frames.mkdir(parents=True,exist_ok=True)
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',21)
small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',15)
labels=[]
for n in range(450):
    if n<45: label='일반 Idle · 손 내린 기본 자세' + (' / 아래 65도 · UI 숨김' if 20<=n<35 else '')
    elif n<95: label='좌클릭 · 왼잽 → 오른잽'
    elif n<180: label='전투 가드 유지 → 팔 내리기'
    elif n<240: label='우클릭 짧게 · 오른쪽 스트레이트'
    elif n<300: label='전투 가드 유지 → 일반 Idle'
    elif n<345: label='우클릭 길게 · 오른쪽 훅 준비'
    elif n<395: label='우클릭 떼기 · 오른쪽 훅'
    else: label='전투 가드 유지 → 일반 Idle'+(' / 아래 65도 · UI 숨김' if n>=441 else '')
    im=Image.open(source/f'frame_{n:04d}.png').convert('RGB');assert im.size==(1280,720)
    d=ImageDraw.Draw(im);d.rectangle((0,0,1279,37),fill=(15,20,27));d.text((12,4),label,font=font,fill=(240,224,190))
    im.save(frames/f'frame_{n:04d}.png')
    labels.append(label)
samples=[25,40,49,53,65,105,140,165,193,215,332,355,390,415,440,445]
sheet=Image.new('RGB',(1280,1520),(15,20,27))
for i,n in enumerate(samples):
    im=Image.open(frames/f'frame_{n:04d}.png');im.save(out/f'frame_{n:04d}.jpg',quality=88)
    x=i%2*640;y=i//2*190
    sheet.paste(im.resize((320,180)),(x,y+10))
    ImageDraw.Draw(sheet).text((x+325,y+35),f'Frame {n}',font=small,fill='white')
sheet.save(out/'gameplay-sheet.jpg',quality=85)
for page in range(2):
    panel=Image.new('RGB',(640,800),(15,20,27))
    for i,n in enumerate(samples[page*8:page*8+8]):
        im=Image.open(frames/f'frame_{n:04d}.png').resize((320,180))
        panel.paste(im,(i%2*320,i//2*200+20));ImageDraw.Draw(panel).text((i%2*320+3,i//2*200),f'Frame {n}',font=small,fill='white')
    panel.save(out/f'review-sheet-{page}.jpg',quality=66)
(out/'capture.json').write_text(json.dumps(dict(frames=450,fps=30,duration=15,samples=samples),indent=2))
print('450 actual gameplay frames labeled')

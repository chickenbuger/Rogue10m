from pathlib import Path
from PIL import Image,ImageDraw
import subprocess
r=Path('D:/Project/Rogue10m');e=r/'Feature/doc/evidence/third-person-inspection-20260928';d=r/'Saved/Screenshots/WindowsEditor/ThirdPersonInspection'
labels=[(0,1,'First person - actual head camera'),(1,4,'V - third person / current Boxing animation'),(4,5.6,'V - return to first person / UI input check'),(5.6,9.34,'V - repeat view switching')]
f=[]
for a,b,t in labels:
 f.append("drawtext=fontfile='C\\:/Windows/Fonts/arial.ttf':text='"+t+"':x=24:y=20:fontsize=22:fontcolor=white:box=1:boxcolor=black@0.65:boxborderw=10:enable='between(t,"+str(a)+","+str(b)+")'")
with (r/'tmp/third-person-inspection/encode.log').open('w') as log:
 subprocess.run([str(r/'tmp/recorded-reference/decoder/ffmpeg.exe'),'-y','-framerate','30','-start_number','0','-i',str(d/'frame_%04d.png'),'-frames:v','280','-vf',','.join(f),'-c:v','libopenh264','-b:v','5000k','-pix_fmt','yuv420p','-movflags','+faststart',str(e/'v-third-person-gameplay.mp4')],stdout=log,stderr=log,check=True)
out=Image.new('RGB',(1200,450)); draw=ImageDraw.Draw(out)
for i,n in enumerate([15,45,62,105,125,180]):
 im=Image.open(d/f'frame_{n:04d}.png').convert('RGB');im.thumbnail((400,225));out.paste(im,(i%3*400,i//3*225));draw.text((i%3*400+8,i//3*225+8),f'Frame {n}',fill='white')
out.save(e/'contact-sheet.jpg',quality=88)
with (e/'video-metadata.json').open('w') as f:
 subprocess.run([str(r/'tmp/recorded-reference/decoder/ffprobe.exe'),'-v','error','-show_entries','stream=codec_name,width,height,r_frame_rate,nb_frames','-show_entries','format=duration,size','-of','json',str(e/'v-third-person-gameplay.mp4')],stdout=f,check=True)
print('Encoded video and contact sheet.')

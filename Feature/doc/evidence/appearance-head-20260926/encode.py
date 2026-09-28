from pathlib import Path
import subprocess,json
r=Path('D:/Project/Rogue10m');e=r/'Feature/doc/evidence/appearance-head-20260926';ff=r/'tmp/recorded-reference/decoder/ffmpeg.exe'
labels=[(0,1.4,'Appearance Mesh - actual head camera'),(1.4,4.7,'LMB - Boxing left jab / mirrored right jab'),(4.7,6,'Raise view toward target'),(6,9.8,'RMB tap - original straight (misses as head turns)'),(9.8,14.6,'RMB hold and release - original hook'),(14.6,16.3,'Look down - actual body and legs'),(16.3,19,'Walk back and jump - animated head movement')]
filters=[]
for a,b,t in labels:
 filters.append("drawtext=fontfile='C\\:/Windows/Fonts/arial.ttf':text='"+t+"':x=24:y=20:fontsize=22:fontcolor=white:box=1:boxcolor=black@0.65:boxborderw=10:enable='between(t,"+str(a)+","+str(b)+")'")
args=[str(ff),'-y','-framerate','30','-start_number','0','-i',str(r/'Saved/Screenshots/WindowsEditor/AppearanceHeadCamera/frame_%04d.png'),'-frames:v','570','-vf',','.join(filters),'-c:v','libopenh264','-b:v','5500k','-pix_fmt','yuv420p','-movflags','+faststart',str(e/'appearance-head-gameplay.mp4')]
with open(r/'tmp/appearance-head/encode.log','w') as f: subprocess.run(args,stdout=f,stderr=f,check=True)
print(e/'appearance-head-gameplay.mp4')

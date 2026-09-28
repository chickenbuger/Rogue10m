$ErrorActionPreference='Stop'
$ff='D:/Project/Rogue10m/tmp/recorded-reference/decoder/ffmpeg.exe'
$probe='D:/Project/Rogue10m/tmp/recorded-reference/decoder/ffprobe.exe'
$out='Feature/doc/images/head-framing-20260922'
& 'C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tmp/head-framing/make-forward30.py
if($LASTEXITCODE -ne 0) { throw 'label failed' }
& $ff -hide_banner -loglevel error -y -framerate 30 -i 'tmp/head-framing/labeled-forward30/frame_%04d.png' -c:v libopenh264 -b:v 4000k -pix_fmt yuv420p -movflags +faststart "$out/head-view-30.mp4"
if($LASTEXITCODE -ne 0) { throw 'encode failed' }
& $ff -hide_banner -loglevel error -y -i 'Feature/doc/images/boxing-direct-head-20260922/boxing-direct-lookdown.mp4' -i "$out/head-view-30.mp4" -filter_complex '[0:v]trim=end_frame=248,setpts=PTS-STARTPTS,scale=960:540[a];[1:v]trim=end_frame=248,setpts=PTS-STARTPTS,scale=960:540[b];[a][b]hstack=inputs=2[v]' -map '[v]' -an -c:v libopenh264 -b:v 6000k -pix_fmt yuv420p -movflags +faststart "$out/head-view-40-vs-30.mp4"
if($LASTEXITCODE -ne 0) { throw 'comparison failed' }
& $probe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate,nb_frames,duration -of json "$out/head-view-40-vs-30.mp4" | Set-Content 'Feature/doc/evidence/head-framing-20260922/video-probe.json' -Encoding UTF8
& $ff -hide_banner -loglevel error -y -ss 1.43 -i "$out/head-view-40-vs-30.mp4" -frames:v 1 -vf scale=1280:360 'tmp/head-framing/comparison-review.jpg'

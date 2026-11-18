param()
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Plan = Get-Content -LiteralPath (Join-Path $ProjectRoot 'Docs/References/WorldMap/landscape-production-20261116.json') -Raw | ConvertFrom-Json
$Destination = Join-Path $ProjectRoot 'Feature/doc/evidence/landscape-production-20261116/landscape-plan.png'
$Bitmap = [System.Drawing.Bitmap]::new(1740,1190)
$Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
$Graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$Graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$Brushes = [System.Collections.Generic.List[System.IDisposable]]::new()
function Brush([string]$Hex) { $b = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml($Hex)); $Brushes.Add($b); return $b }
function Pen([string]$Hex,[single]$Width) { $p = [System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml($Hex),$Width); $p.StartCap='Round'; $p.EndCap='Round'; $Brushes.Add($p); return $p }
function Font([single]$Size,[bool]$Bold=$false) { $style=if($Bold){[System.Drawing.FontStyle]::Bold}else{[System.Drawing.FontStyle]::Regular};$f=[System.Drawing.Font]::new('Malgun Gothic',$Size,$style,[System.Drawing.GraphicsUnit]::Pixel);$Brushes.Add($f);return $f }
$Text = Brush '#203c38'; $Muted=Brush '#627975'; $White=Brush '#ffffff'
$TitleFont=Font 34 $true; $SubFont=Font 21; $LabelFont=Font 18 $true; $SmallFont=Font 16
$Graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#f7f5ec'))
$Graphics.DrawString('Rogue10m  |  Landscape 제작 기획',$TitleFont,$Text,65,35)
$Graphics.DrawString('큰 마을 3  ·  전문 초소 5  ·  중앙 직행 300m  ·  전체 도로 29개 간선',$SubFont,$Text,68,88)
$Graphics.DrawString('1,012.5 × 562.5m   /   9 × 5 컴포넌트   /   기존 배치 좌표 유지   /   2026-11-16',$SubFont,$Muted,68,124)
$PlotX=80.0; $PlotY=215.0; $Scale=1.54
$PlotWidth=1012.5*$Scale; $PlotHeight=562.5*$Scale
function XY([double]$X,[double]$Y) { return @([single]($PlotX+($X+487.5)*$Scale),[single]($PlotY+(462-$Y)*$Scale)) }
$Graphics.FillRectangle((Brush '#e3ead5'),[single]$PlotX,[single]$PlotY,[single]$PlotWidth,[single]$PlotHeight)
# Subtle region areas communicate terrain intent, not measured height contours.
foreach($region in @(@(-315,205,150,150,'#cbdcc1'),@(350,275,140,130,'#dfd8bd'),@(120,315,110,90,'#d3ded6'))) {
    $c=XY $region[0] $region[1];$rx=$region[2]*$Scale;$ry=$region[3]*$Scale
    $Graphics.FillEllipse((Brush $region[4]),[single]($c[0]-$rx),[single]($c[1]-$ry),[single](2*$rx),[single](2*$ry))
}
$Grid=Pen '#c0cdb8' 1
for($i=0;$i -le 9;$i++){$px=$PlotX+$i*112.5*$Scale;$Graphics.DrawLine($Grid,[single]$px,[single]$PlotY,[single]$px,[single]($PlotY+$PlotHeight))}
for($i=0;$i -le 5;$i++){$py=$PlotY+$i*112.5*$Scale;$Graphics.DrawLine($Grid,[single]$PlotX,[single]$py,[single]($PlotX+$PlotWidth),[single]$py)}
$Graphics.DrawRectangle((Pen '#65816c' 2),[single]$PlotX,[single]$PlotY,[single]$PlotWidth,[single]$PlotHeight)
$ById=@{};foreach($n in $Plan.nodes){$ById[$n.id]=$n}
$Arena=XY 0 354.5;$r=55*$Scale
$Graphics.FillEllipse((Brush '#e6c9c0'),[single]($Arena[0]-$r),[single]($Arena[1]-$r),[single](2*$r),[single](2*$r))
$Graphics.DrawEllipse((Pen '#965349' 2),[single]($Arena[0]-$r),[single]($Arena[1]-$r),[single](2*$r),[single](2*$r))
$Graphics.DrawString('아자토르 전투 대지',$LabelFont,$Text,[single]($Arena[0]-90),[single]($Arena[1]-25))
$Graphics.DrawString('반경 55m / 고도 96m',$SmallFont,$Muted,[single]($Arena[0]-89),[single]($Arena[1]+3))
foreach($e in $Plan.edges){
    $a=$ById[$e.from];$b=$ById[$e.to];$p=XY $a.localM.x $a.localM.y;$q=XY $b.localM.x $b.localM.y
    $hex=if($e.role -eq 'central_direct'){'#d99b2d'}elseif($a.localM.x -lt 0 -or $b.localM.x -lt 0){'#4c8b65'}elseif(@('T','O','R','BE') -contains $e.from -or @('T','O','R','BE') -contains $e.to){'#528fa8'}else{'#bb8051'}
    $Graphics.DrawLine((Pen '#fbfaf1' 15),$p[0],$p[1],$q[0],$q[1])
    $Graphics.DrawLine((Pen $hex 8),$p[0],$p[1],$q[0],$q[1])
}
$Offsets=@{S=@(14,0);J100=@(-114,12);J180=@(-123,-8);J240=@(-128,-35);B=@(-106,10);H=@(-90,58);P=@(-100,35);W=@(-120,-4);Q=@(-135,-7);M=@(-40,58);A=@(16,6);G=@(16,18);T=@(21,-40);O=@(18,-1);F=@(22,8);Z=@(20,-25);R=@(-45,-33);BM=@(-150,8);BW=@(-150,-31);BE=@(20,-23);BG=@(-35,-38)}
$Short=@{S='S  필드 진입';J100='남쪽 갈림길';J180='중앙 갈림길';J240='북쪽 갈림길';B='B  보스 입구';H='H  낙엽 수렵촌';P='P  산림초소';W='W  숲 사냥터';Q='Q  돌샘 수련초소';M='M  풍차장터';A='A  운송길';G='G  수비초소';T='T  기록촌 마탑';O='O  관측초소';F='F  잔불 대장간';Z='Z  제련로';R='R  유적';BM='Ⅰ 포자 군락의 어미';BW='Ⅱ 가시 성채의 수문장';BE='Ⅲ 심연을 보는 눈';BG='Ⅳ 황금 포식자'}
foreach($n in $Plan.nodes){
    $p=XY $n.localM.x $n.localM.y
    if($n.baselinePadRadiusM -and $n.kind -ne 'final_boss_entrance'){
        $r=$n.baselinePadRadiusM*$Scale
        $Graphics.FillEllipse((Brush '#f3edd6'),[single]($p[0]-$r),[single]($p[1]-$r),[single](2*$r),[single](2*$r))
        $Graphics.DrawEllipse((Pen '#a6ad8a' 1.5),[single]($p[0]-$r),[single]($p[1]-$r),[single](2*$r),[single](2*$r))
    }
    $color=switch($n.kind){'major_hub'{'#345f50'}'outpost'{'#5f7c7c'}'optional_mid_boss'{'#ad5648'}'final_boss_entrance'{'#ad5648'}default{'#8a9b74'}}
    $Graphics.FillEllipse((Brush $color),[single]($p[0]-7),[single]($p[1]-7),14,14)
    $o=$Offsets[$n.id];$tx=[single]($p[0]+$o[0]);$ty=[single]($p[1]+$o[1]);$label=$Short[$n.id]
    $size=$Graphics.MeasureString($label,$LabelFont)
    $Graphics.FillRectangle((Brush '#f9f7ed'),$tx-2,$ty,[single]($size.Width+4),[single]$size.Height)
    $Graphics.DrawString($label,$LabelFont,$Text,$tx,$ty)
}
$Graphics.DrawString('북 +Y ↑',$LabelFont,$Text,1510,175)
$Graphics.DrawString('가로 1,012.5m  /  격자 한 칸 112.5m',$SmallFont,$Muted,80,1090)
$Graphics.DrawString('제작 배치도 · 원형은 확보 대지 · 배경색은 지역 구분 · Unreal 캡처/높이 실측 아님',$SmallFont,$Muted,80,1125)
$Graphics.DrawString('중앙로: 8m 통행 폭 / 총 14m 정지 어깨 · 마을 방문·보스 처치 강제 없음',$SmallFont,$Text,820,1090)
$Graphics.DrawString('2차 검증: 높이·페인트·레이어 보존 → native 외곽 정리 → 충돌·기본 보행',$SmallFont,$Text,820,1125)
try { $Bitmap.Save($Destination,[System.Drawing.Imaging.ImageFormat]::Png) }
finally { $Graphics.Dispose();$Bitmap.Dispose();foreach($b in $Brushes){$b.Dispose()} }
Write-Output $Destination

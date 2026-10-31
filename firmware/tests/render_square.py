#!/usr/bin/env python3
"""编译并运行桌面模拟预览，输出 PNG（标注 SIMULATED），并与效果图并排对比。
用法: python3 firmware/tests/render_square.py [输出目录] [效果图路径]"""
import os, shlex, subprocess, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'firmware/.preview')
mock = Path(sys.argv[2]) if len(sys.argv) > 2 else None
out.mkdir(parents=True, exist_ok=True)
cxx = shlex.split(os.environ['CXX']) if os.environ.get('CXX') else ['g++', '-O2']   # 例：CXX='python3 -m ziglang c++ -O0'
subprocess.run(cxx + ['-std=c++17', '-I', str(ROOT / 'firmware/boards/216/MotoNav'),
                str(ROOT / 'firmware/tests/render_square.cpp'), '-o', '/tmp/render_square'], check=True)
subprocess.run(['/tmp/render_square', str(out)], check=True)
font = ImageFont.load_default()
pngs = {}
for p in sorted(out.glob('*.ppm')):
    im = Image.open(p).convert('RGB')
    im.save(p.with_suffix('.png')); p.unlink()
    pngs[p.stem] = im
# 效果图里六个界面的位置（1536x1024 效果图）
BOX = {'nav': (558, 137, 848, 452), 'battery': (888, 137, 1178, 452), 'cells': (1217, 137, 1507, 452),
       'music': (558, 578, 848, 882), 'ble': (888, 578, 1178, 882), 'menu': (1217, 578, 1507, 882)}
if mock and mock.exists():
    m = Image.open(mock).convert('RGB')
    for k, box in BOX.items():
        ref = m.crop(box).resize((480, 480), Image.LANCZOS)
        cmp_ = Image.new('RGB', (980, 510), (30, 30, 30))
        cmp_.paste(ref, (0, 30)); cmp_.paste(pngs[k], (500, 30))
        d = ImageDraw.Draw(cmp_)
        d.text((8, 8), f'REFERENCE mockup ({k})', fill=(255, 255, 255), font=font)
        d.text((508, 8), 'SIMULATED host preview (shared firmware layout code, mock data)', fill=(255, 160, 40), font=font)
        cmp_.save(out / f'compare_{k}.png')
print('PNG written to', out)

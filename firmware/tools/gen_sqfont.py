#!/usr/bin/env python3
"""生成方屏 UI 用的抗锯齿位图字体 SqFont.h（4 位灰度，按码位排序，二分查找）。

字形来源：Noto Sans CJK SC（SIL OFL 1.1，Debian 包 fonts-noto-cjk）。
用到的中文字自动从 SqPages.h（以及下面 EXTRA）里扫描，改了界面文字后重新运行即可：
    python3 firmware/tools/gen_sqfont.py
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'boards/216/MotoNav'
OUT = SRC / 'SqFont.h'
REG = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
BOLD = '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
EXTRA = '…“”·×°～'
PREVIEW_ONLY = '进入科技园路É'  # 仅桌面预览替身文字用到（手机文字图片的替身）

ascii_all = ''.join(chr(c) for c in range(32, 127))
text = (SRC / 'SqPages.h').read_text(encoding='utf-8')
cjk = sorted({ch for ch in text + EXTRA + PREVIEW_ONLY if ord(ch) > 126})
full = ascii_all + ''.join(cjk)
digits = ' 0123456789.-:%'

FONTS = [  # 名称, 字号(px), 字体, 字符集
    ('sqF16', 16, REG, full),
    ('sqF20', 20, REG, full),
    ('sqF24', 24, BOLD, full),
    ('sqF32', 32, BOLD, ascii_all + '°'),
    ('sqF64', 64, BOLD, digits),
    ('sqF96', 96, BOLD, digits),
]


def build(name, size, path, chars):
    font = ImageFont.truetype(path, size, index=2)  # index 2 = SC
    asc, desc = font.getmetrics()
    # Noto CJK 的 ascent 偏大（含上方留白），用 "H" 的顶部把行顶收紧
    top_h = font.getbbox('H')[1]
    line_top = max(0, top_h - max(1, size // 8))
    glyphs, blob = [], bytearray()
    for ch in sorted(set(chars), key=ord):
        bbox = font.getbbox(ch)
        adv = int(round(font.getlength(ch)))
        if bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            glyphs.append((ord(ch), len(blob), 0, 0, 0, 0, adv))
            continue
        w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        img = Image.new('L', (w, h), 0)
        ImageDraw.Draw(img).text((-bbox[0], -bbox[1]), ch, font=font, fill=255)
        px = img.load()
        off = len(blob)
        for y in range(h):
            row = [min(15, (px[x, y] + 8) // 17) for x in range(w)]
            if w % 2:
                row.append(0)
            for i in range(0, len(row), 2):
                blob.append((row[i] << 4) | row[i + 1])
        glyphs.append((ord(ch), off, w, h, bbox[0], bbox[1] - line_top, adv))
    out = [f'static const SqGlyph {name}_g[] = {{']
    for g in glyphs:
        out.append('  {%d,%d,%d,%d,%d,%d,%d},' % g)
    out.append('};')
    out.append(f'static const uint8_t {name}_b[] = {{')
    for i in range(0, len(blob), 32):
        out.append('  ' + ','.join(str(b) for b in blob[i:i + 32]) + ',')
    out.append('};')
    out.append(f'static const SqFont {name} = {{{name}_g, {len(glyphs)}, {name}_b, {size}, {asc - line_top}}};')
    return '\n'.join(out), len(blob)


parts = ['// 自动生成：firmware/tools/gen_sqfont.py —— 请勿手改', '// 字形：Noto Sans CJK SC（SIL Open Font License 1.1）',
         '#pragma once', '#include <stdint.h>',
         'struct SqGlyph { uint32_t cp; uint32_t off; uint8_t w, h; int8_t xo, yo; uint8_t adv; };',
         'struct SqFont { const SqGlyph *g; uint16_t n; const uint8_t *bmp; uint8_t size, asc; };']
total = 0
for spec in FONTS:
    src, n = build(*spec)
    parts.append(src)
    total += n
OUT.write_text('\n'.join(parts) + '\n', encoding='utf-8')
print(f'{OUT}: {len(cjk)} CJK chars, {total} bitmap bytes')

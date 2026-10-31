#!/usr/bin/env python3
"""Generate a small 20px Chinese UI glyph subset from Noto Sans CJK SC.

Requires Pillow and the NotoSansCJK-Regular.ttc font; pass a different source
path as argv[1]. Output is firmware/boards/216/MotoNav/SquareGlyphData.h.
Font copyright and SIL OFL are reproduced in firmware/licenses/.
"""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
fontpath = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
font = ImageFont.truetype(fontpath, 20, index=2)
chars = sorted(set('导航音乐电池蓝牙设置关于功能菜单总电压剩余容量流温度健康状态芯单体最高最低压差连接设备扫描断开附近点击查看选择正在离线数据模拟'))
lines = ['#pragma once', '#include <stdint.h>', '// Generated from Noto Sans CJK SC; see firmware/licenses/Noto-CJK-copyright.txt.', 'namespace SquareGlyphData {', 'struct Glyph { uint16_t code; uint8_t pixels[200]; };', 'static const Glyph glyphs[] = {']
for ch in chars:
    im = Image.new('L', (20, 20), 0)
    ImageDraw.Draw(im).text((0, -6), ch, font=font, fill=255)
    values = [(v * 15 + 127) // 255 for v in im.getdata()]
    packed = [(values[i] << 4) | values[i + 1] for i in range(0, 400, 2)]
    lines.append('  {0x%04x, {%s}},' % (ord(ch), ','.join('0x%02x' % n for n in packed)))
lines += ['};', 'static constexpr unsigned count = sizeof(glyphs) / sizeof(glyphs[0]);', '}']
(root / 'boards/216/MotoNav/SquareGlyphData.h').write_text('\n'.join(lines) + '\n')

# A separate bold numeric subset for the large SOC readout.
boldpath = sys.argv[2] if len(sys.argv) > 2 else '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
bold = ImageFont.truetype(boldpath, 55, index=2)
lines = ['#pragma once', '#include <stdint.h>', '// Generated from Noto Sans CJK SC Bold; see firmware/licenses/Noto-CJK-copyright.txt.', 'namespace SquareNumericData {']
entries = []
for ch in '0123456789.-%':
    width = int(bold.getlength(ch) + .5)
    im = Image.new('L', (width, 48), 0)
    ImageDraw.Draw(im).text((0, -20), ch, font=bold, fill=255)
    values = [(v * 15 + 127) // 255 for v in im.getdata()]
    if len(values) % 2: values.append(0)
    packed = [(values[i] << 4) | values[i + 1] for i in range(0, len(values), 2)]
    name = 'g' + str(ord(ch))
    lines.append('static const uint8_t %s[] = {%s};' % (name, ','.join('0x%02x' % n for n in packed)))
    entries.append("  {%d,%d,%s}," % (ord(ch),width,name))
lines += ['struct Glyph { char code; uint8_t width; const uint8_t *pixels; };', 'static const Glyph glyphs[] = {'] + entries + ['};', 'static constexpr unsigned count = sizeof(glyphs) / sizeof(glyphs[0]);', '}']
(root / 'boards/216/MotoNav/SquareNumericData.h').write_text('\n'.join(lines) + '\n')

# Compact bold numbers and units used in the six data cards and cell rows.
small = ImageFont.truetype(boldpath, 20, index=2)
lines = ['#pragma once', '#include <stdint.h>', '// Generated from Noto Sans CJK SC Bold; see firmware/licenses/Noto-CJK-copyright.txt.', 'namespace SquareSmallData {']
entries = []
for ch in dict.fromkeys('0123456789.-% ABCDEFGHIJKLMNOPQRSTUVWXYZhm'):
    width = max(1, int(small.getlength(ch) + .5))
    im = Image.new('L', (width, 24), 0)
    ImageDraw.Draw(im).text((0, -5), ch, font=small, fill=255)
    values = [(v * 15 + 127) // 255 for v in im.getdata()]
    packed = [(values[i] << 4) | values[i + 1] for i in range(0, len(values), 2)]
    name = 'g' + str(ord(ch))
    lines.append('static const uint8_t %s[] = {%s};' % (name, ','.join('0x%02x' % n for n in packed)))
    entries.append('  {%d,%d,%s},' % (ord(ch),width,name))
lines += ['struct Glyph { char code; uint8_t width; const uint8_t *pixels; };', 'static const Glyph glyphs[] = {'] + entries + ['};', 'static constexpr unsigned count = sizeof(glyphs) / sizeof(glyphs[0]);', '}']
(root / 'boards/216/MotoNav/SquareSmallData.h').write_text('\n'.join(lines) + '\n')

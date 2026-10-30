// SqGfx.h — 480x480 方屏 UI 的基础绘图（纯 C++，不依赖 Arduino）
// 固件和桌面预览（firmware/tests/render_square.cpp）共用同一份代码，
// 所以桌面预览图与固件画面的布局、字体、颜色完全一致（预览仍属“模拟预览”，不是实机截图）。
// 所有图形都按“到边缘的距离”算覆盖率做抗锯齿，直接写 RGB565 帧缓冲。
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "SqFont.h"

namespace sq {

static const int W = 480, H = 480;

static inline uint16_t rgb(uint32_t hex) {
  return (uint16_t)(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F));
}

// ---- 颜色（取自效果图）----
static const uint32_t BG = 0x000000, CARD = 0x111A2B, CARD_HI = 0x16233A, CARD_SEL = 0x0F2147,
  BLUE = 0x1E7BFF, BLUE_D = 0x0A3D91, GREEN = 0x22D66B, GREEN_D = 0x0E4B2C, RED = 0xFF3D3D,
  RED_D = 0x3A1420, ORANGE = 0xFF9F1A, YELLOW = 0xF5C518, TXT = 0xFFFFFF, TXT2 = 0x8D97AA,
  TXT3 = 0x4C566A, TRACK = 0x1B2436;

struct Canvas {
  uint16_t *fb;
  int clipY0 = 0, clipY1 = H;           // 可选纵向裁剪（列表滚动用）
  explicit Canvas(uint16_t *f) : fb(f) {}

  inline void blend(int x, int y, uint16_t c, int a) {   // a: 0~255
    if ((unsigned)x >= (unsigned)W || y < clipY0 || y >= clipY1 || a <= 0) return;
    uint16_t &d = fb[y * W + x];
    if (a >= 255) { d = c; return; }
    uint32_t k = (uint32_t)(a + 4) >> 3;                 // 0~32
    uint32_t s = ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
    uint32_t t = ((uint32_t)d | ((uint32_t)d << 16)) & 0x07E0F81Fu;
    uint32_t r = (t + (((s - t) * k) >> 5)) & 0x07E0F81Fu;
    d = (uint16_t)(r | (r >> 16));
  }
  void clear(uint32_t col) {
    uint16_t c = rgb(col);
    for (int i = 0; i < W * H; i++) fb[i] = c;
  }
  void fillRect(int x, int y, int w, int h, uint32_t col) {
    uint16_t c = rgb(col);
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) blend(i, j, c, 255);
  }

  // 圆角矩形（抗锯齿）。a = 整体不透明度
  void roundRect(float x, float y, float w, float h, float r, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    float cx = x + w / 2, cy = y + h / 2, hx = w / 2 - r, hy = h / 2 - r;
    for (int j = (int)floorf(y); j < (int)ceilf(y + h); j++) {
      if (j < clipY0 || j >= clipY1) continue;
      for (int i = (int)floorf(x); i < (int)ceilf(x + w); i++) {
        float qx = fabsf(i + 0.5f - cx) - hx, qy = fabsf(j + 0.5f - cy) - hy;
        float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
        float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.0f) - r;
        float cov = 0.5f - d;
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
    }
  }
  // 圆角矩形描边
  void roundRectStroke(float x, float y, float w, float h, float r, float t, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    float cx = x + w / 2, cy = y + h / 2, hx = w / 2 - r, hy = h / 2 - r;
    for (int j = (int)floorf(y); j < (int)ceilf(y + h); j++)
      for (int i = (int)floorf(x); i < (int)ceilf(x + w); i++) {
        float qx = fabsf(i + 0.5f - cx) - hx, qy = fabsf(j + 0.5f - cy) - hy;
        float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
        float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.0f) - r;
        float cov = fminf(0.5f - d, 0.5f + d + t);
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
  }
  void disc(float cx, float cy, float r, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
      for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
        float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
        float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
  }
  void ring(float cx, float cy, float r, float t, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
      for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
        float dx = i + 0.5f - cx, dy = j + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
        float cov = fminf(r + 0.5f - d, d - (r - t) + 0.5f);
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
  }
  // 粗线段（圆头）
  void line(float x0, float y0, float x1, float y1, float w, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    float r = w / 2, vx = x1 - x0, vy = y1 - y0, L2 = vx * vx + vy * vy;
    int ia = (int)floorf(fminf(x0, x1) - r - 1), ib = (int)ceilf(fmaxf(x0, x1) + r + 1);
    int ja = (int)floorf(fminf(y0, y1) - r - 1), jb = (int)ceilf(fmaxf(y0, y1) + r + 1);
    for (int j = ja; j <= jb; j++)
      for (int i = ia; i <= ib; i++) {
        float px = i + 0.5f - x0, py = j + 0.5f - y0;
        float t = L2 > 0 ? (px * vx + py * vy) / L2 : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        float dx = px - vx * t, dy = py - vy * t;
        float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
  }
  // 凸多边形（最多 8 点，顺/逆时针都可）
  void poly(const float *xs, const float *ys, int n, uint32_t col, int a = 255) {
    uint16_t c = rgb(col);
    float x0 = xs[0], x1 = xs[0], y0 = ys[0], y1 = ys[0], area = 0;
    for (int k = 0; k < n; k++) {
      x0 = fminf(x0, xs[k]); x1 = fmaxf(x1, xs[k]); y0 = fminf(y0, ys[k]); y1 = fmaxf(y1, ys[k]);
      int m = (k + 1) % n; area += xs[k] * ys[m] - xs[m] * ys[k];
    }
    float sgn = area > 0 ? 1.0f : -1.0f;
    for (int j = (int)floorf(y0) - 1; j <= (int)ceilf(y1); j++)
      for (int i = (int)floorf(x0) - 1; i <= (int)ceilf(x1); i++) {
        float px = i + 0.5f, py = j + 0.5f, dmin = 1e9f;
        for (int k = 0; k < n; k++) {
          int m = (k + 1) % n;
          float ex = xs[m] - xs[k], ey = ys[m] - ys[k], L = sqrtf(ex * ex + ey * ey);
          if (L <= 0) continue;
          float d = sgn * ((px - xs[k]) * ey - (py - ys[k]) * ex) / L;   // 内侧为负
          d = -d;
          if (d < dmin) dmin = d;
        }
        float cov = dmin + 0.5f;
        if (cov <= 0) continue;
        blend(i, j, c, cov >= 1 ? a : (int)(a * cov));
      }
  }
  void tri(float ax, float ay, float bx, float by, float cx, float cy, uint32_t col, int a = 255) {
    float xs[3] = {ax, bx, cx}, ys[3] = {ay, by, cy};
    poly(xs, ys, 3, col, a);
  }
  // 圆弧带：角度按屏幕坐标（0° = 右，顺时针为正，单位度），从 a0 顺时针扫到 a1，圆头
  void arc(float cx, float cy, float r, float t, float a0, float a1, uint32_t col, bool roundCaps = true) {
    uint16_t c = rgb(col);
    float rin = r - t, rm = r - t / 2;
    float sweep = a1 - a0; while (sweep < 0) sweep += 360;
    for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
      for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
        float dx = i + 0.5f - cx, dy = j + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
        if (d < rin - 1.5f || d > r + 1.5f) continue;
        float ang = atan2f(dy, dx) * 57.29578f;
        float rel = ang - a0; while (rel < 0) rel += 360; while (rel >= 360) rel -= 360;
        float cov;
        if (rel <= sweep) cov = fminf(r + 0.5f - d, d - rin + 0.5f);
        else cov = 0;
        if (cov > 0) blend(i, j, c, cov >= 1 ? 255 : (int)(255 * cov));
      }
    if (roundCaps) {
      float r0 = a0 / 57.29578f, r1 = (a0 + sweep) / 57.29578f;
      disc(cx + rm * cosf(r0), cy + rm * sinf(r0), t / 2, col);
      disc(cx + rm * cosf(r1), cy + rm * sinf(r1), t / 2, col);
    }
  }

  // ---- 文字 ----
  static uint32_t utf8Next(const char *&s) {
    const uint8_t *p = (const uint8_t *)s;
    uint32_t c = *p++;
    if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    s = (const char *)p;
    return c;
  }
  static const SqGlyph *glyph(const SqFont &f, uint32_t cp) {
    int lo = 0, hi = f.n - 1;
    while (lo <= hi) {
      int m = (lo + hi) / 2;
      if (f.g[m].cp == cp) return &f.g[m];
      if (f.g[m].cp < cp) lo = m + 1; else hi = m - 1;
    }
    return nullptr;
  }
  static int textW(const SqFont &f, const char *s) {
    int w = 0;
    while (*s) { const SqGlyph *g = glyph(f, utf8Next(s)); w += g ? g->adv : f.size / 2; }
    return w;
  }
  // align: 0 左，1 居中，2 右；y = 文字行顶部。返回右边界 x
  int text(const SqFont &f, int x, int y, const char *s, uint32_t col, int align = 0, int a = 255) {
    if (align) x -= align == 1 ? textW(f, s) / 2 : textW(f, s);
    uint16_t c = rgb(col);
    while (*s) {
      const SqGlyph *g = glyph(f, utf8Next(s));
      if (!g) { x += f.size / 2; continue; }
      const uint8_t *b = f.bmp + g->off;
      int bpr = (g->w + 1) / 2;
      for (int j = 0; j < g->h; j++)
        for (int i = 0; i < g->w; i++) {
          uint8_t v = b[j * bpr + i / 2];
          v = (i & 1) ? (v & 15) : (v >> 4);
          if (v) blend(x + g->xo + i, y + g->yo + j, c, v == 15 ? a : v * 17 * a / 255);
        }
      x += g->adv;
    }
    return x;
  }
};

}  // namespace sq

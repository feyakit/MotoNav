// SqPages.h — 480x480 方屏页面（导航卡片、电池主页、24 串电芯、音乐、蓝牙设备、2x3 功能菜单、设置/关于）
// 纯 C++，固件与桌面预览共用；布局常量都在这里，按效果图换算到 480x480。
// 页面只“画”，不做通信；所有实时数据通过 SqState 传入，未知数据一律显示 "--"。
#pragma once
#include <stdio.h>
#include "SqGfx.h"
#include "BmsData.h"

namespace sq {

// 页面编号（和 MotoNav.ino 的 page 变量一致；0~2 是原有页面）
enum { P_NAV = 0, P_WEATHER = 1, P_MUSIC = 2, P_BATTERY = 3, P_CELLS = 4, P_BLE = 5, P_MENU = 6, P_SETTINGS = 7, P_ABOUT = 8 };

struct ScanItem { char name[24]; char addr[18]; int rssi; };

struct LinkInfo {
  // 0 未选择设备，1 已选择未连接（等待重连），2 连接中，3 已连接（发现服务中），4 已连接并已订阅，5 连接失败/断开
  int state = 0;
  char name[24] = "";
  char addr[18] = "";
  int rssi = -999;
  int services = 0, subs = 0;
  uint32_t rx = 0, lastRxMs = 0;
};

struct State {
  uint32_t now = 0;
  int clockMin = -1;                       // 当天分钟数；-1 = 不知道（设备没有可靠时钟时不显示）
  bool phoneConn = false, phoneAuthed = false;
  int phoneRssi = -999;
  const char *bleName = "MotoNav";
  const char *fw = "";
  int boardBatPct = -1;
  // 电池
  const BmsData *bms = nullptr;
  bool simOn = false;
  LinkInfo link;
  const ScanItem *scan = nullptr;
  int scanN = 0;
  bool scanning = false;
  // 导航
  bool inNav = false;
  int navDist = 0, speed = -1, camLimit = 0;
  bool hasCam = false, nearTurn = false;
  bool tripOn = false;
  int tripRemainM = 0, tripEtaHHMM = -1;
  float tripFrac = -1;                     // 行程进度 0~1；-1 = 没有
  // 音乐
  bool musicLinked = false, musicHave = false, musicPlaying = false;
  const char *musicState = "";
  int musicElapsed = -1, musicTotal = -1;
  int musicPressed = -1;
  // 页面内状态
  int cellMode = 0;                        // 0 列表，1 柱状图
  int cellScroll = 0;                      // 列表滚动（像素）
  int bleScroll = 0;                       // 扫描结果滚动（行）
  // 钩子：由固件提供（手机发来的文字图片、矢量转向图标、车道），预览程序提供替身
  bool (*slotAvail)(int slot) = nullptr;
  void (*slot)(int slot, int x, int y, int maxW, int align, uint8_t tint) = nullptr;
  void (*turnIcon)(float cx, float top, float bottom, float maxW, bool near) = nullptr;
  void (*lanes)(float cx, float y, float maxW, float h) = nullptr;
  bool haveLanes = false;
};

// ---------------- 布局常量（480x480） ----------------
static const int SB_H = 46;                                  // 状态栏高度
static const int SB_SOC_X0 = 176, SB_SOC_X1 = 350;           // 点 SOC 进入电池主页的区域
static const int G_CX = 240, G_CY = 248, G_R = 196, G_T = 30; // SOC 圆弧
static const float G_A0 = 172, G_SWEEP = 196;
static const int CARD_X[3] = {20, 170, 320}, CARD_W = 140, CARD_Y[2] = {294, 386}, CARD_H = 84;
static const int CL_Y0 = 112, CL_RH = 40, CL_ROWS = 6;       // 电芯列表
static const int CL_TAB_Y = 56, CL_TAB_H = 46;
static const int CS_Y = 362;                                  // 电芯统计卡
static const int MU_BTN_Y = 404, MU_BTN_X[3] = {128, 240, 352}, MU_BTN_R = 46;
static const int BL_ROW_Y0 = 108, BL_ROW_H = 66, BL_ROW_GAP = 8, BL_ROWS = 4;
static const int BL_BTN_X = 110, BL_BTN_Y = 412, BL_BTN_W = 260, BL_BTN_H = 50;
static const int MN_X[3] = {26, 174, 322}, MN_Y[2] = {114, 268}, MN_W = 132, MN_H = 142;

// ---------------- 小工具 ----------------
static inline void fmt(char *b, int n, const char *f, float v) { snprintf(b, n, f, (double)v); }

static void iconBt(Canvas &c, float x, float y, float s, uint32_t col) {   // 蓝牙符文，s = 半高
  float w = s * 0.55f, t = s * 0.2f;
  c.line(x - w, y - s * 0.45f, x + w, y + s * 0.45f, t, col);
  c.line(x + w, y + s * 0.45f, x, y + s, t, col);
  c.line(x, y + s, x, y - s, t, col);
  c.line(x, y - s, x + w, y - s * 0.45f, t, col);
  c.line(x + w, y - s * 0.45f, x - w, y + s * 0.45f, t, col);
}
static void iconLink(Canvas &c, float x, float y, float s, uint32_t col) {   // 链接（两节链环）
  float d = s * 0.42f;
  c.line(x - d * 1.4f, y + d * 0.2f, x - d * 0.2f, y - d * 1.0f, s * 0.42f, col);
  c.line(x - d * 1.4f, y + d * 0.2f, x - d * 0.2f, y - d * 1.0f, s * 0.16f, BG);
  c.line(x + d * 0.2f, y + d * 1.0f, x + d * 1.4f, y - d * 0.2f, s * 0.42f, col);
  c.line(x + d * 0.2f, y + d * 1.0f, x + d * 1.4f, y - d * 0.2f, s * 0.16f, BG);
  c.line(x - d * 0.45f, y + d * 0.45f, x + d * 0.45f, y - d * 0.45f, s * 0.14f, col);
}
static void iconBattery(Canvas &c, float x, float y, float w, float h, float frac, uint32_t col, uint32_t fill, float t = 2.5f) {
  c.roundRectStroke(x, y, w - h * 0.14f, h, h * 0.22f, t, col);
  c.roundRect(x + w - h * 0.16f, y + h * 0.3f, h * 0.16f, h * 0.4f, h * 0.06f, col);
  float iw = (w - h * 0.14f - 2 * (t + 1.5f));
  if (frac > 0) c.roundRect(x + t + 1.5f, y + t + 1.5f, iw * (frac > 1 ? 1 : frac), h - 2 * (t + 1.5f), h * 0.1f, fill);
}
static void iconSignal(Canvas &c, float x, float y, int rssi, uint32_t on, uint32_t off) {  // 4 格信号，(x,y) = 左下
  int lv = rssi == -999 ? 0 : rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : 1;
  for (int i = 0; i < 4; i++) {
    float h = 7 + i * 5;
    c.roundRect(x + i * 7, y - h, 4.5f, h, 1.5f, i < lv ? on : off);
  }
}
static void iconNote(Canvas &c, float x, float y, float s, uint32_t col) {  // 音符
  c.disc(x - s * 0.35f, y + s * 0.55f, s * 0.32f, col);
  c.line(x - s * 0.06f, y + s * 0.55f, x - s * 0.06f, y - s * 0.7f, s * 0.16f, col);
  c.line(x - s * 0.06f, y - s * 0.7f, x + s * 0.5f, y - s * 0.35f, s * 0.2f, col);
}
static void iconPhone(Canvas &c, float x, float y, float s, uint32_t col, uint32_t bg) {
  c.roundRect(x - s * 0.5f, y - s * 0.85f, s, s * 1.7f, s * 0.2f, col);
  c.roundRect(x - s * 0.38f, y - s * 0.62f, s * 0.76f, s * 1.12f, s * 0.08f, bg);
  c.disc(x, y + s * 0.68f, s * 0.09f, bg);
}
static void iconGear(Canvas &c, float x, float y, float s, uint32_t col, uint32_t bg) {
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    c.line(x + cosf(a) * s * 0.55f, y + sinf(a) * s * 0.55f, x + cosf(a) * s * 0.95f, y + sinf(a) * s * 0.95f, s * 0.32f, col);
  }
  c.disc(x, y, s * 0.72f, col);
  c.disc(x, y, s * 0.3f, bg);
}
static void iconInfo(Canvas &c, float x, float y, float s, uint32_t col, uint32_t bg) {
  c.disc(x, y, s, col);
  c.disc(x, y - s * 0.45f, s * 0.13f, bg);
  c.line(x, y - s * 0.12f, x, y + s * 0.55f, s * 0.22f, bg);
}
static void iconNavArrow(Canvas &c, float x, float y, float s, uint32_t col) {   // 导航箭头（斜上）
  float xs[4] = {x + s * 0.85f, x - s * 0.85f, x - s * 0.05f, x + s * 0.12f};
  float ys[4] = {y - s * 0.85f, y - s * 0.05f, y + s * 0.1f, y + s * 0.9f};
  float ax[3] = {xs[0], xs[1], xs[2]}, ay[3] = {ys[0], ys[1], ys[2]};
  float bx[3] = {xs[0], xs[2], xs[3]}, by[3] = {ys[0], ys[2], ys[3]};
  c.poly(ax, ay, 3, col); c.poly(bx, by, 3, col);
}
static void iconHeart(Canvas &c, float x, float y, float s, uint32_t col) {
  c.disc(x - s * 0.45f, y - s * 0.2f, s * 0.5f, col);
  c.disc(x + s * 0.45f, y - s * 0.2f, s * 0.5f, col);
  c.tri(x - s * 0.93f, y - s * 0.05f, x + s * 0.93f, y - s * 0.05f, x, y + s * 0.95f, col);
}
static void iconList(Canvas &c, float x, float y, uint32_t col) {
  for (int i = 0; i < 3; i++) { c.disc(x - 12, y - 8 + i * 8, 2, col); c.line(x - 5, y - 8 + i * 8, x + 12, y - 8 + i * 8, 3, col); }
}
static void iconBars(Canvas &c, float x, float y, uint32_t col) {
  const int hs[4] = {10, 18, 14, 22};
  for (int i = 0; i < 4; i++) c.roundRect(x - 13 + i * 7, y + 11 - hs[i], 4.5f, hs[i], 1.5f, col);
}

// 文字药丸（标注“模拟数据”“数据过期”等）
static void pill(Canvas &c, int cx, int y, const char *s, uint32_t fg, uint32_t bg) {
  int w = Canvas::textW(sqF16, s) + 20;
  c.roundRect(cx - w / 2, y, w, 22, 11, bg);
  c.text(sqF16, cx, y + 2, s, fg, 1);
}

static bool bmsHas(const State &st, BmsData::Field f) { return st.bms && st.bms->has(f, st.now); }

// ---------------- 顶部状态栏 ----------------
static void statusBar(Canvas &c, const State &st) {
  c.fillRect(0, 0, W, SB_H, BG);
  char b[24];
  if (st.clockMin >= 0) { snprintf(b, sizeof b, "%02d:%02d", (st.clockMin / 60) % 24, st.clockMin % 60); c.text(sqF24, 24, 10, b, TXT); }
  bool soc = bmsHas(st, BmsData::F_SOC);
  float f = soc ? st.bms->soc / 100.0f : 0;
  uint32_t col = !soc ? TXT3 : f < 0.2f ? RED : f < 0.3f ? ORANGE : GREEN;
  iconBattery(c, 180, 13, 38, 20, f, col, col, 2.4f);
  if (soc) fmt(b, sizeof b, "%.1f%%", st.bms->soc); else snprintf(b, sizeof b, "--%%");
  int x = c.text(sqF24, 226, 10, b, soc ? TXT : TXT2);
  if (soc && st.bms->src == BmsData::SIM) c.text(sqF16, x + 4, 13, "模拟", ORANGE);
  iconBt(c, 390, 23, 13, st.phoneConn ? BLUE : TXT3);
  iconLink(c, 438, 23, 21, st.link.state >= 3 ? BLUE : TXT3);
}
static bool hitStatusSoc(int x, int y) { return y < SB_H + 8 && x >= SB_SOC_X0 && x <= SB_SOC_X1; }

// ---------------- 电池主页 ----------------
static void pageBattery(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  char b[32];
  bool soc = bmsHas(st, BmsData::F_SOC);
  float f = soc ? st.bms->soc / 100.0f : 0;
  c.arc(G_CX, G_CY, G_R, G_T, G_A0, G_A0 + G_SWEEP, TRACK);
  if (soc && f > 0.003f) c.arc(G_CX, G_CY, G_R, G_T, G_A0, G_A0 + G_SWEEP * (f > 1 ? 1 : f), f < 0.2f ? RED : GREEN);
  {   // 圆弧末端的小蓝点（效果图）
    float a = (G_A0 + G_SWEEP) / 57.29578f, rm = G_R - G_T / 2.0f;
    c.disc(G_CX + rm * cosf(a), G_CY + rm * sinf(a), 5, BLUE);
  }
  c.text(sqF24, 240, 100, "SOC", TXT2, 1);
  if (soc) fmt(b, sizeof b, "%.1f", st.bms->soc); else snprintf(b, sizeof b, "--");
  int w1 = Canvas::textW(sqF96, b), w2 = Canvas::textW(sqF32, "%");
  int x0 = 240 - (w1 + w2 + 2) / 2, y0 = 128;
  c.text(sqF96, x0, y0, b, soc ? TXT : TXT2);
  c.text(sqF32, x0 + w1 + 2, y0 + sqF96.asc - sqF32.asc, "%", soc ? TXT : TXT2);
  iconBattery(c, 186, 214, 108, 40, f, TXT, f < 0.2f ? RED : GREEN, 3.5f);
  // 数据来源标注
  if (!st.bms || st.bms->src == BmsData::NONE) pill(c, 240, 264, st.link.state >= 3 ? "已连接 · 协议待适配" : "未连接电池 BMS", TXT2, CARD);
  else if (st.bms->expired(st.now)) pill(c, 240, 264, "数据过期", TXT, RED);
  else if (st.bms->src == BmsData::SIM) pill(c, 240, 264, "模拟数据 SIMULATED", ORANGE, 0x3A2408);

  const char *lab[6] = {"总电压", "剩余容量", "电流", "电池温度", "健康度", "BMS状态"};
  for (int i = 0; i < 6; i++) {
    int x = CARD_X[i % 3], y = CARD_Y[i / 3];
    c.roundRect(x, y, CARD_W, CARD_H, 14, CARD);
    uint32_t vc = TXT;
    bool ok = false;
    switch (i) {
      case 0: if ((ok = bmsHas(st, BmsData::F_VOLT))) fmt(b, sizeof b, "%.2f V", st.bms->volt); break;
      case 1: if ((ok = bmsHas(st, BmsData::F_REMAIN))) fmt(b, sizeof b, "%.2f Ah", st.bms->remainAh); break;
      case 2: if ((ok = bmsHas(st, BmsData::F_CURR))) fmt(b, sizeof b, "%.1f A", st.bms->curr); break;
      case 3: if ((ok = bmsHas(st, BmsData::F_TEMP))) fmt(b, sizeof b, "%.0f°C", st.bms->tempC); break;
      case 4: if ((ok = bmsHas(st, BmsData::F_SOH))) fmt(b, sizeof b, "%.0f%%", st.bms->soh); break;
      case 5: if ((ok = bmsHas(st, BmsData::F_ALARM))) { snprintf(b, sizeof b, st.bms->alarms ? "告警" : "正常"); vc = st.bms->alarms ? RED : GREEN; } break;
    }
    if (!ok) { snprintf(b, sizeof b, "--"); vc = TXT2; }
    int cx = x + CARD_W / 2;
    if (i == 4 && ok) {
      int tw = Canvas::textW(sqF24, b);
      iconHeart(c, cx - tw / 2 - 8, y + 27, 10, GREEN);
      c.text(sqF24, cx + 12, y + 13, b, vc, 1);
    } else c.text(sqF24, cx, y + 13, b, vc, 1);
    c.text(sqF20, cx, y + 48, lab[i], TXT2, 1);
  }
}
// 电池页：点圆弧/SOC 区域进入电芯页
static bool hitBatteryCells(int x, int y) { (void)x; return y > SB_H && y < 290; }

// ---------------- 24 串电芯 ----------------
// 柱状图用固定的磷酸铁锂量程 2.50~3.65V，不放大毫伏级差异（避免夸张）；差异看下方“压差”。
static const int CELL_MV_LO = 2500, CELL_MV_HI = 3650;
static int cellScrollMax(const State &st) {
  int n = st.bms ? st.bms->cellN : 0; if (n <= 0) n = BmsData::CELLS;
  int m = n * CL_RH - CL_ROWS * CL_RH; return m > 0 ? m : 0;
}
static void pageCells(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  char b[32];
  bool ok = bmsHas(st, BmsData::F_CELLS);
  int n = ok ? st.bms->cellN : BmsData::CELLS;
  BmsData::CellStats cs; if (ok) cs = st.bms->cellStats();
  // 标签栏
  c.roundRect(20, CL_TAB_Y, 440, CL_TAB_H, 14, CARD);
  c.roundRect(24, CL_TAB_Y + 4, 232, CL_TAB_H - 8, 11, 0x1D4FB8);
  snprintf(b, sizeof b, "电芯电压 (%dS)", n);
  c.text(sqF24, 140, CL_TAB_Y + 9, b, TXT, 1);
  c.roundRect(366, CL_TAB_Y + 6, 88, CL_TAB_H - 12, 10, CARD_HI);
  if (st.cellMode == 0) iconBars(c, 410, CL_TAB_Y + 23, TXT); else iconList(c, 410, CL_TAB_Y + 23, TXT);
  if (st.bms && st.bms->src == BmsData::SIM) c.text(sqF16, 312, CL_TAB_Y + 13, "模拟", ORANGE, 1);
  else if (st.bms && st.bms->expired(st.now)) c.text(sqF16, 312, CL_TAB_Y + 13, "过期", RED, 1);

  if (st.cellMode == 0) {
    int sc = st.cellScroll; if (sc < 0) sc = 0; if (sc > cellScrollMax(st)) sc = cellScrollMax(st);
    c.clipY0 = CL_Y0; c.clipY1 = CL_Y0 + CL_ROWS * CL_RH;
    for (int i = 0; i < n; i++) {
      int y = CL_Y0 + i * CL_RH - sc;
      if (y + CL_RH < CL_Y0 || y > c.clipY1) continue;
      bool isMax = ok && i == cs.maxI, isMin = ok && i == cs.minI && cs.deltaMv > 0;
      c.roundRect(20, y + 3, 436, CL_RH - 6, 8, isMax ? CARD_SEL : 0x0B1220);
      uint32_t tc = isMax ? 0x3D8BFF : isMin ? YELLOW : TXT;
      snprintf(b, sizeof b, "%02d", i + 1);
      c.text(sqF24, 40, y + 6, b, tc);
      uint16_t mv = ok ? st.bms->cellMv[i] : 0;
      if (mv) snprintf(b, sizeof b, "%d.%03d V", mv / 1000, mv % 1000); else snprintf(b, sizeof b, "--");
      c.text(sqF24, 132, y + 6, b, mv ? tc : TXT2);
      c.roundRect(300, y + 10, 140, CL_RH - 20, 4, 0x18233A);
      if (mv) {
        float fr = (float)(mv - CELL_MV_LO) / (CELL_MV_HI - CELL_MV_LO); fr = fr < 0.02f ? 0.02f : fr > 1 ? 1 : fr;
        c.roundRect(300, y + 10, 140 * fr, CL_RH - 20, 4, isMax ? 0x3D8BFF : isMin ? YELLOW : 0x2BE07A);
      }
    }
    c.clipY0 = 0; c.clipY1 = H;
    // 滚动条
    int tot = n * CL_RH, vis = CL_ROWS * CL_RH;
    if (tot > vis) {
      int th = vis * vis / tot, ty = CL_Y0 + (vis - th) * sc / (tot - vis);
      c.roundRect(462, CL_Y0, 4, vis, 2, 0x18233A);
      c.roundRect(462, ty, 4, th, 2, TXT2);
    }
  } else {
    // 柱状图：24 根柱子，固定量程
    int x0 = 70, x1 = 456, y0 = 118, y1 = 330;
    for (int k = 0; k <= 3; k++) {
      int mv = 2500 + k * 375 + (k == 3 ? 25 : 0);    // 2.500 2.875 3.250 3.650
      float fy = y1 - (float)(mv - CELL_MV_LO) / (CELL_MV_HI - CELL_MV_LO) * (y1 - y0);
      c.fillRect(x0, (int)fy, x1 - x0, 1, 0x1A2538);
      snprintf(b, sizeof b, "%d.%02d", mv / 1000, (mv % 1000) / 10);
      c.text(sqF16, 62, (int)fy - 10, b, TXT3, 2);
    }
    float bw = (float)(x1 - x0) / n;
    for (int i = 0; i < n; i++) {
      uint16_t mv = ok ? st.bms->cellMv[i] : 0;
      float bx = x0 + i * bw + bw * 0.18f;
      bool isMax = ok && i == cs.maxI, isMin = ok && i == cs.minI && cs.deltaMv > 0;
      if (mv) {
        float fr = (float)(mv - CELL_MV_LO) / (CELL_MV_HI - CELL_MV_LO); fr = fr < 0.01f ? 0.01f : fr > 1 ? 1 : fr;
        c.roundRect(bx, y1 - fr * (y1 - y0), bw * 0.64f, fr * (y1 - y0), 3, isMax ? 0x3D8BFF : isMin ? YELLOW : 0x2BE07A);
      }
      if (i % 4 == 0 || i == n - 1) { snprintf(b, sizeof b, "%d", i + 1); c.text(sqF16, (int)(bx + bw * 0.32f), y1 + 6, b, TXT3, 1); }
    }
    c.text(sqF16, 456, y0 - 22, "量程 2.50~3.65V（不放大差异）", TXT3, 2);
  }
  // 统计
  c.roundRect(20, CS_Y, 440, 104, 14, CARD);
  const char *lab[3] = {"最高", "最低", "压差"};
  for (int k = 0; k < 3; k++) {
    int x = 44 + k * 150;
    c.text(sqF20, x, CS_Y + 10, lab[k], TXT2);
    uint32_t vc = GREEN;
    if (!ok || cs.maxI < 0) { snprintf(b, sizeof b, "--"); vc = TXT2; }
    else if (k == 0) snprintf(b, sizeof b, "%d.%03d V", cs.maxMv / 1000, cs.maxMv % 1000);
    else if (k == 1) snprintf(b, sizeof b, "%d.%03d V", cs.minMv / 1000, cs.minMv % 1000);
    else { snprintf(b, sizeof b, "%d mV", cs.deltaMv); vc = cs.deltaMv > 100 ? RED : cs.deltaMv > 50 ? ORANGE : GREEN; }
    c.text(sqF24, x, CS_Y + 38, b, vc);
    if (ok && cs.maxI >= 0 && k < 2) { snprintf(b, sizeof b, "#%02d", (k == 0 ? cs.maxI : cs.minI) + 1); c.text(sqF16, x, CS_Y + 72, b, TXT2); }
  }
}
static bool hitCellsMode(int x, int y) { return y >= CL_TAB_Y && y <= CL_TAB_Y + CL_TAB_H && x >= 356; }

// ---------------- 音乐 ----------------
static void pageMusic(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  // 封面：手机协议没有专辑图，画一个装饰性的渐变占位（不是真实封面）
  int cx0 = 146, cy0 = 58, cw = 188;
  for (int j = 0; j < cw; j++) {
    float t = j / (float)cw;
    uint32_t top = 0x3A2A6E, bot = 0xE0704A;
    int r = ((top >> 16) & 255) + (int)((((int)(bot >> 16) & 255) - ((int)(top >> 16) & 255)) * t);
    int g = ((top >> 8) & 255) + (int)((((int)(bot >> 8) & 255) - ((int)(top >> 8) & 255)) * t);
    int bl = (top & 255) + (int)((((int)bot & 255) - ((int)top & 255)) * t);
    c.clipY0 = cy0 + j; c.clipY1 = cy0 + j + 1;
    c.roundRect(cx0, cy0, cw, cw, 18, ((uint32_t)r << 16) | (g << 8) | bl);
  }
  c.clipY0 = 0; c.clipY1 = H;
  iconNote(c, 240, 150, 46, 0xFFFFFF);
  c.disc(416, 150, 30, CARD);
  iconNote(c, 416, 150, 16, 0x7FB2FF);
  bool t5 = st.musicLinked && st.slotAvail && st.slotAvail(5) && st.slot;
  bool t6 = st.musicLinked && st.slotAvail && st.slotAvail(6) && st.slot;
  if (t5) st.slot(5, 240, 262, 420, 1, 0xFF);
  else c.text(sqF24, 240, 262, st.musicLinked ? (st.musicHave ? "" : "未在播放") : "手机未连接", st.musicLinked ? TXT : TXT2, 1);
  if (t6) st.slot(6, 240, 298, 420, 1, 0xBB);
  // 进度（只有手机提供时长时才显示）
  char b[16];
  if (st.musicTotal > 0 && st.musicElapsed >= 0) {
    float f = (float)st.musicElapsed / st.musicTotal; f = f > 1 ? 1 : f;
    c.roundRect(40, 336, 400, 6, 3, TRACK);
    c.roundRect(40, 336, 400 * f, 6, 3, BLUE);
    c.disc(40 + 400 * f, 339, 7, 0x9CC4FF);
    snprintf(b, sizeof b, "%d:%02d", st.musicElapsed / 60, st.musicElapsed % 60); c.text(sqF16, 40, 350, b, TXT2);
    snprintf(b, sizeof b, "%d:%02d", st.musicTotal / 60, st.musicTotal % 60); c.text(sqF16, 440, 350, b, TXT2, 2);
  }
  for (int i = 0; i < 3; i++) {
    int x = MU_BTN_X[i], y = MU_BTN_Y;
    bool en = st.musicLinked;
    uint32_t col = en ? TXT : TXT3;
    if (i == 1) c.disc(x, y, MU_BTN_R, st.musicPressed == 1 ? BLUE_D : CARD_HI);
    else if (st.musicPressed == i) c.disc(x, y, 36, BLUE_D);
    if (i == 1) {
      if (st.musicPlaying) { c.roundRect(x - 13, y - 17, 9, 34, 3, col); c.roundRect(x + 4, y - 17, 9, 34, 3, col); }
      else c.tri(x - 10, y - 18, x - 10, y + 18, x + 18, y, col);
    } else {
      int s = i == 0 ? -1 : 1;
      c.tri(x - s * 11, y - 15, x - s * 11, y + 15, x + s * 11, y, col);
      c.roundRect(x + s * 14 - 2.5f, y - 15, 5, 30, 2, col);
    }
  }
  if (st.musicState && *st.musicState) c.text(sqF16, 240, 452, st.musicState, TXT3, 1);
}
static int hitMusic(int x, int y) {
  for (int i = 0; i < 3; i++) {
    int dx = x - MU_BTN_X[i], dy = y - MU_BTN_Y;
    if (dx * dx + dy * dy <= (MU_BTN_R + 6) * (MU_BTN_R + 6)) return i;
  }
  return -1;
}

// ---------------- 蓝牙设备 ----------------
static const char *linkText(int s) {
  switch (s) {
    case 1: return "未连接 · 自动重连中";
    case 2: return "连接中…";
    case 3: return "已连接 · 读取服务中";
    case 4: return "已连接 · 已订阅通知";
    case 5: return "连接失败 · 稍后重试";
    default: return "未选择 · 点下方设备连接";
  }
}
static void bleRow(Canvas &c, int y, bool hi, int icon, const char *name, const char *sub, int rssi) {
  c.roundRect(20, y, 440, BL_ROW_H, 14, hi ? 0x1A6EF0 : CARD);
  uint32_t ic = hi ? TXT : 0xC9D3E3, bg = hi ? 0x1A6EF0 : CARD;
  if (icon == 0) iconBattery(c, 38, y + 24, 38, 20, 0.6f, ic, ic, 2.4f);
  else if (icon == 1) iconPhone(c, 58, y + 33, 18, ic, bg);
  else iconBt(c, 58, y + 33, 14, ic);
  c.text(sqF24, 96, y + 6, name, TXT);
  c.text(sqF20, 96, y + 37, sub, hi ? 0xD6E4FF : TXT2);
  if (rssi != -999) iconSignal(c, 408, y + 46, rssi, hi ? 0x5CFF9A : GREEN, hi ? 0x4A86F2 : TXT3);
}
static void pageBle(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  c.roundRect(20, 54, 440, 44, 12, CARD);
  c.text(sqF24, 240, 62, "蓝牙设备", TXT, 1);
  char sub[64], name[40];
  // 第 1 行：电池 BMS（只读调试连接）
  const LinkInfo &L = st.link;
  snprintf(name, sizeof name, "%s", L.state ? (L.name[0] ? L.name : L.addr) : "电池 BMS");
  if (L.state >= 3) {
    uint32_t ago = L.rx ? (st.now - L.lastRxMs) / 1000 : 0;
    if (L.rx) snprintf(sub, sizeof sub, "%s  收%lu包 %lus前", L.state == 4 ? "已订阅" : "已连接", (unsigned long)L.rx, (unsigned long)ago);
    else snprintf(sub, sizeof sub, "%s  服务%d 订阅%d", L.state == 4 ? "已订阅" : "已连接", L.services, L.subs);
  } else snprintf(sub, sizeof sub, "%s", linkText(L.state));
  bleRow(c, BL_ROW_Y0, L.state >= 3, 0, name, sub, L.state >= 3 ? L.rssi : -999);
  // 第 2 行：手机（MotoNav 导航/音乐）
  snprintf(sub, sizeof sub, "%s", st.phoneConn ? (st.phoneAuthed ? "已连接 · 已验证" : "已连接 · 等待验证") : "未连接");
  bleRow(c, BL_ROW_Y0 + BL_ROW_H + BL_ROW_GAP, false, 1, "手机 (MotoNav)", sub, st.phoneConn ? st.phoneRssi : -999);
  // 第 3、4 行：扫描结果
  for (int r = 0; r < 2; r++) {
    int i = st.bleScroll + r, y = BL_ROW_Y0 + (r + 2) * (BL_ROW_H + BL_ROW_GAP);
    if (i >= st.scanN) {
      if (r == 0) c.text(sqF16, 240, y + 24, st.scanning ? "扫描中…" : "点“扫描设备”查找附近的电池", TXT3, 1);
      break;
    }
    const ScanItem &it = st.scan[i];
    snprintf(sub, sizeof sub, "RSSI %d  %s", it.rssi, it.addr);
    bleRow(c, y, false, 2, it.name[0] ? it.name : "(无名称)", sub, it.rssi);
  }
  if (st.scanN > 2) {
    char b[24]; snprintf(b, sizeof b, "%d/%d", st.bleScroll + 1, st.scanN);
    c.text(sqF16, 456, BL_BTN_Y + 16, b, TXT3, 2);
  }
  c.roundRect(BL_BTN_X, BL_BTN_Y, BL_BTN_W, BL_BTN_H, BL_BTN_H / 2, st.scanning ? 0x123063 : 0x0F3E9C);
  c.text(sqF24, 240, BL_BTN_Y + 12, st.scanning ? "扫描中…" : "扫描设备", TXT, 1);
}
// 返回：-1 无，0 电池行，1 手机行，10+i 扫描结果 i，100 扫描按钮
static int hitBle(const State &st, int x, int y) {
  if (x < 20 || x > 460) return -1;
  if (y >= BL_BTN_Y - 4 && y <= BL_BTN_Y + BL_BTN_H + 6 && x >= BL_BTN_X - 10 && x <= BL_BTN_X + BL_BTN_W + 10) return 100;
  for (int r = 0; r < BL_ROWS; r++) {
    int y0 = BL_ROW_Y0 + r * (BL_ROW_H + BL_ROW_GAP);
    if (y >= y0 && y < y0 + BL_ROW_H) {
      if (r < 2) return r;
      int i = st.bleScroll + r - 2;
      return i < st.scanN ? 10 + i : -1;
    }
  }
  return -1;
}

// ---------------- 功能菜单 ----------------
static void pageMenu(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  c.text(sqF24, 240, 64, "功能菜单", TXT, 1);
  const char *lab[6] = {"导航", "音乐", "电池", "蓝牙", "设置", "关于"};
  const uint32_t bg[6] = {0x1E6FF0, 0x3A1622, 0x0E3A25, CARD, CARD, CARD};
  for (int i = 0; i < 6; i++) {
    int x = MN_X[i % 3], y = MN_Y[i / 3], cx = x + MN_W / 2, cy = y + 58;
    c.roundRect(x, y, MN_W, MN_H, 18, bg[i]);
    switch (i) {
      case 0: iconNavArrow(c, cx, cy, 30, TXT); break;
      case 1: iconNote(c, cx + 5, cy, 36, 0xFF3B5C); break;
      case 2: iconBattery(c, cx - 32, cy - 19, 66, 38, 0.75f, GREEN, GREEN, 4.5f); break;
      case 3: iconBt(c, cx, cy, 28, BLUE); break;
      case 4: iconGear(c, cx, cy, 30, TXT, bg[i]); break;
      case 5: iconInfo(c, cx, cy, 28, TXT, bg[i]); break;
    }
    c.text(sqF24, cx, y + 100, lab[i], TXT, 1);
  }
  c.text(sqF16, 240, 440, "左右滑动返回导航", TXT3, 1);
}
static int hitMenu(int x, int y) {
  for (int i = 0; i < 6; i++) {
    int x0 = MN_X[i % 3], y0 = MN_Y[i / 3];
    if (x >= x0 - 6 && x <= x0 + MN_W + 6 && y >= y0 - 6 && y <= y0 + MN_H + 6) return i;
  }
  return -1;
}
static const int MENU_TARGET[6] = {P_NAV, P_MUSIC, P_BATTERY, P_BLE, P_SETTINGS, P_ABOUT};

// ---------------- 设置 / 关于 ----------------
static void infoRow(Canvas &c, int y, const char *k, const char *v, uint32_t vc = TXT2) {
  c.roundRect(20, y, 440, 52, 12, CARD);
  c.text(sqF20, 40, y + 13, k, TXT);
  c.text(sqF20, 440, y + 13, v, vc, 2);
}
static void pageSettings(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  c.text(sqF24, 240, 60, "设置", TXT, 1);
  infoRow(c, 104, "电池模拟数据", st.simOn ? "开（点此关闭）" : "关（点此开启）", st.simOn ? ORANGE : TXT2);
  infoRow(c, 164, "电池 BMS 连接", st.link.state ? "点此断开并忘记" : "未选择", st.link.state ? BLUE : TXT2);
  c.text(sqF16, 240, 236, "亮度、方向、地图风格等请在手机 App 中设置", TXT2, 1);
  c.text(sqF16, 240, 262, "BMS 功能默认只读，不会向电池发送控制命令", TXT3, 1);
}
static int hitSettings(int x, int y) { (void)x; return (y >= 104 && y < 156) ? 0 : (y >= 164 && y < 216) ? 1 : -1; }

static void pageAbout(Canvas &c, const State &st) {
  c.clear(BG);
  statusBar(c, st);
  c.text(sqF24, 240, 60, "关于", TXT, 1);
  char b[32];
  infoRow(c, 104, "固件", st.fw);
  infoRow(c, 164, "蓝牙名称", st.bleName);
  if (st.boardBatPct >= 0) snprintf(b, sizeof b, "%d%%", st.boardBatPct); else snprintf(b, sizeof b, "未接电池");
  infoRow(c, 224, "主板电池", b);
  infoRow(c, 284, "BMS 协议", "未验证（只读调试）", ORANGE);
  c.text(sqF16, 240, 356, "MotoNav × BYD 7645 · 480×480", TXT3, 1);
}

// ---------------- 导航（箭头模式，方屏卡片布局） ----------------
static void pageNav(Canvas &c, const State &st, const char *distNum, const char *distUnit) {
  c.clear(BG);
  statusBar(c, st);
  char b[24];
  // 上方转向卡片
  c.roundRect(14, 52, 452, 128, 20, 0x0E1626);
  if (st.turnIcon) st.turnIcon(84, 64, 168, 112, st.nearTurn);
  int x = c.text(sqF64, 160, 58, distNum, st.nearTurn ? ORANGE : TXT);
  c.text(sqF32, x + 6, 58 + sqF64.asc - sqF32.asc, distUnit, st.nearTurn ? ORANGE : TXT);
  if (st.slotAvail && st.slotAvail(0) && st.slot) st.slot(0, 162, 136, 290, 0, 0xEE);
  // 中间：车道
  if (st.haveLanes && st.lanes) st.lanes(240, 204, 420, 44);
  // 右侧：剩余里程
  if (st.tripOn) {
    c.roundRect(312, 262, 152, 84, 16, 0x0E1626);
    if (st.tripRemainM >= 1000) fmt(b, sizeof b, "%.1f km", st.tripRemainM / 1000.0f); else snprintf(b, sizeof b, "%d m", st.tripRemainM);
    c.text(sqF24, 388, 274, b, TXT, 1);
    c.text(sqF16, 388, 310, "剩余", TXT2, 1);
  }
  // 下方：车速、限速、到达时间
  c.roundRect(14, 356, 452, 112, 20, 0x0E1626);
  if (st.speed >= 0) {
    snprintf(b, sizeof b, "%d", st.speed);
    int sx = c.text(sqF64, 34, 364, b, st.hasCam && st.camLimit > 0 && st.speed > st.camLimit ? RED : TXT);
    c.text(sqF20, sx + 6, 364 + sqF64.asc - sqF20.asc, "km/h", TXT2);
  } else c.text(sqF20, 34, 392, "-- km/h", TXT3);
  if (st.hasCam && st.camLimit > 0) {
    c.disc(232, 402, 32, TXT); c.ring(232, 402, 32, 7, 0xE02020);
    snprintf(b, sizeof b, "%d", st.camLimit);
    c.text(sqF24, 232, 389, b, 0x000000, 1);
  }
  if (st.tripOn && st.tripEtaHHMM >= 0) {
    snprintf(b, sizeof b, "%02d:%02d", (st.tripEtaHHMM / 100) % 24, st.tripEtaHHMM % 100);
    c.text(sqF32, 446, 366, b, TXT, 2);
    c.text(sqF16, 446, 406, "到达", TXT2, 2);
  }
  if (st.tripFrac >= 0) {
    float f = st.tripFrac > 1 ? 1 : st.tripFrac;
    c.roundRect(34, 446, 412, 6, 3, 0x2A3550);
    c.roundRect(34, 446, 412 * f, 6, 3, BLUE);
    c.disc(34 + 412 * f, 449, 7, TXT);
  }
}

}  // namespace sq

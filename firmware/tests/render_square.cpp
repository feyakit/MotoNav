// 桌面“模拟预览”渲染器：与固件共用 SqPages.h / SqGfx.h / SqFont.h / BmsData.h，
// 用模拟数据把方屏页面画成 480x480 PPM（由 render_square.py 转 PNG 并加 SIMULATED 标注）。
// 这不是实机截图；手机发来的文字图片、矢量转向图标、车道在这里用替身绘制。
// g++ -std=c++17 -O2 -I firmware/boards/216/MotoNav firmware/tests/render_square.cpp -o /tmp/render_square && /tmp/render_square out_dir
#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>
#include "SqPages.h"

static uint16_t fb[480 * 480];
static sq::Canvas *gc;

static void save(const std::string &path) {
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 480 480 255\n");
  for (int i = 0; i < 480 * 480; i++) {
    uint16_t c = fb[i];
    unsigned char p[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
    fwrite(p, 1, 3, f);
  }
  fclose(f);
}
static bool slotAvail(int) { return true; }
static void slotStub(int slot, int x, int y, int maxW, int align, uint8_t tint) {
  (void)maxW;
  const char *t = slot == 0 ? "进入 科技园路" : slot == 5 ? "APT." : "ROSÉ & Bruno Mars";
  gc->text(slot == 6 ? sqF20 : sqF24, x, y, t, tint > 0xC0 ? 0xFFFFFF : 0xBBBBBB, align);
}
static void turnStub(float cx, float top, float bottom, float maxW, bool near) {
  (void)maxW;
  uint32_t col = near ? sq::ORANGE : 0xFFFFFF;
  float h = bottom - top, x = cx - h * 0.18f, y1 = bottom - 8, y0 = top + h * 0.38f;
  gc->line(x, y1, x, y0 + 10, 14, col);
  gc->arc(x + 22, y0 + 10, 29, 14, 180, 270, col, false);
  gc->line(x + 22, y0 - 12, x + 40, y0 - 12, 14, col);
  gc->tri(x + 38, y0 - 32, x + 38, y0 + 8, x + 64, y0 - 12, col);
}


// ---- 地图模式模拟：合成“图片地图”（索引色：0 街区 1 道路 2 绿地 3 水面 4 屋顶 5 屋顶轮廓），
//      按固件 rasterRows 的同一套旋转公式铺满 480x480，再叠路线、车头和 sq::mapOverlay。
//      颜色 = 固件主题 1（罪恶都市，默认）；楼块色 = blendF(街区, 道路, 64/112)。
static uint16_t blend565(uint16_t a, uint16_t b, int t) {   // t: 0~256
  int r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8, g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8,
      bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}
static int worldCls(float wx, float wy) {           // 米，y 朝北
  if (wx > 150 && wx < 205) return 3;                // 河
  auto md = [](float v, float p) { float m = fmodf(v, p); return m < 0 ? m + p : m; };
  float bx = md(wx + 7, 110), by = md(wy + 7, 95);   // 街区网格：路宽 14 m
  if (bx < 14 || by < 14) return 1;
  if (md(wx * 0.37f + wy * 0.11f, 260) < 10 && wx < 150) return 1;   // 一条斜向小路
  int ix = (int)floorf((wx + 7) / 110), iy = (int)floorf((wy + 7) / 95);
  if (ix == -2 && iy == 1) return 2;                 // 公园
  // 每个街区里 2x2 栋楼，退界 6 m，楼间 8 m，轮廓 2 m
  float lx = bx - 14, ly = by - 14, cw = (110 - 14) / 2.0f, ch = (95 - 14) / 2.0f;
  float qx = fmodf(lx, cw), qy = fmodf(ly, ch);
  unsigned h = (unsigned)ix * 73856093u ^ (unsigned)iy * 19349663u ^ ((unsigned)(int)(lx / cw) * 83492791u + (unsigned)(int)(ly / ch) * 2654435761u);
  if (h % 7 == 0) return 0;                          // 空地
  float in0 = 5 + (h % 3) * 2, in1 = 4 + ((h >> 3) % 3) * 2;
  if (qx < in0 || qx > cw - in1 || qy < in1 || qy > ch - in0) return 0;
  if (qx < in0 + 2 || qx > cw - in1 - 2 || qy < in1 + 2 || qy > ch - in0 - 2) return 5;
  return 4;
}
static void renderMap(sq::Canvas &c, const sq::State &st, float hdgDeg, float sc, const char *dn, const char *du) {
  const uint16_t BLOCK = 0xE6D9, ROAD = 0x3187, PARK = 0x760D, WATER = 0x4E3B, ROUTE = 0xF258, ROUTE2 = 0x78EC;
  uint16_t pal[6] = {BLOCK, ROAD, PARK, WATER, blend565(BLOCK, ROAD, 64), blend565(BLOCK, ROAD, 112)};
  const float CX = 240, PY = sq::MAPQ_PY, MPP = 2.0f;           // 图片地图每像素 2 米
  float h = hdgDeg * 3.14159265f / 180, cs = cosf(h), sn = sinf(h), inv = 1 / sc;
  for (int y = 0; y < 480; y++) for (int x = 0; x < 480; x++) {
    float rx = (x + 0.5f - CX) * inv, ry = (PY - (y + 0.5f)) * inv;
    float wx = rx * cs + ry * sn, wy = -rx * sn + ry * cs;
    wx = floorf(wx / MPP) * MPP; wy = floorf(wy / MPP) * MPP;   // 量化成 2 m 像素
    fb[y * 480 + x] = pal[worldCls(wx, wy)];
  }
  // 路线：车所在南北路 x=0 向北 → y=183 右转向东（世界坐标 → 屏幕，和固件同公式）
  auto scr = [&](float wx, float wy, float &sx, float &sy) { sx = CX + sc * (wx * cs - wy * sn); sy = PY - sc * (wx * sn + wy * cs); };
  float P[4][2] = {{0, -60}, {0, 190}, {440, 190}, {440, 600}};
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < 3; i++) {
      float x0, y0, x1, y1; scr(P[i][0], P[i][1], x0, y0); scr(P[i + 1][0], P[i + 1][1], x1, y1);
      uint16_t col = pass ? ROUTE : ROUTE2; float w = pass ? 9 * sc / 0.6f : 13 * sc / 0.6f;
      auto hex = [](uint16_t v) { return (uint32_t)(((v >> 11) << 19) | (((v >> 5) & 63) << 10) | ((v & 31) << 3)); };
      if (i == 0 && pass) { float mx, my; scr(0, 0, mx, my); c.line(x0, y0, mx, my, w, hex(0xA4B4)); c.line(mx, my, x1, y1, w, hex(col)); }
      else c.line(x0, y0, x1, y1, w, hex(col));
    }
  // 车头（白色三角 + 黑描边，同 drawPlayer 形状）
  const float T[4][2] = {{0, -13}, {-10, 10.5f}, {0, 5}, {10, 10.5f}};
  for (int pass = 0; pass < 2; pass++) {
    float k = pass ? 1.0f : 1.35f; uint32_t col = pass ? 0xFFFFFF : 0x000000;
    c.tri(CX + T[0][0] * k, PY + T[0][1] * k, CX + T[1][0] * k, PY + T[1][1] * k, CX + T[2][0] * k, PY + T[2][1] * k, col);
    c.tri(CX + T[0][0] * k, PY + T[0][1] * k, CX + T[3][0] * k, PY + T[3][1] * k, CX + T[2][0] * k, PY + T[2][1] * k, col);
  }
  sq::mapNorth(c, hdgDeg);
  sq::mapOverlay(c, st, dn, du, true);
  // 半透明状态栏（同固件 sqDrawStatus(false)）
  static uint16_t band[480 * sq::SB_H];
  for (int y = 0; y < sq::SB_H; y++) for (int x = 0; x < 480; x++) c.blend(x, y, 0, 170);
  memcpy(band, fb, sizeof band);
  sq::statusBar(c, st);
  for (int i = 0; i < 480 * sq::SB_H; i++) if (fb[i] == 0) fb[i] = band[i];
}
static void lanesStub(float cx, float y, float maxW, float h) {
  (void)maxW;
  for (int i = 0; i < 4; i++) {
    float x = cx - 54 + i * 36; uint32_t col = i == 3 ? 0xFFFFFF : 0x5A6478;
    gc->line(x, y + h - 2, x, y + 8, 5, col);
    gc->tri(x - 7, y + 10, x + 7, y + 10, x, y + 1, col);
  }
}

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  sq::Canvas c(fb); gc = &c;
  BmsData bms; bms.loadSimulation(1000);
  // 数据模型自检
  auto cs = bms.cellStats();
  assert(cs.maxI == 2 && cs.minI == 23 && cs.deltaMv == 2);
  { BmsData live = bms; live.src = BmsData::LIVE; live.updatedMs = 1000; assert(live.has(BmsData::F_SOC, 5000)); assert(!live.has(BmsData::F_SOC, 1000 + 16000)); assert(live.expired(1000 + 16000)); }
  { BmsData none; assert(!none.has(BmsData::F_SOC, 0)); }
  sq::ScanItem scan[3] = {{"BYD_BMS_7645", "A4:C1:38:12:34:56", -62}, {"Other Device", "5C:F3:70:AA:BB:CC", -75}, {"", "11:22:33:44:55:66", -88}};
  sq::State st;
  st.now = 5000; st.clockMin = 18 * 60 + 24; st.phoneConn = true; st.phoneAuthed = true; st.phoneRssi = -58;
  st.bms = &bms; st.simOn = true; st.fw = "v7.12.1-216-linktop"; st.bleName = "MotoNav-3A7F"; st.boardBatPct = 86;
  st.link.state = 4; strcpy(st.link.name, "BYD_BMS_7645"); strcpy(st.link.addr, "A4:C1:38:12:34:56"); st.link.rssi = -62;
  st.link.services = 4; st.link.subs = 1; st.link.rx = 0;
  st.scan = scan; st.scanN = 3;
  st.inNav = true; st.navDist = 268; st.speed = 36; st.hasCam = true; st.camLimit = 60; st.tripOn = true;
  st.tripRemainM = 12400; st.tripEtaHHMM = 1852; st.tripFrac = 0.42f;
  st.musicLinked = st.musicHave = st.musicPlaying = true; st.musicElapsed = 85; st.musicTotal = 169; st.musicState = "PLAYING";
  st.slotAvail = slotAvail; st.slot = slotStub; st.turnIcon = turnStub;
  sq::pageNav(c, st, "268", "m"); save(out + "/nav.ppm");
  sq::pageBattery(c, st); save(out + "/battery.ppm");
  sq::pageCells(c, st); save(out + "/cells.ppm");
  st.cellMode = 1; sq::pageCells(c, st); save(out + "/cells_bars.ppm"); st.cellMode = 0;
  sq::pageMusic(c, st); save(out + "/music.ppm");
  sq::pageBle(c, st); save(out + "/ble.ppm");
  sq::pageMenu(c, st); save(out + "/menu.ppm");
  sq::pageSettings(c, st); save(out + "/settings.ppm");
  sq::pageAbout(c, st); save(out + "/about.ppm");
  // 地图模式（合成地图 + 楼块 + 路线）
  { sq::State m = st; m.navDist = 183; m.nearTurn = false;
    renderMap(c, m, 12, 0.6f, "183", "m"); save(out + "/map_square.ppm");
    m.navDist = 60; m.nearTurn = true; m.speed = 28; m.haveLanes = true; m.lanes = lanesStub;
    renderMap(c, m, 0, 0.6f, "60", "m"); save(out + "/map_square_near.ppm"); }
  // 无数据状态：未连接 BMS => 全部 "--"
  BmsData empty; st.bms = &empty; st.link = sq::LinkInfo(); st.simOn = false;
  sq::pageBattery(c, st); save(out + "/battery_nodata.ppm");
  printf("rendered to %s\n", out.c_str());
  return 0;
}

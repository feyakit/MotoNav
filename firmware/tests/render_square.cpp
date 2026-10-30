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
  // 无数据状态：未连接 BMS => 全部 "--"
  BmsData empty; st.bms = &empty; st.link = sq::LinkInfo(); st.simOn = false;
  sq::pageBattery(c, st); save(out + "/battery_nodata.ppm");
  printf("rendered to %s\n", out.c_str());
  return 0;
}

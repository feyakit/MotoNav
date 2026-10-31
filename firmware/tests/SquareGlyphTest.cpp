#include "../boards/216/MotoNav/SquareGlyphs.h"
#include "../boards/216/MotoNav/SquareNumeric.h"
#include "../boards/216/MotoNav/SquareSmall.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>
struct Canvas { int runs=0, minx=10000, maxx=-1; void fillRect(int x,int,int w,int,uint16_t) {++runs; if(x<minx) minx=x; if(x+w-1>maxx) maxx=x+w-1;} };
static void check(const char *s) {
  auto p=(const unsigned char*)s;
  while(*p) { assert((p[0]&0xf0)==0xe0 && p[1] && p[2]); uint16_t cp=((p[0]&15)<<12)|((p[1]&63)<<6)|(p[2]&63); assert(SquareGlyphs::find(cp)); p+=3; }
}
int main() {
  for (auto label : {"导航","音乐","电池","蓝牙","设置","关于","功能菜单","总电压","剩余容量","电流","温度","健康度","状态","电芯电压","最高","最低","压差","正在扫描","断开连接"}) check(label);
  Canvas c; SquareGlyphs::draw(c,240,0,"电池",0xffff,0,true); assert(c.runs>0 && c.minx>=220 && c.maxx<260);
  for (char d='0';d<='9';++d) assert(SquareNumeric::find(d));
  assert(SquareNumeric::find('.') && SquareNumeric::find('%') && SquareNumeric::find('-'));
  assert(SquareSmall::supports("3.287 V") && SquareSmall::supports("18.84 Ah") && SquareSmall::supports("2 mV"));
  assert(!SquareSmall::supports("ALERT"));
  Canvas sm; SquareSmall::draw(sm,240,0,"3.287 V",0xffff,0,true); assert(sm.runs>0 && sm.minx>=180 && sm.maxx<300);
  Canvas n; SquareNumeric::draw(n,240,0,"40.6%",0xffff,0); assert(n.runs>0 && n.minx>=140 && n.maxx<340);
}

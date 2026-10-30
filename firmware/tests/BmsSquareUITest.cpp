#include "../boards/216/MotoNav/BmsSquareUI.h"
#include <cassert>
#include <string>
#include <vector>
struct Canvas {
  std::vector<std::string> text;
  void fillScreen(uint16_t) {} void fillRect(int,int,int,int,uint16_t) {}
  void fillRoundRect(int,int,int,int,int,uint16_t) {} void drawRoundRect(int,int,int,int,int,uint16_t) {}
  void fillCircle(int,int,int,uint16_t) {} void setTextSize(int) {} void setTextColor(uint16_t) {}
  void setCursor(int,int) {} void print(const char *s) { text.emplace_back(s); }
  bool has(const char *s) const { for (auto &v:text) if (v==s) return true; return false; }
};
int main() {
  Bms::Data b; Canvas offline;
  BmsSquareUI::dashboard(offline,b,1000,false);
  assert(offline.has("NO DATA") && offline.has("--%") && offline.has("-- V") && !offline.has("LIVE"));
  b.link=Bms::Link::Connected; b.hasUpdate=true; b.updatedAt=1000; b.valid=Bms::Soc|Bms::Cells;
  b.soc=40.6f; for(int i=0;i<24;++i)b.cells[i]=3.2f+i*0.001f;
  for(int page=0;page<4;++page) {
    Canvas c; BmsSquareUI::cells(c,b,1000,true,page);
    assert(c.has("SIMULATED") && c.has("SIM DATA"));
    for(int j=0;j<6;++j){char s[8];snprintf(s,sizeof(s),"%02d",page*6+j+1);assert(c.has(s));}
  }
  Canvas stale; BmsSquareUI::cells(stale,b,16001,false,0);
  assert(stale.has("NO DATA") && stale.has("--.--- V"));
}

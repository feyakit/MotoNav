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
  char soc[24]; BmsSquareUI::formatSoc(soc,sizeof(soc),b,1000);
  assert(std::string(soc)=="--%");
  assert(offline.has("NO DATA") && !offline.has("LIVE"));
  assert(SquareSmall::supports("-- V") && SquareSmall::supports("18.84 Ah"));
  b.link=Bms::Link::Connected; b.hasUpdate=true; b.updatedAt=1000; b.valid=Bms::Soc|Bms::Cells;
  b.soc=40.6f; BmsSquareUI::formatSoc(soc,sizeof(soc),b,1000); assert(std::string(soc)=="40.6%"); for(int i=0;i<24;++i)b.cells[i]=3.2f+i*0.001f;
  for(int page=0;page<4;++page) {
    Canvas c; BmsSquareUI::cells(c,b,1000,true,page);
    assert(c.has("SIMULATED") && c.has("SIM DATA"));
    for(int j=0;j<6;++j){char s[8];snprintf(s,sizeof(s),"%02d",page*6+j+1);assert(c.has(s));}
  }
  BmsSquareUI::formatSoc(soc,sizeof(soc),b,16001); assert(std::string(soc)=="--%");
  Canvas stale; BmsSquareUI::cells(stale,b,16001,false,0);
  char cell[24]; BmsSquareUI::formatCell(cell,sizeof(cell),false,b.cells[0]);
  assert(stale.has("NO DATA") && std::string(cell)=="--.--- V");
  BmsSquareUI::formatCell(cell,sizeof(cell),true,3.287f); assert(std::string(cell)=="3.287 V");
}

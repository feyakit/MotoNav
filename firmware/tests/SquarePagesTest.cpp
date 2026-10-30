#include "../boards/216/MotoNav/SquarePages.h"
#include <cassert>
#include <string>
#include <vector>
struct Canvas {
  std::vector<std::string> words;
  void fillScreen(uint16_t) {} void fillRect(int,int,int,int,uint16_t) {}
  void fillRoundRect(int,int,int,int,int,uint16_t) {} void drawRoundRect(int,int,int,int,int,uint16_t) {}
  void fillCircle(int,int,int,uint16_t) {} void fillTriangle(int,int,int,int,int,int,uint16_t) {}
  void drawLine(int,int,int,int,uint16_t) {} void setTextSize(int) {} void setTextColor(uint16_t) {}
  void setCursor(int,int) {} void print(const char *s) { words.emplace_back(s); }
  bool has(const char *s) const { for (auto &x:words) if (x==s) return true; return false; }
};
int main() {
  Canvas menu; SquarePages::menu(menu);
  for (auto s : {"NAVIGATION","MUSIC","BATTERY","BLUETOOTH","SETTINGS","ABOUT"}) assert(menu.has(s));
  Canvas empty; SquarePages::bluetooth(empty,false,false,false,false,false,"--",nullptr,0);
  assert(empty.has("PHONE") && empty.has("BMS") && empty.has("OFFLINE") && empty.has("RETRY OFF"));
  assert(!empty.has("EXAMPLE BMS"));
  SquarePages::Device d[]={{"test peer","00:11:22:33:44:55",-62}};
  Canvas scan; SquarePages::bluetooth(scan,true,false,true,false,true,"--",d,1);
  assert(scan.has("CONNECTED") && scan.has("CONNECTING") && scan.has("test peer") && scan.has("-62 dBm"));
}

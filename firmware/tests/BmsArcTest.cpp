#include "../boards/216/MotoNav/BmsSquareUI.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <set>
struct Canvas {
  uint16_t before=0x1234, pixels[480*480]={}, after=0x4321;
  uint16_t *getFramebuffer(){return pixels;}
  void fillScreen(uint16_t c){std::fill(pixels,pixels+480*480,c);}
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=0;j<h;++j)for(int i=0;i<w;++i)if(x+i>=0&&x+i<480&&y+j>=0&&y+j<480)pixels[(y+j)*480+x+i]=c;}
  void fillCircle(int,int,int,uint16_t){} void fillRoundRect(int x,int y,int w,int h,int,uint16_t c){fillRect(x,y,w,h,c);}
  void drawRoundRect(int,int,int,int,int,uint16_t){} void setTextSize(int){} void setTextColor(uint16_t){} void setCursor(int,int){} void print(const char*){}
};
int main(){
  static Canvas canvas; Bms::Data b;
  BmsSquareUI::dashboard(canvas,b,100,false); uint16_t track=canvas.pixels[211*480+82];
  b.link=Bms::Link::Connected;b.hasUpdate=true;b.updatedAt=100;b.valid=Bms::Soc;b.soc=0;
  BmsSquareUI::dashboard(canvas,b,100,false); assert(canvas.pixels[211*480+82]==track);
  b.soc=50; BmsSquareUI::dashboard(canvas,b,100,false); assert(canvas.pixels[211*480+82]!=track);
  std::set<uint16_t> shades; for(int y=150;y<180;++y)for(int x=85;x<115;++x) shades.insert(canvas.pixels[y*480+x]);
  assert(shades.size()>4); // Includes antialiased coverage and gradient colors.
  BmsSquareUI::antialiasedDisc(canvas,0,0,11,0xffff); assert(canvas.before==0x1234 && canvas.after==0x4321);
}

// Host framebuffer renderer: executes BmsSquareUI's production drawing code.
// Shape rasterization is a simple software approximation of Arduino_GFX;
// this is not a capture from the physical display.
#include "../boards/216/MotoNav/BmsSquareUI.h"
#include "../boards/216/MotoNav/SquarePages.h"
#include "../libraries/GFX_Library_for_Arduino/src/font/glcdfont.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
struct Canvas {
  uint16_t pixels[480*480] = {}; int cx=0,cy=0,size=1; uint16_t color=0;
  void pixel(int x,int y,uint16_t c){if(x>=0&&y>=0&&x<480&&y<480)pixels[y*480+x]=c;}
  void fillScreen(uint16_t c){std::fill(pixels,pixels+480*480,c);}
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=0;j<h;++j)for(int i=0;i<w;++i)pixel(x+i,y+j,c);}
  void fillCircle(int x,int y,int r,uint16_t c){for(int j=-r;j<=r;++j)for(int i=-r;i<=r;++i)if(i*i+j*j<=r*r)pixel(x+i,y+j,c);}
  void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){
    if(w<=0||h<=0)return; r=std::min(r,std::min(w,h)/2);
    fillRect(x+r,y,w-2*r,h,c);fillRect(x,y+r,w,h-2*r,c);
    for(int j=0;j<r;++j)for(int i=0;i<r;++i)if((r-1-i)*(r-1-i)+(r-1-j)*(r-1-j)<=r*r){pixel(x+i,y+j,c);pixel(x+w-1-i,y+j,c);pixel(x+i,y+h-1-j,c);pixel(x+w-1-i,y+h-1-j,c);}
  }
  void drawRoundRect(int x,int y,int w,int h,int r,uint16_t c){fillRoundRect(x,y,w,h,r,c);fillRoundRect(x+1,y+1,w-2,h-2,r-1,0);}
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(;;){pixel(x0,y0,c);if(x0==x1&&y0==y1)break;int t=2*e;if(t>=dy){e+=dy;x0+=sx;}if(t<=dx){e+=dx;y0+=sy;}}}
  void fillTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){int minx=std::min({x0,x1,x2}), maxx=std::max({x0,x1,x2}), miny=std::min({y0,y1,y2}),maxy=std::max({y0,y1,y2});for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){int a=(x-x0)*(y1-y0)-(y-y0)*(x1-x0),b=(x-x1)*(y2-y1)-(y-y1)*(x2-x1),d=(x-x2)*(y0-y2)-(y-y2)*(x0-x2);if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))pixel(x,y,c);}}
  void setTextSize(int n){size=n;}
  void setTextColor(uint16_t c){color=c;}
  void setCursor(int x,int y){cx=x;cy=y;}
  void print(const char *s){for(;*s;++s){unsigned ch=(unsigned char)*s;for(int i=0;i<5;++i)for(int j=0;j<8;++j)if(font[ch*5+i]&(1<<j))fillRect(cx+i*size,cy+j*size,size,size,color);cx+=6*size;}}
  void save(const char *path){FILE *f=fopen(path,"wb");if(!f)std::abort();fprintf(f,"P6\n480 480\n255\n");for(uint16_t p:pixels){unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
};
int main(int argc,char **argv){if(argc!=3)return 2; Bms::Data b; b.link=Bms::Link::Connected;b.hasUpdate=true;b.updatedAt=1000;b.valid=Bms::Soc|Bms::Voltage|Bms::Current|Bms::Temperature|Bms::Cells|Bms::Remaining|Bms::Rated;b.soc=40.6;b.voltage=78.89;b.current=0;b.temperature=22;b.remainingAh=18.84f;b.ratedAh=45;for(int i=0;i<24;++i)b.cells[i]=3.286f+(i%3)*0.001f;Canvas g;if(!strcmp(argv[1],"summary"))BmsSquareUI::dashboard(g,b,1000,true);else if(!strcmp(argv[1],"menu")) SquarePages::menu(g);
else if(!strcmp(argv[1],"bluetooth")) { SquarePages::Device d[]={{"EXAMPLE BMS", "00:11:22:33:44:55", -62}, {"EXAMPLE DEVICE", "AA:BB:CC:DD:EE:FF", -75}}; SquarePages::bluetooth(g,true,false,false,false,true,"--",d,2); }
else if(!strncmp(argv[1],"cells",5) && argv[1][5] >= '0' && argv[1][5] <= '3' && !argv[1][6])BmsSquareUI::cells(g,b,1000,true,argv[1][5]-'0');else return 2;g.save(argv[2]);}

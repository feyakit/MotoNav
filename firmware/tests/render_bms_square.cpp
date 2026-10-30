// Host framebuffer renderer: executes BmsSquareUI's production drawing code.
// Shape rasterization is a simple software approximation of Arduino_GFX;
// this is not a capture from the physical display.
#include "../boards/216/MotoNav/BmsSquareUI.h"
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
  void setTextSize(int n){size=n;}
  void setTextColor(uint16_t c){color=c;}
  void setCursor(int x,int y){cx=x;cy=y;}
  void print(const char *s){for(;*s;++s){unsigned ch=(unsigned char)*s;for(int i=0;i<5;++i)for(int j=0;j<8;++j)if(font[ch*5+i]&(1<<j))fillRect(cx+i*size,cy+j*size,size,size,color);cx+=6*size;}}
  void save(const char *path){FILE *f=fopen(path,"wb");if(!f)std::abort();fprintf(f,"P6\n480 480\n255\n");for(uint16_t p:pixels){unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
};
int main(int argc,char **argv){if(argc!=3)return 2; Bms::Data b; b.link=Bms::Link::Connected;b.hasUpdate=true;b.updatedAt=1000;b.valid=Bms::Soc|Bms::Voltage|Bms::Current|Bms::Temperature|Bms::Cells;b.soc=40.6;b.voltage=78.89;b.current=0;b.temperature=22;for(int i=0;i<24;++i)b.cells[i]=3.286f+(i%3)*0.001f;Canvas g;if(!strcmp(argv[1],"summary"))BmsSquareUI::dashboard(g,b,1000,true);else if(!strncmp(argv[1],"cells",5) && argv[1][5] >= '0' && argv[1][5] <= '3' && !argv[1][6])BmsSquareUI::cells(g,b,1000,true,argv[1][5]-'0');else return 2;g.save(argv[2]);}

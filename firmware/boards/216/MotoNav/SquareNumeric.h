#pragma once
#include "SquareGlyphs.h"
#include "SquareNumericData.h"
namespace SquareNumeric {
inline const SquareNumericData::Glyph *find(char c) {
  for(unsigned i=0;i<SquareNumericData::count;++i) if(SquareNumericData::glyphs[i].code==c) return &SquareNumericData::glyphs[i];
  return nullptr;
}
template<class G> void draw(G &g,int cx,int y,const char *text,uint16_t fg,uint16_t bg) {
  int width=0; for(const char *p=text;*p;++p) {const auto *a=find(*p); if(a) width+=a->width;}
  int x=cx-width/2;
  for(const char *p=text;*p;++p) {
    const auto *a=find(*p); if(!a) continue;
    for(int row=0;row<48;++row) {
      int col=0;
      while(col<a->width) {
        int k=row*a->width+col; uint8_t v=a->pixels[k/2]; unsigned alpha=(k&1)?v&15:v>>4;
        int end=col+1;
        while(end<a->width) {int n=row*a->width+end;uint8_t b=a->pixels[n/2];if(((n&1)?b&15:b>>4)!=alpha)break;++end;}
        if(alpha) g.fillRect(x+col,y+row,end-col,1,SquareGlyphs::color(fg,bg,alpha));
        col=end;
      }
    }
    x+=a->width;
  }
}
}

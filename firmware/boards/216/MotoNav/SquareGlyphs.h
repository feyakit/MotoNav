#pragma once
#include "SquareGlyphData.h"
#include <stdint.h>
namespace SquareGlyphs {
inline const SquareGlyphData::Glyph *find(uint16_t code) {
  unsigned lo=0, hi=SquareGlyphData::count;
  while(lo<hi) { unsigned mid=(lo+hi)/2; if(SquareGlyphData::glyphs[mid].code<code)lo=mid+1; else hi=mid; }
  return lo<SquareGlyphData::count && SquareGlyphData::glyphs[lo].code==code ? &SquareGlyphData::glyphs[lo] : nullptr;
}
inline uint16_t color(uint16_t fg,uint16_t bg,unsigned a) {
  unsigned r=(((fg>>11)&31)*a+((bg>>11)&31)*(15-a)+7)/15;
  unsigned g=(((fg>>5)&63)*a+((bg>>5)&63)*(15-a)+7)/15;
  unsigned b=((fg&31)*a+(bg&31)*(15-a)+7)/15;
  return (uint16_t)((r<<11)|(g<<5)|b);
}
// Only fixed, audited three-byte Chinese labels are passed here; unsupported
// characters occupy space but never render invented content.
template<class G> void draw(G &g,int x,int y,const char *utf8,uint16_t fg,uint16_t bg,bool centered=false) {
  if(centered){int n=0;for(const unsigned char *q=(const unsigned char*)utf8;*q;++q)if((*q&0xc0)!=0x80)++n;x-=n*10;}
  const unsigned char *p=(const unsigned char*)utf8;
  while(*p) {
    if((p[0]&0xf0)!=0xe0 || !p[1] || !p[2]) break;
    uint16_t cp=((p[0]&15)<<12)|((p[1]&63)<<6)|(p[2]&63); p+=3;
    const auto *glyph=find(cp);
    if(glyph) for(int row=0;row<20;++row) {
      int col=0;
      while(col<20) {
        int k=row*20+col; uint8_t v=glyph->pixels[k/2]; unsigned a=(k&1)?v&15:v>>4;
        int end=col+1;
        while(end<20) { int n=row*20+end; uint8_t b=glyph->pixels[n/2]; if(((n&1)?b&15:b>>4)!=a)break; ++end; }
        if(a) g.fillRect(x+col,y+row,end-col,1,color(fg,bg,a));
        col=end;
      }
    }
    x+=20;
  }
}
}

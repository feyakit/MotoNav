#pragma once
#include "BmsData.h"
#include "SquareGlyphs.h"
#include "SquareNumeric.h"
#include "SquareSmall.h"
#include <math.h>
#include <stdio.h>

// Drawn directly into the existing Arduino_GFX canvas. The same drawing
// functions are exercised by the host framebuffer renderer for review.
namespace BmsSquareUI {
static const uint16_t BLACK=0x0000, WHITE=0xffff, MUTED=0x8cb2, CARD=0x10a3,
  TRACK=0x0885, BLUE=0x047f, GREEN=0x07e9, YELLOW=0xfe60;

template<class G> void text(G &g, int x, int y, const char *s, int size, uint16_t color, bool centered=false) {
  if (centered) { int n=0; while (s[n]) ++n; x-=n*size*3; }
  g.setTextSize(size); g.setTextColor(color); g.setCursor(x,y); g.print(s);
}
// Draw antialiased circles directly into the same RGB565 canvas used by the
// panel. The host renderer exposes the same framebuffer for review.
inline uint16_t mix(uint16_t fg,uint16_t bg,unsigned a) {
  unsigned r=(((fg>>11)&31)*a+((bg>>11)&31)*(255-a)+127)/255;
  unsigned g=(((fg>>5)&63)*a+((bg>>5)&63)*(255-a)+127)/255;
  unsigned b=((fg&31)*a+(bg&31)*(255-a)+127)/255;
  return (uint16_t)((r<<11)|(g<<5)|b);
}
template<class G> void antialiasedDisc(G &g,int cx,int cy,float radius,uint16_t color) {
  uint16_t *fb=g.getFramebuffer();
  if(!fb) { g.fillCircle(cx,cy,(int)radius,color); return; }
  int extent=(int)radius+1;
  for(int y=cy-extent;y<=cy+extent;++y) {
    if(y<0 || y>=480) continue;
    for(int x=cx-extent;x<=cx+extent;++x) {
      if(x<0 || x>=480) continue;
      float dx=x-cx,dy=y-cy;
      float cov=radius+0.5f-sqrtf(dx*dx+dy*dy);
      if(cov<=0) continue;
      unsigned alpha=(unsigned)(fminf(cov,1.0f)*255.0f);
      uint16_t &pixel=fb[y*480+x]; pixel=alpha>=255 ? color : mix(color,pixel,alpha);
    }
  }
}
template<class G> void softCard(G &g,int x,int y,int w,int h,int r,uint16_t color) {
  g.fillRoundRect(x,y,w,h,r,color);
  // Only corners need antialiasing. Their background is the black canvas.
  for(int yy=0;yy<r;++yy) for(int xx=0;xx<r;++xx) {
    int inside=0;
    for(int sy=0;sy<4;++sy) for(int sx=0;sx<4;++sx) {
      float dx=(r-0.5f)-(xx+(sx+0.5f)/4.0f);
      float dy=(r-0.5f)-(yy+(sy+0.5f)/4.0f);
      if(dx*dx+dy*dy <= r*r) ++inside;
    }
    uint16_t c=mix(color,BLACK,(unsigned)(inside*255/16));
    g.fillRect(x+xx,y+yy,1,1,c); g.fillRect(x+w-1-xx,y+yy,1,1,c);
    g.fillRect(x+xx,y+h-1-yy,1,1,c); g.fillRect(x+w-1-xx,y+h-1-yy,1,1,c);
  }
}
template<class G> void top(G &g, bool fresh, bool sim) {
  g.fillScreen(BLACK);
  SquareSmall::draw(g,16,9,sim ? "SIMULATED" : "BMS",sim ? YELLOW : WHITE,BLACK);
  SquareSmall::draw(g,400,9,fresh ? (sim ? "SIM DATA" : "LIVE") : "NO DATA",fresh ? GREEN : MUTED,BLACK,true);
  g.fillRect(12,42,456,2,CARD);
}
template<class G> void card(G &g,int x,int y,const char *value,const char *label,uint16_t valueColor=WHITE) {
  softCard(g,x,y,144,62,9,CARD);
  if(SquareSmall::supports(value)) SquareSmall::draw(g,x+72,y+5,value,valueColor,CARD,true);
  else text(g,x+72,y+9,value,2,valueColor,true);
  SquareGlyphs::draw(g,x+72,y+35,label,MUTED,CARD,true);
}
inline void formatSoc(char *buf,size_t n,const Bms::Data &b,uint32_t now) {
  if(b.fresh(now,15000) && (b.valid & Bms::Soc)) snprintf(buf,n,"%.1f%%",(double)b.soc);
  else snprintf(buf,n,"--%%");
}
template<class G> void dashboard(G &g,const Bms::Data &b,uint32_t now,bool sim) {
  bool fresh=b.fresh(now,15000);
  top(g,fresh,sim);
  float fraction=fresh && (b.valid & Bms::Soc) ? fmaxf(0,fminf(1,b.soc/100.0f)) : 0;
  // Two-degree overlapping antialiased discs form a smooth 240-degree arc.
  // The foreground is drawn only for known SOC and never invents a value.
  for(int i=0;i<=240;i+=2) {
    float a=(150+i)*0.01745329252f;
    int x=240+(int)lroundf(158*cosf(a)), y=211+(int)lroundf(158*sinf(a));
    antialiasedDisc(g,x,y,11,TRACK);
  }
  if(fresh && (b.valid & Bms::Soc) && fraction>0) for(int i=0;i<=240*fraction;i+=2) {
    float a=(150+i)*0.01745329252f;
    int x=240+(int)lroundf(158*cosf(a)), y=211+(int)lroundf(158*sinf(a));
    // A restrained green gradient; this is visual styling, not another metric.
    uint16_t shade = mix(0x07ec,0x05e5,(unsigned)(70+i*150/240));
    antialiasedDisc(g,x,y,11,shade);
  }
  SquareSmall::draw(g,240,104,"SOC",MUTED,BLACK,true);
  char buf[32];
  formatSoc(buf,sizeof(buf),b,now);
  SquareNumeric::draw(g,240,145,buf,WHITE,BLACK);
  g.drawRoundRect(188,216,100,34,4,MUTED); g.fillRect(289,225,5,16,MUTED);
  if(fresh && (b.valid & Bms::Soc)) g.fillRect(192,220,(int)(92*fraction),26,GREEN);
  char v[24],a[24],t[24],cap[24],delta[24]; float d;
  if(fresh && (b.valid & Bms::Voltage)) snprintf(v,sizeof(v),"%.2f V",(double)b.voltage); else snprintf(v,sizeof(v),"-- V");
  if(fresh && (b.valid & Bms::Current)) snprintf(a,sizeof(a),"%.1f A",(double)b.current); else snprintf(a,sizeof(a),"-- A");
  if(fresh && (b.valid & Bms::Temperature)) snprintf(t,sizeof(t),"%.0f C",(double)b.temperature); else snprintf(t,sizeof(t),"-- C");
  if(fresh && (b.valid & Bms::Remaining)) snprintf(cap,sizeof(cap),"%.2f Ah",(double)b.remainingAh); else snprintf(cap,sizeof(cap),"-- Ah");
  if(fresh && b.maxDelta(d)) snprintf(delta,sizeof(delta),"%.0f mV",(double)(d*1000)); else snprintf(delta,sizeof(delta),"-- mV");
  card(g,16,300,v,"总电压"); card(g,168,300,cap,"剩余容量"); card(g,320,300,a,"电流");
  char health[24];
  if(fresh && (b.valid & Bms::Health)) snprintf(health,sizeof(health),"%.0f%%",(double)b.soh); else snprintf(health,sizeof(health),"--%%");
  const char *status = !fresh || !(b.valid & Bms::Alarms) ? "--" : b.alarms ? "ALERT" : "OK";
  card(g,16,370,t,"温度"); card(g,168,370,health,"健康度"); card(g,320,370,status,"状态",fresh && (b.valid & Bms::Alarms) ? (b.alarms ? YELLOW : GREEN) : MUTED);
  SquareGlyphs::draw(g,240,449,"点击查看电芯",MUTED,BLACK,true);
}
inline void formatCell(char *buf,size_t n,bool valid,float value) {
  if(valid) snprintf(buf,n,"%.3f V",(double)value); else snprintf(buf,n,"--.--- V");
}
template<class G> void cells(G &g,const Bms::Data &b,uint32_t now,bool sim,int part,bool bars=true) {
  bool fresh=b.fresh(now,15000), valid=fresh && (b.valid & Bms::Cells);
  if(part<0 || part>3) part=0;
  top(g,fresh,sim);
  g.fillRoundRect(16,53,448,42,8,CARD);
  char s[40]; SquareGlyphs::draw(g,30,64,"电芯电压",WHITE,CARD); snprintf(s,sizeof(s),"%d / 4",part+1); text(g,136,65,s,2,WHITE);
  float lo=0,hi=0; int low=0,high=0;
  if(valid){lo=hi=b.cells[0]; for(int i=1;i<24;++i){ if(b.cells[i]<lo){lo=b.cells[i];low=i;} if(b.cells[i]>hi){hi=b.cells[i];high=i;} }}
  for(int row=0;row<6;++row){
    int i=part*6+row, y=104+row*41;
    if(row%2==0) g.fillRoundRect(16,y,448,38,5,CARD);
    snprintf(s,sizeof(s),"%02d",i+1); text(g,28,y+11,s,2,valid && i==high ? GREEN : valid && i==low ? BLUE : MUTED);
    formatCell(s,sizeof(s),valid,b.cells[i]);
    SquareSmall::draw(g,106,y+7,s,valid && i==high ? GREEN : valid && i==low ? BLUE : WHITE,row%2==0 ? CARD : BLACK);
    if(bars) g.fillRoundRect(285,y+8,159,22,4,TRACK);
    if(valid && bars){float p=fmaxf(0,fminf(1,(b.cells[i]-2.5f)/1.15f)); int width=(int)(159*p); if(width>0) g.fillRoundRect(285,y+8,width,22,4,GREEN);}
  }
  g.fillRect(16,358,448,2,CARD);
  char a[24],c[24],d[24];
  if(valid){snprintf(a,sizeof(a),"%.3f V",(double)hi);snprintf(c,sizeof(c),"%.3f V",(double)lo);snprintf(d,sizeof(d),"%.0f mV",(double)((hi-lo)*1000));}
  else {snprintf(a,sizeof(a),"-- V");snprintf(c,sizeof(c),"-- V");snprintf(d,sizeof(d),"-- mV");}
  SquareGlyphs::draw(g,88,371,"最高",MUTED,BLACK,true); SquareGlyphs::draw(g,240,371,"最低",MUTED,BLACK,true); SquareGlyphs::draw(g,392,371,"压差",MUTED,BLACK,true);
  SquareSmall::draw(g,88,393,a,GREEN,BLACK,true); SquareSmall::draw(g,240,393,c,GREEN,BLACK,true); SquareSmall::draw(g,392,393,d,GREEN,BLACK,true);
  if(valid){snprintf(s,sizeof(s),"#%02d",high+1);text(g,88,418,s,1,MUTED,true);snprintf(s,sizeof(s),"#%02d",low+1);text(g,240,418,s,1,MUTED,true);}
  text(g,240,453,"SWIPE: PAGE  |  TOP: BACK / MODE",1,MUTED,true);
}
}

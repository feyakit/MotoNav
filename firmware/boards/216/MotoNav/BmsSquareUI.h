#pragma once
#include "BmsData.h"
#include <math.h>
#include <stdio.h>

// Drawn directly into the existing Arduino_GFX canvas. The same drawing
// functions are exercised by the host framebuffer renderer for review.
namespace BmsSquareUI {
static const uint16_t BLACK=0x0000, WHITE=0xffff, MUTED=0x8cb2, CARD=0x10a3,
  TRACK=0x0885, BLUE=0x047f, GREEN=0x0669, YELLOW=0xfe60;

template<class G> void text(G &g, int x, int y, const char *s, int size, uint16_t color, bool centered=false) {
  if (centered) { int n=0; while (s[n]) ++n; x-=n*size*3; }
  g.setTextSize(size); g.setTextColor(color); g.setCursor(x,y); g.print(s);
}
template<class G> void top(G &g, bool fresh, bool sim) {
  g.fillScreen(BLACK);
  text(g,16,14,sim ? "SIMULATED" : "BMS",2,sim ? YELLOW : WHITE);
  text(g,420,14,fresh ? (sim ? "SIM DATA" : "LIVE") : "NO DATA",2,fresh ? GREEN : MUTED,true);
  g.fillRect(12,42,456,2,CARD);
}
template<class G> void card(G &g,int x,int y,const char *value,const char *label,uint16_t valueColor=WHITE) {
  g.fillRoundRect(x,y,144,62,9,CARD);
  text(g,x+72,y+9,value,2,valueColor,true);
  text(g,x+72,y+36,label,1,MUTED,true);
}
template<class G> void dashboard(G &g,const Bms::Data &b,uint32_t now,bool sim) {
  bool fresh=b.fresh(now,15000);
  top(g,fresh,sim);
  float fraction=fresh && (b.valid & Bms::Soc) ? fmaxf(0,fminf(1,b.soc/100.0f)) : 0;
  // 240-degree gauge; drawing short circles retains the same primitives on
  // Arduino_GFX and in the host renderer.
  for(int i=0;i<=240;++i) {
    float a=(150+i)*0.01745329252f;
    int x=240+(int)lroundf(158*cosf(a)), y=211+(int)lroundf(158*sinf(a));
    g.fillCircle(x,y,11,fresh && (b.valid & Bms::Soc) && i<=240*fraction ? GREEN : TRACK);
  }
  text(g,240,108,"SOC",2,MUTED,true);
  char buf[32];
  if(fresh && (b.valid & Bms::Soc)) snprintf(buf,sizeof(buf),"%.1f%%",(double)b.soc); else snprintf(buf,sizeof(buf),"--%%");
  text(g,240,153,buf,5,WHITE,true);
  g.drawRoundRect(188,216,100,34,4,MUTED); g.fillRect(289,225,5,16,MUTED);
  if(fresh && (b.valid & Bms::Soc)) g.fillRect(192,220,(int)(92*fraction),26,GREEN);
  char v[24],a[24],t[24],cap[24],delta[24]; float d;
  if(fresh && (b.valid & Bms::Voltage)) snprintf(v,sizeof(v),"%.2f V",(double)b.voltage); else snprintf(v,sizeof(v),"-- V");
  if(fresh && (b.valid & Bms::Current)) snprintf(a,sizeof(a),"%.1f A",(double)b.current); else snprintf(a,sizeof(a),"-- A");
  if(fresh && (b.valid & Bms::Temperature)) snprintf(t,sizeof(t),"%.0f C",(double)b.temperature); else snprintf(t,sizeof(t),"-- C");
  if(fresh && (b.valid & Bms::Soc)) snprintf(cap,sizeof(cap),"%.2f Ah",45.0*b.soc/100); else snprintf(cap,sizeof(cap),"-- Ah");
  if(fresh && b.maxDelta(d)) snprintf(delta,sizeof(delta),"%.0f mV",(double)(d*1000)); else snprintf(delta,sizeof(delta),"-- mV");
  card(g,16,300,v,"PACK VOLTAGE"); card(g,168,300,cap,"EST. CAPACITY"); card(g,320,300,a,"CURRENT");
  card(g,16,370,t,"TEMPERATURE"); card(g,168,370,delta,"CELL SPREAD"); card(g,320,370,fresh ? "DATA" : "OFFLINE","LINK",fresh ? GREEN : MUTED);
  text(g,240,453,"TAP FOR CELLS",1,MUTED,true);
}
template<class G> void cells(G &g,const Bms::Data &b,uint32_t now,bool sim,int part) {
  bool fresh=b.fresh(now,15000), valid=fresh && (b.valid & Bms::Cells);
  if(part<0 || part>3) part=0;
  top(g,fresh,sim);
  g.fillRoundRect(16,53,448,42,8,CARD);
  char s[40]; snprintf(s,sizeof(s),"CELL VOLTAGE  %d / 4",part+1); text(g,30,65,s,2,WHITE);
  float lo=0,hi=0; int low=0,high=0;
  if(valid){lo=hi=b.cells[0]; for(int i=1;i<24;++i){ if(b.cells[i]<lo){lo=b.cells[i];low=i;} if(b.cells[i]>hi){hi=b.cells[i];high=i;} }}
  for(int row=0;row<6;++row){
    int i=part*6+row, y=104+row*41;
    if(row%2==0) g.fillRoundRect(16,y,448,38,5,CARD);
    snprintf(s,sizeof(s),"%02d",i+1); text(g,28,y+11,s,2,MUTED);
    if(valid) snprintf(s,sizeof(s),"%.3f V",(double)b.cells[i]); else snprintf(s,sizeof(s),"--.--- V");
    text(g,106,y+11,s,2,WHITE);
    g.fillRoundRect(285,y+8,159,22,4,TRACK);
    if(valid){float p=fmaxf(0,fminf(1,(b.cells[i]-2.5f)/1.15f)); int width=(int)(159*p); if(width>0) g.fillRoundRect(285,y+8,width,22,4,GREEN);}
  }
  g.fillRect(16,358,448,2,CARD);
  char a[24],c[24],d[24];
  if(valid){snprintf(a,sizeof(a),"%.3f V",(double)hi);snprintf(c,sizeof(c),"%.3f V",(double)lo);snprintf(d,sizeof(d),"%.0f mV",(double)((hi-lo)*1000));}
  else {snprintf(a,sizeof(a),"-- V");snprintf(c,sizeof(c),"-- V");snprintf(d,sizeof(d),"-- mV");}
  text(g,88,375,"MAX",1,MUTED,true); text(g,240,375,"MIN",1,MUTED,true); text(g,392,375,"SPREAD",1,MUTED,true);
  text(g,88,393,a,2,GREEN,true); text(g,240,393,c,2,GREEN,true); text(g,392,393,d,2,GREEN,true);
  if(valid){snprintf(s,sizeof(s),"#%02d",high+1);text(g,88,418,s,1,MUTED,true);snprintf(s,sizeof(s),"#%02d",low+1);text(g,240,418,s,1,MUTED,true);}
  text(g,240,453,"TAP: NEXT  |  TOP: BACK",1,MUTED,true);
}
}

#pragma once
#include "BmsSquareUI.h"
#include <stdio.h>
namespace SquarePages {
using namespace BmsSquareUI;
struct Device { const char *name; const char *address; int rssi; };
template<class G> void title(G &g,const char *name) {
  g.fillScreen(BLACK); text(g,20,15,"< MENU",2,BLUE); text(g,260,15,name,2,WHITE,true);
  g.fillRect(16,43,448,2,CARD);
}
template<class G> void menu(G &g) {
  title(g,"FUNCTIONS");
  const char *labels[]={"NAVIGATION","MUSIC","BATTERY","BLUETOOTH","SETTINGS","ABOUT"};
  const uint16_t colors[]={0x047f,0x6904,0x0386,0x0148,0x3186,0x3186};
  for(int i=0;i<6;++i){ int x=20+(i%2)*225, y=62+(i/2)*132;
    g.fillRoundRect(x,y,215,120,12,colors[i]);
    int cx=x+107, cy=y+46;
    if(i==0) g.fillTriangle(cx-20,cy-2,cx+22,cy-25,cx+5,cy+23,WHITE);
    if(i==1) { g.fillRect(cx+5,cy-22,6,37,WHITE); g.fillRect(cx+5,cy-22,19,6,WHITE); g.fillCircle(cx-1,cy+16,9,WHITE); }
    if(i==2) { g.drawRoundRect(cx-23,cy-15,44,30,5,WHITE); g.fillRect(cx+22,cy-6,5,12,WHITE); g.fillRect(cx-18,cy-10,26,20,WHITE); }
    if(i==3) { g.drawLine(cx,cy-25,cx,cy+25,WHITE); g.drawLine(cx,cy-25,cx+15,cy-12,WHITE); g.drawLine(cx+15,cy-12,cx-14,cy+14,WHITE); g.drawLine(cx-14,cy-14,cx+15,cy+12,WHITE); g.drawLine(cx+15,cy+12,cx,cy+25,WHITE); }
    if(i==4) { g.fillCircle(cx,cy,21,WHITE); g.fillCircle(cx,cy,9,colors[i]); }
    if(i==5) { g.fillCircle(cx,cy,21,WHITE); text(g,cx,cy-14,"i",3,colors[i],true); }
    text(g,x+107,y+87,labels[i],2,WHITE,true);
  }
}
template<class G> void bluetooth(G &g,bool phone,bool connected,bool connecting,bool scanning,bool retry,const char *age,const Device *d,int count) {
  title(g,"BLUETOOTH");
  g.fillRoundRect(16,54,448,52,8,CARD); text(g,28,64,"PHONE",2,WHITE); text(g,420,64,phone?"CONNECTED":"OFFLINE",2,phone?GREEN:MUTED,true);
  g.fillRoundRect(16,112,448,52,8,CARD); text(g,28,122,"BMS",2,WHITE); text(g,420,122,connected?"CONNECTED":connecting?"CONNECTING":"OFFLINE",2,connected?GREEN:MUTED,true);
  text(g,22,175,scanning?"SCANNING...":"NEARBY (TAP TO CONNECT)",1,MUTED); text(g,290,175,retry?"RETRY ON":"RETRY OFF",1,MUTED); text(g,456,175,age,1,MUTED,true);
  for(int i=0;i<4;++i) {
    int y=195+i*52; g.fillRoundRect(16,y,448,46,7,CARD);
    if(i<count) { char r[24]; text(g,28,y+5,d[i].name && d[i].name[0] ? d[i].name : "UNKNOWN",2,WHITE);
      text(g,28,y+27,d[i].address,1,MUTED); snprintf(r,sizeof(r),"%d dBm",d[i].rssi); text(g,416,y+15,r,1,MUTED,true);
    }
  }
  g.fillRoundRect(16,413,215,48,10,BLUE); g.fillRoundRect(249,413,215,48,10,CARD);
  text(g,123,427,scanning?"SCANNING":"SCAN",2,WHITE,true); text(g,356,427,"DISCONNECT",2,WHITE,true);
}
template<class G> void info(G &g,bool about) {
  title(g,about?"ABOUT":"SETTINGS");
  text(g,240,150,about?"MOTONAV":"PHONE SETTINGS",3,WHITE,true);
  text(g,240,210,about?"ESP32-S3 / 2.16":"USE PHONE APP",2,MUTED,true);
  text(g,240,245,about?"BMS PROTOCOL UNKNOWN":"FOR EXISTING OPTIONS",2,MUTED,true);
}
}

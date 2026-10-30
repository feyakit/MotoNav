#include "../boards/216/MotoNav/BmsAdvertisement.h"
#include <cassert>
#include <cstring>
int main() {
  const uint8_t adv[] = {2,1,6,6,9,'H','e','l','l','o'};
  char name[4] = {}; assert(bmsAdvertisementName(adv,sizeof(adv),name,sizeof(name)) && !strcmp(name,"Hel"));
  const uint8_t bad[] = {9,9,'x'}; strcpy(name,"old"); assert(!bmsAdvertisementName(bad,sizeof(bad),name,sizeof(name)) && !strcmp(name,"old"));
  const uint8_t nonascii[] = {3,9,'A',0xff}; assert(bmsAdvertisementName(nonascii,sizeof(nonascii),name,sizeof(name)) && !strcmp(name,"A?"));
  assert(!bmsAdvertisementName(nullptr,0,name,sizeof(name)));
  const uint8_t nameless[] = {2,1,6,0}; assert(!bmsAdvertisementName(nameless,sizeof(nameless),name,sizeof(name)));
}

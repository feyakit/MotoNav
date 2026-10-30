#include "../boards/216/MotoNav/BmsCharacteristics.h"
#include <cassert>
int main() {
  BmsCharacteristics c;
  assert(!c.select(3, false));
  assert(c.add(2,3,20,0x10));
  assert(c.add(7,8,20,0x20));
  assert(c.select(3,false)->end == 6);
  assert(!c.select(3,true));
  assert(c.select(8,true)->end == 20);
  assert(!c.select(8,false));
  assert(c.add(21,22,22,0x10));
  assert(!c.select(22,false)); // no room for a descriptor
  assert(!c.add(30,29,40,0x10));
  assert(!c.select(3,false)); // incomplete discovery cannot select
  c.clear();
  for (int i=0; i<64; ++i) assert(c.add(1+i*3,2+i*3,3+i*3,0x10));
  assert(!c.add(193,194,195,0x10));
  assert(!c.select(2,false));
}

#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
using namespace tr::firstperson;
using namespace tr::motiongun;
int checks=0;
void Check(bool ok,const char* s) {++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
#include "recorded_hands_out_regression.inl"
int main() {TestRecordedHandsOutSwivels();std::printf("PASS: %d swivel/walking checks\n",checks);}

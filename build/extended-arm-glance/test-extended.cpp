#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
int checks=0;void Check(bool ok,const char* text){++checks;if(!ok){printf("FAIL after %d: %s\n",checks,text);std::exit(1);}}
#include "../../tools/extended_arm_glance_regression.inl"
int main(){TestExtendedArmGlances();printf("PASS: %d extended-arm stance checks\n",checks);}

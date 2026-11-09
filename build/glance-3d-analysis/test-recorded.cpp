#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
int checks=0;void Check(bool ok,const char* text){++checks;if(!ok){printf("FAIL after %d: %s\n",checks,text);std::exit(1);}}
#include "../../tools/recorded_planted_glance_regression.inl"
int main(){TestRecordedPlantedGlances();printf("PASS: %d recorded glance/step checks\n",checks);}

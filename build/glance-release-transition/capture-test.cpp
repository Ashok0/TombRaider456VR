#include "ControllerBodyAnchor.h"
#include <cstdio>
#include <cstdlib>
int checks=0;
void Check(bool pass,const char* message) {++checks;if(!pass){printf("FAIL after %d checks: %s\n",checks,message);std::exit(1);}}
#include "../../tools/body_anchor_capture.inl"
int main(){TestCapturedBodyAnchorTransitions();printf("PASS: %d captured-transition checks\n",checks);}

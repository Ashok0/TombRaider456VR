#define main RunFirstPersonSuite
#include "tools/first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() { return RunFirstPersonSuite(); }

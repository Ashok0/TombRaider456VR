#define main UnusedFullFirstPersonSuite
#include "../../tools/first_person_tests.cpp"
#undef main
namespace tr { void VRMenuShutdown() {} }
int main() {
    TestArmedInventoryButton();TestWorldInventory();TestGamepadFocus();
    std::printf("PASS: %d input and inventory checks\n",checks);
}

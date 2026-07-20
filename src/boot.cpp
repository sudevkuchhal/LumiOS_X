#include "boot.h"
#include "display.h"
#include "leds.h"

BootManager boot;

void BootManager::begin()
{
    // Boot Screen
    showBootScreen();

    // LED Startup Animation
    leds.bootAnimation();

    // Home Screen
    setPage(PAGE_HOME);
}
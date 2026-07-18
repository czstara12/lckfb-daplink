#include "ui.h"
#include "screens.h"


static enum CURRENT_SCREEN current_screen = SCREEN_MENU;

void current_screen_set(enum CURRENT_SCREEN screen)
{
    current_screen = screen;
}

enum CURRENT_SCREEN current_screen_get(void)
{
    return current_screen;
}

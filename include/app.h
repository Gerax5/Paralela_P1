#pragma once
#include <stdbool.h>

typedef struct App App;

bool appInit(App **outApp, int width, int height, const char *title);
void appRun(App *app);      // loop principal (ESC para salir)
void appShutdown(App *app); // libera SDL y memoria
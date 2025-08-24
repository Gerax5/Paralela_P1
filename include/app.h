#pragma once
#include <stdbool.h>

typedef struct App App;

bool appInit(App **outApp, int width, int height, const char *title,
             const char *imagePath, int npoints);
void appRun(App *app);
void appShutdown(App *app);

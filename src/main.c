#include "app.h"
#include "config.h"
#include <stdio.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  App *app = NULL;
  if (!appInit(&app, defaultWidth, defaultHeight, defaultTitle))
  {
    return 1;
  }

  appRun(app);
  appShutdown(app);
  return 0;
}
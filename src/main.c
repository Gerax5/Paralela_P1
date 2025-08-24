#include "app.h"
#include "config.h"
#include <stdio.h>

int main(int argc, char **argv)
{
  const char *imgPath = (argc >= 2) ? argv[1] : NULL;

  App *app = NULL;
  if (!appInit(&app, defaultWidth, defaultHeight, defaultTitle, imgPath))
  {
    return 1;
  }
  appRun(app);
  appShutdown(app);
  return 0;
}
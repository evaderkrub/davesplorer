// Entry point. SDL_main.h supplies WinMain for the windowed subsystem and
// hands the arguments to this main.
#include <SDL3/SDL_main.h>

#include "platform/Host.h"

int main(int argc, char** argv)
{
    return platform::RunApplication(argc, argv);
}

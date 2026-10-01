#include "PlatformLifetime.h"

#include <SDL3/SDL.h>

namespace Rhiza
{

PlatformLifetime::~PlatformLifetime()
{
    SDL_Quit();
}

}  // namespace Rhiza

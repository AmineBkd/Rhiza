#include "FileSystem.h"

#include <SDL3/SDL.h>

namespace Rhiza
{

bool readFile( const std::string &path, std::vector<uint8_t> &out )
{
    size_t size = 0;
    void *data = SDL_LoadFile( path.c_str(), &size );
    if( !data )
    {
        SDL_Log( "could not read '%s': %s", path.c_str(), SDL_GetError() );
        return false;
    }

    const uint8_t *bytes = static_cast<const uint8_t *>( data );
    out.assign( bytes, bytes + size );
    SDL_free( data );
    return true;
}

}  // namespace Rhiza

#pragma once

// Include where main( int argc, char *argv[] ) is defined. Android's Java side
// calls SDL_main in libmain.so, so there main is renamed and exported.
#if defined( __ANDROID__ )
#    define main SDL_main
extern "C" __attribute__( ( visibility( "default" ) ) ) int SDL_main( int argc, char *argv[] );
#endif

package io.github.aminebkd.rhiza;

import org.libsdl.app.SDLActivity;

public class RhizaActivity extends SDLActivity {
    // SDL is linked statically into the game, so there is no libSDL3.so to load first.
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}

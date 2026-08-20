#pragma once

struct SDL_Window;

class RhizaEngine {
    public:
        RhizaEngine();
        int initialize_window();
        void initialize_renderer();
    private:
        SDL_Window *window = nullptr;

};
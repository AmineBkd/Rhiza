#pragma once

struct SDL_Window;
struct SDL_Renderer;

class RhizaEngine {
    public:
        RhizaEngine();
        int initialize_window();
        void initialize_renderer();
    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
};
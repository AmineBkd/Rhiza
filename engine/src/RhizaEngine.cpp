#include <iostream>
#include <SDL3/SDL.h>
#include <Rhiza/RhizaEngine.h>

RhizaEngine::RhizaEngine() { }
int RhizaEngine::initialize_window() {
    std::string window_name = "Rhiza";

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer(window_name.c_str(), 800, 600, 0, &window, &renderer)) {
        SDL_Log("Window/Renderer Creation Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    return 0;
}

void RhizaEngine::initialize_renderer() {
    bool is_running = true;
    SDL_Event event;

    while (is_running) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_EVENT_QUIT) {
                is_running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 40, 80, 255);
        SDL_RenderClear(renderer);

        SDL_RenderPresent(renderer);
    }


    std::cout << "exiting ..." << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
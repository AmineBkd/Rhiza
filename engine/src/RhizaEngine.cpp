#include <iostream>
#include <SDL3/SDL.h>
#include <Rhiza/RhizaEngine.h>
#include <OGRE-Next/Ogre.h>

RhizaEngine::RhizaEngine() { }

int RhizaEngine::initialize_window() {
    std::string window_name = "Rhiza";
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow(window_name.c_str(), 800, 600, 0);
    if (!window) {
        SDL_Log("Window Creation Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    return 0;
}

void RhizaEngine::initialize_renderer() {
    SDL_Event event;
    bool is_running = true;

    while (is_running) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_EVENT_QUIT) {
                is_running = false;
            }
        }

        //SDL_SetRenderDrawColor(renderer, 20, 40, 80, 255);
        //SDL_RenderClear(renderer);

        //SDL_RenderPresent(renderer);
    }


    std::cout << "exiting ..." << std::endl;
    //SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
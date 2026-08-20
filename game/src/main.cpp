#include <iostream>
#include <Rhiza/RhizaEngine.h>

int main(int argc, char *argv[]) {
    RhizaEngine engine = RhizaEngine();
    engine.initialize_window();
    engine.initialize_renderer();

    return 0;
}
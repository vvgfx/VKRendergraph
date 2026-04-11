#include <RGEngine.h>

int main(int argc, char *argv[])
{

    // switch from vulkan engine to pbr engine for inheritance hierarchy
    RGEngine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}

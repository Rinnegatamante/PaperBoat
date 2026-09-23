#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <libultraship.h>

#include "Engine.h"
#include "port/interpolation/FrameInterpolation.h"

#ifdef __vita__
#include <vitasdk.h>
#include <pthread.h>
int _newlib_heap_size_user = 256 * 1024 * 1024;

extern "C" int __wrap_pte_osSemaphorePost(SceUID handle, int count) {
    SceKernelSemaInfo info;
    info.size = sizeof(SceKernelSemaInfo);
    sceKernelGetSemaInfo(handle, &info);
    if (info.currentCount < info.maxCount) {
        sceKernelSignalSema(handle, count);
    }
    return 0;
}
#endif

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#include "port/web/WebUtils.h"
#endif
#ifdef __ANDROID__
// Redefines main() to SDL_main(), which SDLActivity calls into. The game data
// is unpacked before this process starts, by GameAssets.kt.
#include <SDL2/SDL_main.h>
#endif

MtxF sInterpolationMatrixStack[0x1000];
MtxF* gInterpolationMatrix = &sInterpolationMatrixStack[0];

extern "C" {
void load_engine_data(void);
void create_audio_system(void);
void init_game_globals(void);
void Graphics_ThreadUpdate(void); // New unified frame function from gfx_frame.c
}

extern "C" void Graphics_PushFrame(Gfx* displayList) {
    GameEngine::ProcessGfxCommands(displayList);
}

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#elif defined(__vita__)
extern "C" void* vita_main(void* argv);

int main(int argc, char** argv) {
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    sceIoMkdir("ux0:data/paperboat", 0777);
    sceIoMkdir("ux0:data/paperboat/logs", 0777);
    sceIoMkdir("ux0:data/paperboat/shader_cache", 0777);

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
    pthread_create(&thread, &attr, vita_main, nullptr);
    pthread_attr_destroy(&attr);
    return sceKernelExitDeleteThread(0);
}

void* vita_main(void* ignored) {
    int argc = 0;
    char** argv = nullptr;
#else
#if defined(__cplusplus) && defined(PLATFORM_IOS)
extern "C"
#endif
    int main(int argc, char* argv[]) {
#endif
#ifdef __EMSCRIPTEN__
    // Everything the engine writes lives under /storage, an IndexedDB mount.
    // Both calls must precede anything that looks for a file there.
    WebCache_Mount("/storage");
    WebCache_Load();
#endif

    GameEngine::Create(argc, argv);

    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());

    // Initialize game systems
    init_game_globals();
    load_engine_data();

    // Main loop
    while (wnd->IsRunning()) {
        GameEngine::Instance->StartFrame();
        FrameInterpolation_StartRecord();
        Graphics_ThreadUpdate();
        FrameInterpolation_StopRecord();
#ifdef __EMSCRIPTEN__
        // A tab can close without warning, so sync periodically, not just on exit.
        static uint32_t lastSync = 0;
        const uint32_t now = SDL_GetTicks();
        if (now - lastSync > 5000) {
            lastSync = now;
            WebCache_Save();
        }
#endif
    }

    GameEngine::Instance->Destroy();
    GameEngine::RelaunchIfRequested(argc, argv);
#ifdef __EMSCRIPTEN__
    // Destroy() wrote the config after the last periodic sync. Not awaited: the
    // write finishes in the page after the runtime exits.
    WebCache_SaveNoWait();
#endif
#ifdef __vita__
    return nullptr;
#else
    return 0;
#endif
}

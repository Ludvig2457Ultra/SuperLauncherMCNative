#include <cstdio>
#include <string>
#include "../core/log.h"
#include "../platform/plat.h"

// Minimal console entry point для сборки ядра без Win32-UI (Linux/macOS).
// Полноценный CLI (install/run версии) добавляется поверх core-слоя.
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    sl::log_info("SuperLauncherNative core (headless) — cross-platform build");
    sl::log_info("home: " + sl::env_user_home());
    sl::log_info("core OK. UI слой Win32 не включён в эту сборку.");
    return 0;
}
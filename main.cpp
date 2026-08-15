#include "src/ui/ui.h"
#include "src/core/paths.h"
#include "src/core/config.h"
#include "src/core/log.h"
#include "src/minecraft/install.h"

// Точка входа (SUBSYSTEM:WINDOWS, ENTRY:mainCRTStartup -> main).
int main() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    sl::mkdirs_w(sl::app_root());
    sl::log_set_file(sl::path_join(sl::app_root_utf8(), "launcher.log"));

    sl::Config cfg;
    cfg.load();
    sl::set_network_proxy(cfg.proxy_host, cfg.proxy_port, cfg.proxy_user, cfg.proxy_pass);

    return slui::run_app(hInstance, SW_SHOWDEFAULT);
}

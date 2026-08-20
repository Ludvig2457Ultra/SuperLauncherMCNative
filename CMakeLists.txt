cmake_minimum_required(VERSION 3.20)
project(SuperLauncherNative
        VERSION 2.0
        DESCRIPTION "SuperLauncher 2.0 native C++ Win32 launcher"
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(STATUS "SuperLauncherNative: cross-platform сборка ядра БЕЗ Win32-UI (Linux/macOS)")
endif()

# Статический CRT (/MT) — как в build.bat, чтобы не зависеть от VC++ Redist.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING
    "Runtime library variant for MSVC")

# Кроссплатформенное ядро: без Win32-API, компилируется и на Windows, и на Unix.
set(SL_CORE
    src/core/json.cpp
    src/core/paths.cpp
    src/core/log.cpp
    src/core/config.cpp
    src/core/zip.cpp
    src/core/inflate.cpp
    src/crypto/sha1.c
    src/crypto/sha1file.cpp
    src/crypto/sha256.cpp
    src/net/http.cpp
    src/minecraft/version.cpp
    src/minecraft/install.cpp
    src/minecraft/command.cpp
    src/platform/plat.cpp
)

if(WIN32)
    # Полная сборка с Win32-UI (повторяет scripts/build.bat).
    set(SL_ALL ${SL_CORE}
        src/minecraft/neoforge.cpp
        src/instances/instances.cpp
        src/backend/account.cpp
        src/backend/servers.cpp
        src/backend/updates.cpp
        src/backend/mods.cpp
        src/backend/ai.cpp
        src/net/ping.cpp
        src/ui/ui.cpp
        src/ui/pages/pages.cpp
        src/ui/pages/pages_util.cpp
        src/ui/pages/page_home.cpp
        src/ui/pages/page_account.cpp
        src/ui/pages/page_mods.cpp
        src/ui/pages/page_builds.cpp
        src/ui/pages/page_skins.cpp
        src/ui/pages/page_news.cpp
        src/ui/pages/page_updates.cpp
        src/ui/pages/page_servers.cpp
        src/ui/pages/page_settings.cpp
        src/ui/pages/page_launch.cpp
        src/ui/pages/page_ai.cpp
        src/ui/dialogs/dialogs.cpp
        main.cpp
    )
else()
    # Headless-ядро: пока консольная заглушка, поверх неё наращивается CLI.
    set(SL_ALL ${SL_CORE}
        src/platform/headless_main.cpp
    )
endif()

add_executable(SuperLauncherNative ${SL_ALL})

target_compile_definitions(SuperLauncherNative PRIVATE
    UNICODE
    _UNICODE
)

if(WIN32)
    # Windows GUI-подсистема + main() как точка входа (как /ENTRY:mainCRTStartup).
    set_target_properties(SuperLauncherNative PROPERTIES
        WIN32_EXECUTABLE TRUE
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # LNK-флаги: /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OPT:REF
    target_link_options(SuperLauncherNative PRIVATE
        /SUBSYSTEM:WINDOWS
        /ENTRY:mainCRTStartup
        /OPT:REF
    )

    target_link_libraries(SuperLauncherNative PRIVATE
        user32
        gdi32
        comctl32
        wininet
        advapi32
        shell32
        ole32
        comdlg32
    )

    # Опциональный шаг UPX-сжатия после линковки (как в build.bat [3/3]).
    # Ищем upx.exe в WinGet-директории UPX* или в PATH.
    find_program(SL_UPX_BIN upx.exe
        PATHS "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/UPX*"
        HINTS "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/UPX*"
        NO_DEFAULT_PATH
    )
    if(NOT SL_UPX_BIN)
        find_program(SL_UPX_BIN upx.exe)
    endif()
    if(SL_UPX_BIN)
        message(STATUS "UPX found: ${SL_UPX_BIN} (применяю сжатие после сборки)")
        add_custom_command(TARGET SuperLauncherNative POST_BUILD
            COMMAND "${SL_UPX_BIN}" --best --lzma "$<TARGET_FILE:SuperLauncherNative>"
            COMMENT "UPX-сжатие SuperLauncherNative.exe"
        )
    else()
        message(STATUS "UPX не найден - сжатие пропущено (как в build.bat)")
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(SuperLauncherNative PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()
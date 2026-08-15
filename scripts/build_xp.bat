@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  SuperLauncherNative - Windows XP-совместимая сборка
REM  32-бита (i686), CRT = системный msvcrt.dll (есть в XP).
REM  Тулчейн: MinGW-w64 GCC 16 (niXman, POSIX threads, MSVCRT).
REM  Важно: НЕ LLVM/Clang libc++ - она тянет Vista+ функции
REM  (SRWLock, Fls, ConditionVariable, K32EnumProcessModules),
REM  из-за чего XP выдаёт "not a valid Win32 application".
REM ============================================================
set "ROOT=%~dp0.."
cd /d "%ROOT%"

REM ---- найти MinGW-w64 GCC (i686) ----
set "MG="
REM 1) локальный распакованный дистрибутив
if exist "C:\Users\artem\AppData\Local\Temp\opencode\i686-gcc\mingw32\bin\i686-w64-mingw32-g++.exe" (
    set "MG=C:\Users\artem\AppData\Local\Temp\opencode\i686-gcc\mingw32"
)
REM 2) WinLibs / другие пути
if not defined MG for /d %%d in ("C:\mingw32" "C:\mingw" "C:\mingw-w64\mingw32") do (
    if exist "%%d\bin\i686-w64-mingw32-g++.exe" set "MG=%%d"
)
if not defined MG (echo [error] MinGW-w64 GCC i686 not found & exit /b 1)
set "BIN=%MG%\bin"

if not exist "build_t\obj" mkdir "build_t\obj"

echo Compiling with: %BIN%

echo [1/3] compiling sha1.c (C) ...
"%BIN%\i686-w64-mingw32-gcc.exe" -c -O2 -o build_t\obj\sl_xp_sha1.o src\crypto\sha1.c || exit /b 1

echo [2/3] compiling C++ modules ...
set "SRCS=src\core\json.cpp src\core\paths.cpp src\core\log.cpp src\core\config.cpp src\core\zip.cpp src\core\inflate.cpp src\crypto\sha1file.cpp src\crypto\sha256.cpp src\net\http.cpp src\minecraft\version.cpp src\minecraft\install.cpp src\minecraft\command.cpp src\instances\instances.cpp src\backend\account.cpp src\backend\servers.cpp src\backend\updates.cpp src\backend\mods.cpp src\backend\ai.cpp src\ui\ui.cpp src\ui\pages\pages.cpp src\ui\pages\pages_util.cpp src\ui\pages\page_home.cpp src\ui\pages\page_account.cpp src\ui\pages\page_mods.cpp src\ui\pages\page_builds.cpp src\ui\pages\page_skins.cpp src\ui\pages\page_news.cpp src\ui\pages\page_updates.cpp src\ui\pages\page_servers.cpp src\ui\pages\page_settings.cpp src\ui\pages\page_launch.cpp src\ui\pages\page_ai.cpp src\ui\dialogs\dialogs.cpp main.cpp"
for %%s in (%SRCS%) do (
    "%BIN%\i686-w64-mingw32-g++.exe" -std=c++17 -Os -ffunction-sections -fdata-sections -c -DSL_USE_OPENSSL -D_WIN32_WINNT=0x0501 -D_WINNT_VER=0x0501 -DUNICODE -D_UNICODE -I"third_party\openssl\include" -o "build_t\obj\sl_xp_%%~ns.o" "%%s" || exit /b 1
)

echo [3/3] linking ...
REM -mwindows - статическая сборка на libstdc++ (GCC не тянет Vista+ API).
REM OpenSSL 1.1.1 (third_party\openssl) даёт TLS 1.2/1.3 на XP; WinINet там умеет только TLS 1.0.
REM --gc-sections выкидывает неиспользуемые секции; -s убирает символы.
REM --major/minor-*-version=5.1: XP-загрузчик не грузит PE с subsystem 6+.
"%BIN%\i686-w64-mingw32-g++.exe" -o "SuperLauncherNative_xp.exe" -mwindows -static -s -Wl,--gc-sections -Wl,--major-os-version,5 -Wl,--minor-os-version,1 -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1 build_t\obj\sl_xp_*.o -lstdc++ -lwinpthread third_party\openssl\libssl.a third_party\openssl\libcrypto.a -luser32 -lgdi32 -lcomctl32 -ladvapi32 -lshell32 -lole32 -lcomdlg32 -lws2_32 -lcrypt32 -liphlpapi || exit /b 1

echo [4/4] UPX-сжатие ...
set "SLUPX="
for /d %%p in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\UPX*") do set "UPXDIR=%%p"
if defined UPXDIR for /f "delims=" %%u in ('dir /s /b "%UPXDIR%\upx.exe" 2^>nul') do set "SLUPX=%%u"
if defined SLUPX (
    "%SLUPX%" --best --lzma "SuperLauncherNative_xp.exe" || exit /b 1
) else (
    echo [note] UPX не найден - пропускаю сжатие
)

echo [5/5] установщик (NSIS) ...
set "MAKENSIS="
if exist "C:\Program Files (x86)\NSIS\makensis.exe" set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
if defined MAKENSIS (
    if not exist "dist" mkdir "dist"
    "%MAKENSIS%" "installers\installer_xp.nsi" || exit /b 1
) else (
    echo [note] NSIS не найден - пропускаю сборку установщика
)

echo.
echo Built: %ROOT%\SuperLauncherNative_xp.exe
for %%A in ("%ROOT%\SuperLauncherNative_xp.exe") do echo Size: %%~zA bytes
for %%A in ("%ROOT%\dist\SuperLauncher_Setup_xp.exe") do if exist "%%A" echo Installer: %%~zA bytes
endlocal
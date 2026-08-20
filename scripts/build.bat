@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  SuperLauncherNative - сборка из модулей C++ + x64 ассемблер
REM  Требования: MSVC Build Tools (ml64, cl), Windows SDK
REM ============================================================
set "ROOT=%~dp0.."
cd /d "%ROOT%"

REM ---- найти последний установленный MSVC ----
set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if not exist "%VSDIR%" set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
if not exist "%VSDIR%" set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community"
if not exist "%VSDIR%" set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Professional"

set "MSVC="
for /f "delims=" %%d in ('dir /b /ad "%VSDIR%\VC\Tools\MSVC" 2^>nul') do set "MSVC=%VSDIR%\VC\Tools\MSVC\%%d"
if "%MSVC%"=="" (echo [error] MSVC not found & exit /b 1)

set "SDKVER="
for /f "delims=" %%d in ('dir /b /ad "C:\Program Files (x86)\Windows Kits\10\Include" 2^>nul') do set "SDKVER=%%d"
if "%SDKVER%"=="" for /f "delims=" %%d in ('dir /b /ad "C:\Program Files\Windows Kits\10\Include" 2^>nul') do set "SDKVER=%%d"

set "SDK=C:\Program Files (x86)\Windows Kits\10"
if not exist "%SDK%\Include\%SDKVER%\um" set "SDK=C:\Program Files\Windows Kits\10"

set "BIN=%MSVC%\bin\Hostx64\x64"
set "INCLUDE=%MSVC%\include;%SDK%\Include\%SDKVER%\ucrt;%SDK%\Include\%SDKVER%\um;%SDK%\Include\%SDKVER%\shared"
set "LIB=%MSVC%\lib\x64;%SDK%\Lib\%SDKVER%\ucrt\x64;%SDK%\Lib\%SDKVER%\um\x64"

if not exist "build_t\obj" mkdir "build_t\obj"

echo [1/3] compiling C++ modules ...
set "SRCS=src\core\json.cpp src\core\paths.cpp src\core\log.cpp src\core\config.cpp src\core\zip.cpp src\core\inflate.cpp src\crypto\sha1.c src\crypto\sha1file.cpp src\crypto\sha256.cpp src\platform\plat.cpp src\net\http.cpp src\net\ping.cpp src\minecraft\version.cpp src\minecraft\install.cpp src\minecraft\command.cpp src\minecraft\neoforge.cpp src\instances\instances.cpp src\backend\account.cpp src\backend\servers.cpp src\backend\updates.cpp src\backend\mods.cpp src\backend\ai.cpp src\ui\ui.cpp src\ui\pages\pages.cpp src\ui\pages\pages_util.cpp src\ui\pages\page_home.cpp src\ui\pages\page_account.cpp src\ui\pages\page_mods.cpp src\ui\pages\page_builds.cpp src\ui\pages\page_skins.cpp src\ui\pages\page_news.cpp src\ui\pages\page_updates.cpp src\ui\pages\page_servers.cpp src\ui\pages\page_settings.cpp src\ui\pages\page_launch.cpp src\ui\pages\page_ai.cpp src\ui\dialogs\dialogs.cpp main.cpp"
for %%s in (%SRCS%) do (
    "%BIN%\cl.exe" /nologo /c /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /Fo"build_t\obj\\" "%%s" || exit /b 1
)

echo [2/3] linking ...
"%BIN%\link.exe" /nologo /OUT:"SuperLauncherNative.exe" /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OPT:REF build_t\obj\*.obj user32.lib gdi32.lib comctl32.lib wininet.lib advapi32.lib shell32.lib ole32.lib comdlg32.lib || exit /b 1

echo [3/3] UPX-сжатие ...
set "SLUPX="
for /d %%p in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\UPX*") do set "UPXDIR=%%p"
if defined UPXDIR for /f "delims=" %%u in ('dir /s /b "%UPXDIR%\upx.exe" 2^>nul') do set "SLUPX=%%u"
if defined SLUPX (
    "%SLUPX%" --best --lzma "SuperLauncherNative.exe" || exit /b 1
) else (
    echo [note] UPX не найден - пропускаю сжатие
)

echo.
echo Built: %ROOT%\SuperLauncherNative.exe
for %%A in ("%ROOT%\SuperLauncherNative.exe") do echo Size: %%~zA bytes
endlocal
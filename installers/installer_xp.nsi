; SuperLauncher 2.0 - установщик для Windows XP (минимальный, NSIS 3.x)
; Сборка: "C:\Program Files (x86)\NSIS\makensis.exe" installers\installer_xp.nsi

!include "MUI2.nsh"

Name "SuperLauncher"
OutFile "..\dist\SuperLauncher_Setup_xp.exe"
InstallDir "$PROGRAMFILES\SuperLauncher"
InstallDirRegKey HKLM "Software\SuperLauncher" "InstallDir"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma
CRCCheck on
XPStyle on

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Russian"

Section "SuperLauncher" SecMain
    SetOutPath "$INSTDIR"
    File "..\SuperLauncherNative_xp.exe"

    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher" \
        "DisplayName" "SuperLauncher 2.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher" \
        "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher" \
        "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher" \
        "DisplayIcon" "$INSTDIR\SuperLauncherNative_xp.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher" \
        "Publisher" "SuperLauncherMC"

    CreateDirectory "$SMPROGRAMS\SuperLauncher"
    CreateShortCut "$SMPROGRAMS\SuperLauncher\SuperLauncher.lnk" "$INSTDIR\SuperLauncherNative_xp.exe"
    CreateShortCut "$DESKTOP\SuperLauncher.lnk" "$INSTDIR\SuperLauncherNative_xp.exe"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\uninstall.exe"
    Delete "$INSTDIR\SuperLauncherNative_xp.exe"
    RMDir /r "$INSTDIR\.sl"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\SuperLauncher\SuperLauncher.lnk"
    RMDir "$SMPROGRAMS\SuperLauncher"
    Delete "$DESKTOP\SuperLauncher.lnk"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SuperLauncher"
    DeleteRegKey HKLM "Software\SuperLauncher"
SectionEnd
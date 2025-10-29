@echo off

if "%1"=="-install" (
    call :install
    exit /b
)

if "%1"=="-uninstall" (
    call :uninstall
    exit /b
)

echo usage: deployment.bat [-install][-uninstall]
exit /b

:uninstall
sc delete inj
exit /b

:install
sc create inj binPath="@BIN_OUTPUT_DIR@\injdrv.sys" type=kernel
exit /b
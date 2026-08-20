@echo off
REM CJASTHelper Build System for Windows with MinGW clang++
REM Wrapper that calls the PowerShell build script

setlocal enabledelayedexpansion

if "%~1"=="" (
    powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" -h
    goto :EOF
)

REM Pass all arguments to PowerShell script
powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
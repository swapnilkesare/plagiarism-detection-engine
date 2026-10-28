@echo off
echo ========================================================
echo   Building Plagiarism Detection Engine (Pure C)
echo ========================================================

gcc -Wall -Wextra -O2 -std=gnu11 -I. -Ifrontend -Ibackend main.c frontend/frontend.c backend/backend.c backend/miniz.c -o plagiarism_detector.exe -lws2_32 -lwininet

if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Binary created: plagiarism_detector.exe
    echo Launch with: .\plagiarism_detector.exe
) else (
    echo [BUILD ERROR] Compilation failed.
)

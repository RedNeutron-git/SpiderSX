@echo off
REM Compile resource/link
rc resource.rc

REM Compile programm
cl stealersx.c resource.res /Fe:stealersx.exe /EHsc /O2 /MT /link user32.lib wininet.lib

if %errorlevel%==0 (
    echo Sukses
) else (
    echo Gagal
)

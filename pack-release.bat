@echo off
set OUTDIR=build
set PKGROOT=%OUTDIR%\sdp
set OUTFILE=%PKGROOT%.7z

del /f /q %OUTFILE%
rmdir /s /q %PKGROOT%
mkdir %PKGROOT%
copy /y %OUTDIR%\*.exe %PKGROOT%\
copy /y LICENSE %PKGROOT%\
copy /y README.md %PKGROOT%\

rem touch -m -r %OUTDIR%\sdp.exe %PKGROOT%
7z.exe a -t7z -mx -myx -mtr- -stl %OUTFILE% .\%PKGROOT%\

rmdir /s /q %PKGROOT%

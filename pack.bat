REM SPDX-FileCopyrightText: 2023 chrdev
REM
REM SPDX-License-Identifier: MIT

@echo off
set OUTDIR=build
set PKGROOT=%OUTDIR%\sdp
set OUTFILE=%PKGROOT%.7z

del /f /q %OUTFILE%
rmdir /s /q %PKGROOT%
mkdir %PKGROOT%
copy /y %OUTDIR%\*.exe %PKGROOT%\
copy /y LICENSES\MIT.txt %PKGROOT%\
copy /y README.md %PKGROOT%\

touch -m -r %OUTDIR%\sdp64-gcc.exe %PKGROOT%
7z.exe a -t7z -mx -myx -mtr- -stl %OUTFILE% .\%PKGROOT%\

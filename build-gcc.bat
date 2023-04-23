REM SPDX-FileCopyrightText: 2023 chrdev
REM
REM SPDX-License-Identifier: MIT

@echo off
set OUTDIR=build
set EXECLI64=sdp64-gcc.exe
set EXECLI32=sdp32-gcc.exe

set SRCCLI=src/shared/cap.c src/shared/uac.c src/shared/unit.c src/cli/cmd.c src/cli/sdp.c

set GCC64=x86_64-w64-mingw32-gcc.exe
set GCC32=i686-w64-mingw32-gcc.exe

set CFLAGS=-Wno-incompatible-pointer-types -s -O2 -D _UNICODE -D UNICODE
set LDFLAGS=-mwin32 -municode -mconsole

set ARGS64=%CFLAGS% -o %OUTDIR%/%EXECLI64% %SRCCLI% %LDFLAGS%
set ARGS32=%CFLAGS% -o %OUTDIR%/%EXECLI32% %SRCCLI% %LDFLAGS%

echo Building 64-bit binary...
%GCC64% %ARGS64%
echo Building 32-bit binary...
%GCC32% %ARGS32%
echo Done.

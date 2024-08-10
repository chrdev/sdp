@echo off
set OUTDIR=build
set EXECLI64=sdp.exe
set EXECLI32=sdp_x86.exe

set SRCCLI=src/common/cap.c src/common/uac.c src/common/unit.c src/common/multisz.c src/common/disk.c src/cli/cmd.c src/cli/sdp.c

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

REM SPDX-FileCopyrightText: 2023 chrdev
REM
REM SPDX-License-Identifier: MIT

@echo off

set AUTHOR=chrdev
set LIC=MIT
set LIC_CC=CC0-1.0

SET ADDLIC=reuse annotate --copyright=%AUTHOR% --license=%LIC% --merge-copyrights
set ADDLIC_CC=reuse annotate --copyright=%AUTHOR% --license=%LIC_CC% --merge-copyrights

%ADDLIC_CC% .gitignore
%ADDLIC% run-reuse.bat README.md build-gcc.bat pack.bat
cd src/shared
%ADDLIC% unit.c unit.h uac.c uac.h cap.c cap.h
cd ../cli
%ADDLIC% sdp.c cmd.c cmd.h
cd ../../vc17
%ADDLIC% --force-dot-license sdp.sln sdp.vcxproj sdp.vcxproj.filters
cd ..

if not exist LICENSES\ (
	reuse download --all
)

// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#include "uac.h"

#include <Windows.h>
#pragma comment(lib, "Advapi32.lib")


bool
uac_isElevated(void) {
	HANDLE h;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) return false;

	TOKEN_ELEVATION token = { 0 };
	GetTokenInformation(h, TokenElevation, &token, sizeof(token), &(DWORD){0});
	// TODO: deal with XP?
	CloseHandle(h);

	return token.TokenIsElevated;
}

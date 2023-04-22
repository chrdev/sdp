// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#include "cmd.h"

#include <stdbool.h>


static inline int
parseDriveId(const wchar_t* arg) {
	int n = 0;
	for (; *arg && *arg >= L'0' && *arg <= L'9'; ++arg) {
		n *= 10;
		n += *arg - L'0';
	}
	if (n >= cmd_kDriveCountLimit) n = -1;
	return n;
}

static void
addDrive(Cmd* cmd, const wchar_t* arg) {
	int n = parseDriveId(arg);
	if (n < 0) return;

	cmd->drives |= 1ULL << n;
}

static bool
doParseTimerNumber(uint32_t* v, const wchar_t** p) {
	uint64_t n = 0;
	const wchar_t* t = *p;
	for (; *t; ++t) {
		if (*t < L'0' || *t > L'9') break;
		n *= 10;
		n += *t - L'0';
		if (n > 0xFFFFFFFF) return false;
	}
	if (*p == t) return false;

	n *= 10;
	if (n > 0xFFFFFFFF) return false;

	*v = (uint32_t)n;
	*p = t;
	return true;
}

static bool
parseTimerNumber(uint32_t* timer, const wchar_t** p) {
	++*p;
	if (!**p) return false;
	return doParseTimerNumber(timer, p);
}

static bool
parseNextTimer(Cmd* cmd, const wchar_t** p) {
	switch (**p) {
	case L'i':
	case L'I':
	case L'a':
	case L'A':
		if (cmd->timerIdleA) return false;
		cmd->timerIdleA = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleA, p);
		break;
	case L'b':
	case L'B':
		if (cmd->timerIdleB) return false;
		cmd->timerIdleB = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleB, p);
		break;
	case L'c':
	case L'C':
		if (cmd->timerIdleC) return false;
		cmd->timerIdleC = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleC, p);
		break;
	case L'y':
	case L'Y':
		if (cmd->timerStandbyY) return false;
		cmd->timerStandbyY = 1;
		return parseTimerNumber(cmd->timers + unit_kStandbyY, p);
		break;
	case L'z':
	case L'Z':
	case L's':
	case L'S':
		if (cmd->timerStandbyZ) return false;
		cmd->timerStandbyZ = 1;
		return parseTimerNumber(cmd->timers + unit_kStandbyZ, p);
		break;
	default:
		return false;
	}
}

static void
parseTimers(Cmd* cmd, const wchar_t* t) {
	// t points to char next to 'w/W'
	cmd->timerMask = 0;
	bool ok = parseNextTimer(cmd, &t);
	while(ok && *t) {
		ok = parseNextTimer(cmd, &t);
	}
	cmd->intent = ok ? cmd_kTimerWrite : cmd_kTimerError;
}

static void
parseTimerIntent(Cmd* cmd, const wchar_t* t) {
	// t points to char next to leading 'w/W'
	switch (*t) {
	case L'\0':
		cmd->intent = cmd_kTimerHelp;
		break;
	case L'l':
	case L'L':
		cmd->intent = cmd_kTimerList;
		break;
	default:
		parseTimers(cmd, t);
		break;
	}
}

static void
parseIntent(Cmd* cmd, const wchar_t* arg) {
	// Eat leading '-' and '/'
	for (;; ++arg) {
		if (!*arg) return;
		if (*arg == L'-' || *arg == L'/') continue;
		break;
	}

	switch (*arg) {
	case L'l':
	case L'L':
		cmd->intent = cmd_kList;
		break;
	case L's':
	case L'S':
		cmd->intent = cmd_kStart;
		break;
	case L'p':
	case L'P':
		cmd->intent = cmd_kStop;
		break;
	case L'w':
	case L'W':
		parseTimerIntent(cmd, arg + 1);
		break;
	default:
		cmd->intent = cmd_kUnknown;
		break;
	}
}

void
cmd_parse(Cmd* cmd, int argc, const wchar_t* const* argv) {
	cmd->intent = cmd_kNone;
	cmd->drives = 0;

	for (int i = 1; i < argc; ++i) {
		wchar_t c = argv[i][0];
		if (c >= L'0' && c <= L'9') {
			addDrive(cmd, argv[i]);
		}
		else if (cmd->intent == cmd_kNone){
			parseIntent(cmd, argv[i]);
		}
		else {
			break;
		}
	}
}

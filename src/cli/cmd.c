#include "cmd.h"

#include <sdkddkver.h>
#include <Windows.h>

#include <stddef.h> // offsetof, GCC x686 requires
#include <assert.h>

#include "../common/disk.h" // dskid_parse
#include "../common/heap.h"


static const wchar_t* kEmptyTimerNumber = L"Timer without a number is not allowed.";


static bool
addDrive(Cmd* cmd, const wchar_t* arg, const wchar_t** errmsg) {
	static const wchar_t* kBadId = L"Unrecognized disk number.";

	int n = dskid_parse(arg);
	if (n < 0) {
		*errmsg = kBadId;
		return false;
	}

	cmd->diskIds[cmd->diskCount++] = (uint32_t)n;
	return true;
}

static bool
doParseTimerNumber(uint32_t* v, const wchar_t** p, const wchar_t** errmsg) {
	static const wchar_t* kBadNumRange = L"Too large timer number.";
	uint64_t n = 0;
	const wchar_t* t = *p;
	for (; *t; ++t) {
		if (*t < L'0' || *t > L'9') break;
		n *= 10;
		n += *t - L'0';
		if (n > 0xFFFFFFFF) goto err;
	}
	if (*p == t) {
		*errmsg = kEmptyTimerNumber;
		return false;
	}

	n *= 10;
	if (n > 0xFFFFFFFF) goto err;

	*v = (uint32_t)n;
	*p = t;
	return true;

err:
	*errmsg = kBadNumRange;
	return false;
}

static bool
parseTimerNumber(uint32_t* timer, const wchar_t** p, const wchar_t** errmsg) {
	++*p;
	if (!**p) {
		*errmsg = kEmptyTimerNumber;
		return false;
	}
	return doParseTimerNumber(timer, p, errmsg);
}

static bool
parseNextTimer(Cmd* cmd, const wchar_t** p, const wchar_t** errmsg) {
	static const wchar_t* kBadTimerId = L"Unrecognized timer condition identifier.";
	static const wchar_t* kDupItmerId = L"Duplicate timer condition identifiers not allowed.";

	switch (**p) {
	case L'i':
	case L'I':
	case L'a':
	case L'A':
		if (cmd->timerIdleA) goto err;
		cmd->timerIdleA = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleA, p, errmsg);
	case L'b':
	case L'B':
		if (cmd->timerIdleB) goto err;
		cmd->timerIdleB = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleB, p, errmsg);
	case L'c':
	case L'C':
		if (cmd->timerIdleC) goto err;
		cmd->timerIdleC = 1;
		return parseTimerNumber(cmd->timers + unit_kIdleC, p, errmsg);
	case L'y':
	case L'Y':
		if (cmd->timerStandbyY) goto err;
		cmd->timerStandbyY = 1;
		return parseTimerNumber(cmd->timers + unit_kStandbyY, p, errmsg);
	case L'z':
	case L'Z':
	case L's':
	case L'S':
		if (cmd->timerStandbyZ) goto err;
		cmd->timerStandbyZ = 1;
		return parseTimerNumber(cmd->timers + unit_kStandbyZ, p, errmsg);
	default:
		*errmsg = kBadTimerId;
		return false;
	}
	return true;

err:;
	*errmsg = kDupItmerId;
	return false;
}

static bool
parseTimers(Cmd* cmd, const wchar_t* t, const wchar_t** errmsg) {
	// t points to the char behind 'w/W'
	cmd->timerMask = 0;
	bool ok = parseNextTimer(cmd, &t, errmsg);
	while(ok && *t) {
		ok = parseNextTimer(cmd, &t, errmsg);
	}
	if (ok) cmd->intent = cmd_kTimerWrite;
	return ok;
}

static bool
parseTimerIntent(Cmd* cmd, const wchar_t* t, const wchar_t** errmsg) {
	// t points to the char behind 'w/W'
	switch (*t) {
	case L'\0':
		cmd->intent = cmd_kTimerHelp;
		break;
	case L'l':
	case L'L':
		cmd->intent = cmd_kTimerList;
		break;
	default:
		return parseTimers(cmd, t, errmsg);
		break;
	}
	return true;
}

static bool
parseIntent(Cmd* cmd, const wchar_t* arg, const wchar_t** errmsg) {
	static const wchar_t* kMultiIntent = L"Multiple commands not allowed.";

	if (cmd->intent != cmd_kNone) {
		*errmsg = kMultiIntent;
		return false;
	}

	// Eat leading '-' and '/'
	for (;; ++arg) {
		if (!*arg) return true; // Tolerate dangling '-'s and '/'s.
		if (*arg == L'-' || *arg == L'/') continue;
		break;
	}

	switch (*arg) {
	case L'l':
	case L'L':
		cmd->intent = cmd_kList;
		break;
	case L'p':
	case L'P':
		cmd->intent = cmd_kStop;
		break;
	case L'w':
	case L'W':
		return parseTimerIntent(cmd, arg + 1, errmsg);
		break;
	default:
		cmd->intent = cmd_kHelp;
		break;
	}
	return true;
}

static bool
validateIntent(Cmd* cmd, const wchar_t** errmsg) {
	static const wchar_t* kNoTarget = L"Must specify one or more disk numbers.";

	switch (cmd->intent) {
	case cmd_kNone:
		cmd->intent = cmd->diskCount ? cmd_kList : cmd_kHelp;
		break;
	case cmd_kTimerHelp:
		if (cmd->diskCount) cmd->intent = cmd_kTimerList;
		break;
	case cmd_kStop:
	case cmd_kTimerWrite:
		if (!cmd->diskCount) {
			*errmsg = kNoTarget;
			return false;
		}
		break;
	}
	return true;
}

Cmd*
cmd_parse(int argc, const wchar_t* const* argv, const wchar_t** errmsg)
{
	static const wchar_t* kLowMem = L"Low memory to parse command.";

	Cmd* cmd = heap_alloc(0, offsetof(Cmd, diskIds[(argc - 1)]));
	if (!cmd) {
		*errmsg = kLowMem;
		return NULL;
	}

	cmd->intent = cmd_kNone;
	cmd->diskCount = 0;

	for (int i = 1; i < argc; ++i) {
		wchar_t c = argv[i][0];
		if (c >= L'0' && c <= L'9') {
			if (!addDrive(cmd, argv[i], errmsg)) goto err;
		}
		else {
			if (!parseIntent(cmd, argv[i], errmsg)) goto err;
		}
	}
	if (!validateIntent(cmd, errmsg)) goto err;
	return cmd;

err:
	heap_free(0, cmd);
	return NULL;
}

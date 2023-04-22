// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#pragma once


#include <stdint.h>

#include "../shared/unit.h" // enum PowerConditon, union TimerMask


typedef enum Intent{
	cmd_kNone,
	cmd_kUnknown,
	cmd_kHelp,
	cmd_kList,
	cmd_kStart,
	cmd_kStop,
	cmd_kTimerHelp,
	cmd_kTimerList,
	cmd_kTimerWrite,
	cmd_kTimerError,
}Intent;

enum { cmd_kDriveCountLimit = 64 };

typedef struct Cmd {
	Intent intent;
	uint64_t drives;
	union TimerMask;
	uint32_t timers[unit_kPowerConditionCount];
}Cmd;

void
cmd_parse(Cmd* cmd, int argc, const wchar_t* const* argv);

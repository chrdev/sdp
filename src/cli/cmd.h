#pragma once

#include <stdint.h>
#include <wchar.h>

#include "../common/unit.h" // unit_kPowerConditionCount, union TimerMask


enum Intent{
	cmd_kNone,
	cmd_kHelp,
	cmd_kList,
	cmd_kStop,
	cmd_kTimerHelp,
	cmd_kTimerList,
	cmd_kTimerWrite,
};

typedef struct Cmd {
	enum Intent intent;
	union TimerMask;
	uint32_t timers[unit_kPowerConditionCount];
	uint32_t diskCount;
	uint32_t diskIds[1];
}Cmd;


Cmd*
cmd_parse(int argc, const wchar_t* const* argv, const wchar_t** errmsg);

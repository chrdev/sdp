// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#include <sdkddkver.h>
#include <Windows.h>
#include <strsafe.h>

#include <stdbool.h>

#include "cmd.h"
#include "../shared/uac.h"
#include "../shared/unit.h"
#include "../shared/cap.h"


#define MYNAME L"SDP - SCSI disk Power"
#define MYVER  L"1.0"
#define MYLIC  L"MIT License"
#define MYTITLE MYNAME L" " MYVER L"    " MYLIC L"\n"

#define SHOW_STATIC_TEXT(x) WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), x, _countof(x) - 1, &(DWORD){0}, NULL)


static const wchar_t kTextDone[] = L"Done\n";
static const wchar_t kTextFailed[] = L"Failed\n";
static const wchar_t kTextNoInfo[] = L"No Info\n";
static const wchar_t kTextNoDriveId[] = L"No specified driveID.\n";


static const wchar_t*
getPhyDriveName(BYTE id) {
	enum { kCch = 20 }; // sufficient for "\\.\PhysicalDrive##"
	static wchar_t t[kCch];
	StringCchPrintf(t, kCch, L"\\\\.\\PhysicalDrive%hhu", id);
	return t;
}

static HANDLE
getPhyDriveHandle(BYTE id) {
	const wchar_t* fn = getPhyDriveName(id);
	HANDLE h = CreateFile(
		fn,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	return h;
}

// Return pointer to inner static buffer
static const wchar_t*
getFormFactorText(UnitFormFactor f) {
	static const wchar_t* kText[] = {
		L"n/a",
		L"5.25",
		L"3.5",
		L"2.5",
		L"1.8",
		L"1.8-",
		L"oth",
	};
	return f < unit_kFormFactorOther ? kText[f] : kText[unit_kFormFactorOther];
}

// Retruns pointer to inner static buffer
static const wchar_t*
getRpmText(WORD rpm) {
	static wchar_t t[8];

	if (rpm == 1) {
		StringCchCopy(t, _countof(t), L"SSD");
	}
	else if (rpm >= 0x401 && rpm <= 0xFFFE) {
		StringCchPrintf(t, _countof(t), L"%hu", rpm);
	}
	else {
		StringCchCopy(t, _countof(t), L"n/a");
	}
	return t;
}

static inline void
showCommonHeader(void) {
	static const wchar_t kT[] =
		L"ID: FF   RPM   CAP  BS   Vendor   Product          Rev  Serial\n";
	SHOW_STATIC_TEXT(kT);
}

static inline void
showTimerHeader(void) {
	static const wchar_t kT[] =
		L"    Cond:Current/Mask(Hex)/Default ...\n";
	SHOW_STATIC_TEXT(kT);
}

static inline void
showHeaderSplitter(void) {
	static const wchar_t kT[] =
		L"--------------------------------------------------------------\n";
	SHOW_STATIC_TEXT(kT);
}

static void
showHeader(bool alt) {
	showCommonHeader();
	if (alt) showTimerHeader();
	showHeaderSplitter();
}

static inline void
showInfo(const UnitInfo* p) {
	const wchar_t* ff = getFormFactorText(p->formFactor);
	const wchar_t* rpm = getRpmText(p->rpm);
	wchar_t bs[5];
	cap_getShortText(p->blockSize, bs);
	const wchar_t* cap = cap_getShortText(p->blockCount * p->blockSize, NULL);
	wprintf_s(
		L"%-4ls %-5ls %-4ls %-4ls %-8ls %-16ls %-4ls %ls\n",
		ff, rpm, cap, bs, p->vendor, p->product, p->revision, p->serial
	);
}

// Return pointer to inner static buffer
//static const wchar_t*
//getTimerMaskText(BYTE powcon) {
//	static wchar_t t[unit_kPowerConditionCount + 1];
//	static const wchar_t kT[] = L"ABCYZ";
//	for (int i = 0; i < unit_kPowerConditionCount; ++i) {
//		t[i] = powcon & (1 << i) ? kT[i] : L'-';
//	}
//	t[unit_kPowerConditionCount] = L'\0';
//	return t;
//}

static inline WORD
swapColor(WORD attr) {
	WORD rlt = attr & 0xFF00;
	rlt |= (attr & 0x0F) << 4;
	rlt |= (attr & 0xF0) >> 4;
	return rlt;
}

static inline void
invertTextColor(void) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(h, &info);
	WORD attr = swapColor(info.wAttributes);
	SetConsoleTextAttribute(h, attr);
}

static void
showTimers(const UnitInfo* p) {
	static const wchar_t kT[] = L"ABCYZ";
	for (int i = 0; i < unit_kPowerConditionCount; ++i) {
		if (!(p->timerMask & 1 << i)) continue;
		wprintf_s(
			L" %lc:%u/%X/%u",
			kT[i], p->timers[i] / 10, p->timersModMask[i], p->timersDefault[i] / 10
		);
	}
}

static inline void
indent(void) {
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"   ", 3, &(DWORD){0}, NULL);
}

static inline void
newline(void) {
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"\n", 1, &(DWORD){0}, NULL);
}

static void
showDriveTimers(HANDLE h, UnitInfo* p) {
	unit_getTimers(h, p);
	indent();
	if (p->timerMask) {
		showTimers(p);
	}
	else {
		wprintf_s(L" -");
	}
	newline();
}

static void
showDriveInfo(HANDLE h, BYTE id, void* ex) {
	wprintf_s(L"%2hhu: ", id);

	UnitInfo d;
	if (unit_getInfo(h, &d)) {
		showInfo(&d);
		if (ex) showDriveTimers(h, &d);
	}
	else {
		wprintf_s(kTextNoInfo);
	}
}

static void
stopDrive(HANDLE h, BYTE id, void* ex) {
	showDriveInfo(h, id, 0);
	indent();
	wprintf_s(L" Stopping... ");
	bool ok = unit_stop(h);
	wprintf_s(ok ? kTextDone : kTextFailed);
}

static void
showStarting(BYTE id) {
	wprintf_s(L"%2hhu: Starting... ", id);
}

static void
startDrive(HANDLE h, BYTE id, void* ex) {
	bool ok = unit_start(h);
	if (ok) {
		wprintf_s(L"\r");
		showDriveInfo(h, id, 0);
		indent();
		wprintf_s(L" Started\n");
	}
	else {
		wprintf_s(kTextFailed);
	}
}

static void
writeTimers(HANDLE h, BYTE id, void* ex) {
	showDriveInfo(h, id, (void*)0);

	const Cmd* cmd = (const Cmd*)ex;
	indent();
	wprintf_s(L" Writing timers... ");
	bool ok = unit_setTimers(h, cmd->timerMask, cmd->timers);
	if (!ok) {
		wprintf_s(kTextFailed);
	}
	else {
		wprintf_s(L"Done. Check with \"SDP W %hhu\"\n", id);
	}
}

typedef void (*HandleDrive)(HANDLE h, BYTE id, void* ex);

static void
forEachDriveDo(uint64_t drives, const HandleDrive handleDrive, void* ex) {
	for (int i = 0; drives; ++i) {
		if (drives & 1) {
			if (handleDrive == startDrive) { 
				showStarting(i);
			}
			HANDLE h = getPhyDriveHandle(i);
			if (h != INVALID_HANDLE_VALUE) {
				handleDrive(h, i, ex);
				CloseHandle(h);
			}
		}
		drives >>= 1;
	}
}

static inline void
warn(void) {
	static const wchar_t t[] = L"Caution: this software can cause MAJOR DAMAGE to your DATA & DEVICES!";

	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(h, &info);
	WORD attr = BACKGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
	SetConsoleTextAttribute(h, attr);
	WriteConsoleW(h, t, _countof(t) - 1, &(DWORD){0}, NULL);
	SetConsoleTextAttribute(h, info.wAttributes);
	newline();
}

static void
help(void) {
	static const wchar_t kT[] =
		MYTITLE
		L"SDP [command] [driveID] [driveID] ...\n"
		L"Only one command is allowed, with optional leading - or /\n"
		L"Command is case-insensitive. Command and driveID(s) can appear in any order\n"
		L"Commands:\n"
		L"  L: List, can be omitted if specified driveID\n"
		L"  S: Start\n"
		L"  P: Stop\n"
		L"  W: Write power condition timer. \"SDP W\" to see more help\n"
		L"Examples:\n"
		L"  List all drives: SDP L\n"
		L"  List drive0 and drive2: SDP L 0 2\n"
		L"  Stop drive2 and drive3: SDP P 2 3\n";
	SHOW_STATIC_TEXT(kT);
}

static void
showTimerError(void) {
	static const wchar_t kT[] =
		L"Timers Syntax Error";
	newline();
	invertTextColor();
	SHOW_STATIC_TEXT(kT);
	invertTextColor();
	newline();
}

static void
timerHelp(bool alt) {
	static const wchar_t kT[] =
		MYTITLE
		L"SDP WL [driveID] [driveID] ...\n"
		L"  List power condition timers\n"
		L"  L can be omitted if specified driveID\n"
		L"SDP W[I|A#][B#][C#][Y#][S|Z#] [driveID] [driveID] ...\n"
		L"  Write power condition timers as # seconds\n"
		L"Example:\n"
		L"  Set drive5 Standby_Z timer to 7200 seconds: SDP 5 WZ7200\n"
		L"  Set drive3 Idle_A to 1800 and Standby_Z to 3600: SDP 3 Wa1800z3600\n"
		L"Power Consumption: Idle_A >= Idle_B >= Idle_C > Standby_Y >= Standby_Z\n"
		L"Caution:\n"
		L"  Prevent setting timers to aggressively low values, because\n"
		L"  frequent spin down/up or head unload/load cycles\n"
		L"  can do harm to your hard drive!\n";
	SHOW_STATIC_TEXT(kT);
	if (alt) showTimerError();
}

static void
adjustIntent(Cmd* cmd) {
	switch (cmd->intent) {
	case cmd_kUnknown:
		cmd->intent = cmd_kHelp;
		break;
	case cmd_kNone:
		cmd->intent = cmd->drives ? cmd_kList : cmd_kHelp;
		break;
	case cmd_kTimerHelp:
		if (cmd->drives) cmd->intent = cmd_kTimerList;
		break;
	}
}

static void
showPrivilegeError(void) {
	static const wchar_t kT[] =
		MYTITLE
		L"Requires elevation. Run as administrator.\n";
	SHOW_STATIC_TEXT(kT);
}

enum {
	kExitSuccess,
	kExitPrivilege,
	kExitDriveId,
	kExitSyntax,
};

int wmain(int argc, wchar_t** argv) {
	warn();

	Cmd cmd;
	cmd_parse(&cmd, argc, argv);
	adjustIntent(&cmd);

	switch (cmd.intent) {
	case cmd_kHelp:
		help();
		return kExitSuccess;
	case cmd_kTimerHelp:
		timerHelp(false);
		return kExitSuccess;
	case cmd_kTimerError:
		timerHelp(true);
		return kExitSyntax;
	}

	if (!uac_isElevated()) {
		showPrivilegeError();
		return kExitPrivilege;
	}

	bool alt = false;

	switch (cmd.intent) {
	case cmd_kTimerList:
		alt = true;
		// fall through
	case cmd_kList:
		showHeader(alt);
		if (!cmd.drives) cmd.drives = ~cmd.drives;
		forEachDriveDo(cmd.drives, showDriveInfo, (void*)alt);
		return kExitSuccess;
	}

	if (!cmd.drives) {
		SHOW_STATIC_TEXT(kTextNoDriveId);
		return kExitDriveId;
	}
	showHeader(0);
	switch (cmd.intent) {
	case cmd_kStop:
		forEachDriveDo(cmd.drives, stopDrive, 0);
		break;
	case cmd_kStart:
		forEachDriveDo(cmd.drives, startDrive, 0);
		break;
	case cmd_kTimerWrite:
		forEachDriveDo(cmd.drives, writeTimers, &cmd);
		break;
	}
	return kExitSuccess;
}

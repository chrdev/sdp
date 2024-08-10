#include <sdkddkver.h>
#include <Windows.h>
#include <strsafe.h>

#include <stdbool.h>

#include "cmd.h"
#include "../common/uac.h"
#include "../common/unit.h"
#include "../common/cap.h"
#include "../common/disk.h"
#include "../common/heap.h"


#define MYVER  L"1.10"
#define MYNAME L"SDP SCSI Disk Power"
#define MYLIC  L"MIT License"
#define MYTITLE MYNAME L" " MYVER L"          " MYLIC L"\n\n"

#define SHOW_STATIC_TEXT(x) WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), x, _countof(x) - 1, &(DWORD){0}, NULL)


static const wchar_t kTextDone[] = L"Done\n";
static const wchar_t kTextFailed[] = L"Failed\n";
static const wchar_t kTextNoInfo[] = L"No Info\n";


static inline void
indent(void) {
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"    ", 4, &(DWORD){0}, NULL);
}

static inline void
volIndent(void) {
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"         ", 9, &(DWORD){0}, NULL);
}

static inline void
newline(void) {
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"\n", 1, &(DWORD){0}, NULL);
}

static inline WORD
swapColor(WORD attr) {
	WORD rlt = attr & 0xFF00;
	rlt |= (attr & 0x0F) << 4;
	rlt |= (attr & 0xF0) >> 4;
	return rlt;
}

static inline void
invertStderrColor(void) {
	HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(h, &info);
	WORD attr = swapColor(info.wAttributes);
	SetConsoleTextAttribute(h, attr);
}

static void
showError(const wchar_t* msg) {
	if (!msg) msg = L"Unknown error.";

	invertStderrColor();
	fwprintf(stderr, L"%ls", msg);
	invertStderrColor();
}

static inline void
showAttrText(const wchar_t* t, DWORD cch, WORD attr) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(h, &info);
	SetConsoleTextAttribute(h, attr);
	WriteConsoleW(h, t, cch, &(DWORD){0}, NULL);
	SetConsoleTextAttribute(h, info.wAttributes);
	newline();
}

static inline void
warn(void) {
	static const wchar_t t[] = L"CAUTION: Improper usage may cause MAJOR DAMAGE to DATA/DEVICES!";
	static const WORD attr = BACKGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
	showAttrText(t, _countof(t) - 1, attr);
}

static inline void
showTitle(void) {
	static const wchar_t t[] = MYTITLE;
	SHOW_STATIC_TEXT(t);
}

static void
help(void) {
	static const wchar_t t[] =
		L"SDP [command] [diskNum] [diskNum] ...\n"
		L"Only one command is allowed, - or / is optional\n"
		L"Command is case-insensitive. Command and drive numbers can be input in any order\n"
		L"Commands:\n"
		L"  L: List, can be omitted if specified diskNum\n"
		L"  P: Stop\n"
		L"  W: Write power condition timer. Use \"SDP W\" for more help\n"
		L"Examples:\n"
		L"  List all drives: SDP L\n"
		L"  List drive0 and drive2: SDP L 0 2\n"
		L"  Stop drive2 and drive3: SDP P 2 3\n";
	SHOW_STATIC_TEXT(t);
}

static void
timerHelp() {
	static const wchar_t t[] =
		L"SDP WL [diskNum] [diskNum] ...\n"
		L"  List power condition timers\n"
		L"  L can be omitted if specified diskNum\n"
		L"SDP W[I|A#][B#][C#][Y#][S|Z#] [diskNum] [diskNum] ...\n"
		L"  Write power condition timers as # seconds\n"
		L"Example:\n"
		L"  Set drive5 Standby_Z timer to 7200 seconds: SDP 5 WZ7200\n"
		L"  Set drive3 Idle_A to 1800 and Standby_Z to 3600: SDP 3 Wa1800z3600\n"
		L"Power Consumption: Idle_A >= Idle_B >= Idle_C > Standby_Y >= Standby_Z\n"
		L"Caution:\n"
		L"  Avoid setting timers to excessively low values, because\n"
		L"  short spin down/up and head unload/load cycles\n"
		L"  can harm your hard drives!\n";
	SHOW_STATIC_TEXT(t);
}

static inline void
showPrivilegeTip(void) {
	static const wchar_t* t = L"TIP: Run as Administrator to do actual work.";
	showError(t);
}

static inline void
showPrivilegeError(void) {
	static const wchar_t* t = L"Please run as administrator.";
	showError(t);
}

// Return pointer to inner static buffer
static const wchar_t*
getFormFactorText(enum UnitFormFactor f) {
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
showHeader(bool hasTimer) {
	showCommonHeader();
	if (hasTimer) showTimerHeader();
	showHeaderSplitter();
}

static inline void
showInfo(const UnitInfo* p) {
	const wchar_t* ff = getFormFactorText(p->formFactor);
	const wchar_t* rpm = getRpmText(p->rpm);
	wchar_t bs[5];
	cap_getShortText(p->blockSize, bs);
	const wchar_t* cap = cap_getShortText(p->blockCount * p->blockSize, NULL);
	wprintf(
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

static void
showTimers(const UnitInfo* p) {
	static const wchar_t kT[] = L"ABCYZ";
	for (int i = 0; i < unit_kPowerConditionCount; ++i) {
		if (!(p->timerMask & 1 << i)) continue;
		wprintf(
			L"%lc:%u/%X/%u",
			kT[i], p->timers[i] / 10, p->timersModMask[i], p->timersDefault[i] / 10
		);
	}
}

static void
showDiskTimers(HANDLE h, UnitInfo* p) {
	unit_getTimers(h, p);
	indent();
	if (p->timerMask) {
		showTimers(p);
	}
	else {
		wprintf(L"-");
	}
	newline();
}

static inline void
showVolumeName(const VolumeInfo* vi) {
	wprintf(L"%ls", vi->name);
}

// Show the first mount point
static inline void
showMountPoint(const VolumeInfo* vi) {
	if (vi->mountPoints) wprintf(L" \"%ls\"", vi->mountPoints);
}

static inline void
showVolumeDisks(const VolumeInfo* vi) {
	if (!vi->diskCount) return;

	wprintf(L"%ls", L" Disks[");
	wprintf(L"%lu", vi->disks[0]);
	for (UINT32 i = 1; i < vi->diskCount; ++i) {
		wprintf(L" %u", vi->disks[i]);
	}
	wprintf(L"%ls", L"]");
}

static void
showSpannedVolume(const VolumeInfo* vi) {
	static const WORD kAttr = FOREGROUND_RED | FOREGROUND_GREEN;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(h, &info);
	SetConsoleTextAttribute(h, kAttr);
	
	showVolumeName(vi);
	SetConsoleTextAttribute(h, info.wAttributes);

	showMountPoint(vi);
	showVolumeDisks(vi);
}

static void
showSimpleVolume(const VolumeInfo* vi) {
	showVolumeName(vi);
	showMountPoint(vi);
}

static void
showVolumeInfo(const DiskInfo* di) {
	for (UINT32 i = 0; i < di->volumeCount; ++i) {
		const VolumeInfo* vi = di->volumes[i];

		volIndent();
		if (vi->diskCount > 1) {
			showSpannedVolume(vi);
		}
		else {
			showSimpleVolume(vi);
		}
		newline();
	}
}

static bool
showDiskInfo(DiskInfo* di, void* ex) {
	wprintf(L"%2u: ", di->id);

	UnitInfo d;
	if (unit_getInfo(di->handle, &d)) {
		showInfo(&d);
		if (ex) showDiskTimers(di->handle, &d);
		showVolumeInfo(di);
	}
	else {
		wprintf(kTextNoInfo);
	}
	return true;
}

static bool
stopDisk(DiskInfo* di, void* ex) {
	static const wchar_t* kInUse = L"Disk in use.";

	showDiskInfo(di, 0);
	indent();
	wprintf(L"Stopping... ");

	if (!dsk_eject(di)) {
		wprintf(kTextFailed);
		indent();
		showError(kInUse);
		return false;
	}

	if (!unit_stop(di->handle)) {
		wprintf(kTextFailed);
		return false;
	}

	wprintf(kTextDone);
	return true;
}

static bool
writeTimers(DiskInfo* di, void* ex) {
	showDiskInfo(di, 0);

	const Cmd* cmd = (const Cmd*)ex;
	indent();
	wprintf(L"Writing timers... ");
	const wchar_t* errmsg;
	bool ok = unit_setTimers(di->handle, cmd->timerMask, cmd->timers, &errmsg);
	if (!ok) {
		wprintf(kTextFailed);
		indent();
		showError(errmsg);
		return false;
	}

	wprintf(L"Done. Check with \"SDP W %u\"\n", di->id);
	return true;
}

typedef bool (*DiskHandler)(DiskInfo*, void*);

static bool
forEachDiskDo(DiskSet* ds, DiskHandler func, void* ex) {
	for (UINT32 i = 0; i < ds->count; ++i) {
		bool ok = func(ds->items[i], ex);
		newline();
		if (!ok) return false;
	}
	return true;
}

static wchar_t*
manuDosDevices(void) {
	DWORD cch = 20480; // Initial buffer size. will be doubled each time if seen not enough.
	wchar_t* p = heap_alloc(0, sizeof(*p) * cch);
	if (!p) return NULL;
	DWORD rcch = QueryDosDevice(NULL, p, cch);
	if (rcch) return p;

	while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		cch += cch;
		heap_free(0, p);
		p = heap_alloc(0, sizeof(*p) * cch);
		if (!p) return NULL;
		rcch = QueryDosDevice(NULL, p, cch);
		if (rcch) return p;
	}

	heap_free(0, p);
	return NULL;
}

static DiskSet*
createDiskSet(const Cmd* cmd, const wchar_t** errmsg) {
	static const wchar_t* kLowMem = L"Low memory to get device list.";

	wchar_t* dosDevices = manuDosDevices();
	if (!dosDevices) {
		*errmsg = kLowMem;
		return NULL;
	}
	DiskSet* ds = dskset_create(cmd->diskCount ? cmd->diskIds : NULL, cmd->diskCount, dosDevices, errmsg);
	heap_free(0, dosDevices);
	return ds;
}

enum {
	kExitSuccess,
	kExitFail,
	kExitCmd,
	kExitPrivilege,
	kExitDiskSet,
};

int wmain(int argc, wchar_t** argv)
{
	warn();
	showTitle();

	const wchar_t* errmsg = NULL;
	Cmd* cmd = cmd_parse(argc, argv, &errmsg);
	if (!cmd) {
		showError(errmsg);
		return kExitCmd;
	}

	bool isElevated = uac_isElevated();
	switch (cmd->intent) {
	case cmd_kHelp:
		help();
		if (!isElevated) showPrivilegeTip();
		return kExitSuccess;
	case cmd_kTimerHelp:
		timerHelp();
		if (!isElevated) showPrivilegeTip();
		return kExitSuccess;
	}

	if (!isElevated) {
		showPrivilegeError();
		return kExitPrivilege;
	}

	DiskSet* ds = createDiskSet(cmd, &errmsg);
	if (!ds) {
		showError(errmsg);
		return kExitDiskSet;
	}

	int ret = kExitSuccess;
	bool hasTimer = false;

	switch (cmd->intent) {
	case cmd_kTimerList:
		hasTimer = true;
		// fall through
	case cmd_kList:
		showHeader(hasTimer);
		forEachDiskDo(ds, showDiskInfo, (void*)hasTimer);
		break;
	case cmd_kStop:
		showHeader(false);
		if (!forEachDiskDo(ds, stopDisk, 0)) ret = kExitFail;
		break;
	case cmd_kTimerWrite:
		showHeader(false);
		if (!forEachDiskDo(ds, writeTimers, cmd)) ret = kExitFail;
		break;
	}

	dskset_destroy(ds);
	return ret;
}

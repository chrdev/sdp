// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#pragma once

#include <Windows.h>

#include <stdbool.h>
#include <stdint.h>


enum {
	unit_kLenVendorId = 8,
	unit_kCchVendorId,
	unit_kLenProductId = 16,
	unit_kCchProductId,
	unit_kLenRevision = 4,
	unit_kCchRevision,

	unit_kLenSerial = 48, // no limit by standard, but guess 48 should be enough
	unit_kCchSerial,
};

typedef enum PowerConditon {
	unit_kIdle,
	unit_kIdleA = unit_kIdle,
	unit_kIdleB,
	unit_kIdleC,
	unit_kStandbyY,
	unit_kStandbyZ,
	unit_kStandby = unit_kStandbyZ,
	unit_kPowerConditionCount,
}PowerCondition;

typedef enum UnitFormFactor {
	unit_kFormFactorNA,
	unit_kFormFactor525,
	unit_kFormFactor35,
	unit_kFormFactor25,
	unit_kFormFactor18,
	unit_kFormFactor18minus,
	unit_kFormFactorOther,
}UnitFormFactor;

typedef union TimerMask {
	struct {
		BYTE timerIdleA : 1;
		BYTE timerIdleB : 1;
		BYTE timerIdleC : 1;
		BYTE timerStandbyY : 1;
		BYTE timerStandbyZ : 1;
	};
	BYTE timerMask;
}TimerMask;

typedef struct UnitInfo {
	DWORD blockSize;
	uint64_t blockCount;
	wchar_t vendor[unit_kCchVendorId];
	wchar_t product[unit_kCchProductId];
	wchar_t revision[unit_kCchRevision];
	wchar_t serial[unit_kCchSerial];
	UnitFormFactor formFactor;
	WORD rpm;
	union TimerMask;
	bool timerWritable;
	DWORD timers[unit_kPowerConditionCount];
	DWORD timersModMask[unit_kPowerConditionCount];
	DWORD timersDefault[unit_kPowerConditionCount];
}UnitInfo;

bool
unit_start(HANDLE h);

bool
unit_stop(HANDLE h);

// Get basic info without timers.
// If want timers, call unit_getTimers
bool
unit_getInfo(HANDLE h, UnitInfo* info);

// Get timers without basic info
// If want basic info, call unit_getInfo.
// This function resets info.timerMask even if failed
bool
unit_getTimers(HANDLE h, UnitInfo* info);

bool
unit_setTimers(HANDLE h, BYTE mask, const DWORD timers[unit_kPowerConditionCount]);

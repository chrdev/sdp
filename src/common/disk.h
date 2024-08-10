#pragma once

#include <sdkddkver.h>
#include <windows.h>

#include <stdbool.h>


typedef struct VolumeInfo {
	HANDLE handle;
	wchar_t name[46]; // 45 is for "Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}". Add 1 for padding.
	bool isLocked;
	wchar_t* mountPoints;
	UINT32 diskCount;
	UINT32 disks[1]; // Disk numbers as in "PhysicalDrive#"
}VolumeInfo;

typedef struct VolumeSet {
	UINT32 count;
	VolumeInfo** items;
}VolumeSet;


typedef struct DiskInfo {
	HANDLE handle;
	UINT32 id;
	// wchar_t name[28]; // 28 is to hold "\\.\PhysicalDrive##########" with 10 digits (enough for UINT32).
	UINT32 volumeCount;
	VolumeInfo* volumes[1];
}DiskInfo;

typedef struct DiskSet {
	VolumeSet* volumeSet;
	UINT32 count;
	DiskInfo** items;
}DiskSet;


int
dskid_parse(const wchar_t* t);

void
dskset_destroy(DiskSet* s);

DiskSet*
dskset_create(const UINT32* diskIds, size_t count, const wchar_t* dosDevices, const wchar_t** errmsg);

// Do 3 things to related volumes in order: // 1. Lock; 2. Dismount; 3. Offline.
bool
dsk_eject(DiskInfo* di);

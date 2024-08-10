#include "disk.h"

#include <strsafe.h>
#pragma comment(lib, "strsafe.lib")

#include <stddef.h> // offsetof. GCC i686 requires this
#include <assert.h>

#include "multisz.h"
#include "heap.h"


// Return: If successful, return physical drive id, which is >=0.
//         If failed, return a negative number.
int
dskid_parse(const wchar_t* t)
{
	int n = 0;
	for (; *t; ++t) {
		if (*t < L'0' || *t > L'9') return -1;
		int orig = n;
		n *= 10;
		n += *t - L'0';
		if (n < orig) return -1;
	}
	return n;
}


static HANDLE
openDevice(const wchar_t* dosDeviceName) {
	wchar_t name[MAX_PATH];
	HRESULT hr = StringCchPrintf(name, ARRAYSIZE(name), L"\\\\.\\%ls", dosDeviceName);
	if (FAILED(hr)) return INVALID_HANDLE_VALUE;

	HANDLE h = CreateFile(
		name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL
	);
	return h;
}

// If successful, return buffer pointer. User must call HeapFree(GetProcessHeap(),...) after use.
// If failed, return NULL.
static VOLUME_DISK_EXTENTS*
vol_manuDiskExtents(HANDLE h) {
	VOLUME_DISK_EXTENTS* de = heap_alloc(0, sizeof(*de));
	if (!de) return NULL;

	DWORD cb;
	BOOL ok = DeviceIoControl(
		h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
		de, sizeof(*de), &cb, NULL
	);
	if (ok) return de;

	while (GetLastError() == ERROR_MORE_DATA) {
		size_t sz = offsetof(VOLUME_DISK_EXTENTS, Extents[de->NumberOfDiskExtents]);
		heap_free(0, de);
		de = heap_alloc(0, sz);
		if (!de) return NULL;
		ok = DeviceIoControl(
			h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
			de, (DWORD)sz, &cb, NULL
		);
		if (ok) return de;
	}

	heap_free(0, de);
	return NULL;
}

static wchar_t*
vol_manuMountPoints(const wchar_t* volName) {
	wchar_t path[50]; // 50 is enough for "\\?\Volume{GUID}\";
	HRESULT hr = StringCchPrintf(path, ARRAYSIZE(path), L"\\\\?\\%ls\\", volName);
	if (FAILED(hr)) return NULL;

	DWORD cch = 0;
	BOOL rc = GetVolumePathNamesForVolumeName(path, NULL, 0, &cch);
	if (GetLastError() != ERROR_MORE_DATA) return NULL;
	if (cch <= 1) return NULL;

	wchar_t* buf = heap_alloc(0, sizeof(*buf) * cch);
	if (!buf) return NULL;
	rc = GetVolumePathNamesForVolumeName(path, buf, cch, &cch);
	if (!rc) {
		heap_free(0, buf);
		return NULL;
	}
	return buf;
}

static void
fillVolumeInfo(VolumeInfo* vi, HANDLE h, const wchar_t* name, const VOLUME_DISK_EXTENTS* de) {
	vi->handle = h;
	StringCchCopy(vi->name, ARRAYSIZE(vi->name), name);
	vi->isLocked = false;
	vi->mountPoints = vol_manuMountPoints(name);
	vi->diskCount = de->NumberOfDiskExtents;
	for (UINT32 i = 0; i < vi->diskCount; ++i) {
		vi->disks[i] = de->Extents[i].DiskNumber;
	}
}

static VolumeInfo*
vol_manuInfo(const wchar_t* dosDeviceName) {
	HANDLE h = openDevice(dosDeviceName);
	if (h == INVALID_HANDLE_VALUE) return NULL;

	VOLUME_DISK_EXTENTS* de = vol_manuDiskExtents(h);
	if (!de) {
		CloseHandle(h);
		return NULL;
	}

	VolumeInfo* vi = heap_alloc(0, offsetof(VolumeInfo, disks[de->NumberOfDiskExtents]));
	if (!vi) {
		CloseHandle(h);
		heap_free(0, de);
		return NULL;
	}

	fillVolumeInfo(vi, h, dosDeviceName, de);
	return vi;
}

static inline bool
vol_lock(HANDLE h) {
	return DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &(DWORD){0}, NULL);
}

static inline bool
vol_dismount(HANDLE h) {
	return DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &(DWORD){0}, NULL);
}

static inline bool
vol_offline(HANDLE h) {
	return DeviceIoControl(h, IOCTL_VOLUME_OFFLINE, NULL, 0, NULL, 0, &(DWORD){0}, NULL);
}

static void
volset_destroy(VolumeSet* s) {
	if (!s) return;

	for (UINT32 i = 0; i < s->count; ++i) {
		VolumeInfo* info = s->items[i];
		CloseHandle(info->handle);
		if (info->mountPoints) heap_free(0, info->mountPoints);
		heap_free(0, info);
	}
	heap_free(0, s);
}


static UINT
getDosDeviceType(const wchar_t* dosDevice) {
	wchar_t path[MAX_PATH];
	HRESULT hr = StringCchPrintf(path, ARRAYSIZE(path), L"\\\\?\\%ls\\", dosDevice);
	if (FAILED(hr)) return DRIVE_UNKNOWN;
	return GetDriveType(path);
}

static VolumeSet*
createEmptyVolumeSet(size_t itemCount) {
	VolumeSet* s = heap_alloc(0, sizeof(*s));
	if (!s) return NULL;
	s->items = heap_alloc(0, sizeof(s->items[0]) * itemCount);
	if (!s->items) {
		heap_free(0, s);
		return NULL;
	}

	s->count = 0;
	return s;
}

// Filter given volume names, only volumes that are DRIVE_FIXED type will be add to set.
// Param VolumeNames: May has the form of "Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}", as returned by QueryDosDevice().
static VolumeSet*
volset_createFromNames(const wchar_t** volumeNames, size_t count) {
	assert(volumeNames);

	VolumeSet* s = createEmptyVolumeSet(count);
	if (!s) return NULL;

	for (size_t i = 0; i < count; ++i) {
		const wchar_t* name = *volumeNames++;
		if (getDosDeviceType(name) != DRIVE_FIXED) continue;
		
		VolumeInfo* info = vol_manuInfo(name);
		if (!info) continue;
		s->items[s->count] = info;
		++s->count;
	}

	return s;
}

static VolumeSet*
volset_createFromDosDevices(const wchar_t* dosDevices) {
	size_t count;
	const wchar_t** names = msz_manuStringListStartsWith(&count, L"Volume", dosDevices);
	if (!names) return NULL;

	VolumeSet* vs = volset_createFromNames(names, count);
	heap_free(0, names);
	
	return vs;
}

void
dskset_destroy(DiskSet* s)
{
	if (!s) return;

	volset_destroy(s->volumeSet);
	for (UINT32 i = 0; i < s->count; ++i) {
		DiskInfo* info = s->items[i];
		CloseHandle(info->handle);
		heap_free(0, info);
	}
	heap_free(0, s);
}

static DiskSet*
createEmptyDiskSet(size_t itemCount) {
	DiskSet* s = heap_alloc(0, sizeof(*s));
	if (!s) return NULL;
	s->items = heap_alloc(0, sizeof(s->items[0]) * itemCount);
	if (!s->items) {
		heap_free(0, s);
		return NULL;
	}

	s->volumeSet = NULL;
	s->count = 0;
	return s;
}

static HANDLE
openDisk(UINT32 id) {
	wchar_t name[28]; // 28 is to hold "\\.\PhysicalDrive##########" with 10 digits(enough for UINT32).
	HRESULT hr = StringCchPrintf(name, ARRAYSIZE(name), L"\\\\.\\PhysicalDrive%u", id);
	if (FAILED(hr)) return INVALID_HANDLE_VALUE;

	return openDevice(name);
}

static size_t
getVolumesOnDisk(const VolumeInfo** info, size_t infoSize, const VolumeSet* vs, UINT32 diskId) {
	if (!vs) return 0;

	size_t c = 0;
	for (size_t i = 0; i < vs->count; ++i) {
		const VolumeInfo* vi = vs->items[i];
		for (size_t j = 0; j < vi->diskCount; ++j) {
			if (vi->disks[j] == diskId) {
				++c;
				if (info) {
					if (c > infoSize) return 0;
					*info++ = vi;
				}
			}
		}
	}
	return c;
}

static DiskInfo*
dsk_manuInfo(UINT32 id, const VolumeSet* vs) {
	HANDLE h = openDisk(id);
	if (h == INVALID_HANDLE_VALUE) return NULL;

	size_t volCount = getVolumesOnDisk(NULL, 0, vs, id);
	size_t sz = offsetof(DiskInfo, volumes[volCount]);
	DiskInfo* info = heap_alloc(0, sz);
	if (!info) {
		CloseHandle(h);
		return NULL;
	}

	info->handle = h;
	info->id = id;
	info->volumeCount = (UINT32)volCount;
	if (volCount) getVolumesOnDisk(&info->volumes[0], volCount, vs, id);
	
	return info;
}

static DiskSet*
dskset_createFromIds(const UINT32* ids, size_t count, const wchar_t* dosDevices) {
	assert(ids);
	assert(count);
	assert(dosDevices);

	DiskSet* s = createEmptyDiskSet(count);
	if (!s) return NULL;

	s->volumeSet = volset_createFromDosDevices(dosDevices);
	// volumeSet allowed to be NULL.
	for (size_t i = 0; i < count; ++i) {
		DiskInfo* info = dsk_manuInfo(*ids++, s->volumeSet);
		if (!info) {
			dskset_destroy(s);
			return NULL;
		}
		s->items[s->count++] = info;
	}

	return s;
}

static UINT32*
manuDiskIds(const wchar_t** names, size_t count) {
	assert(names);
	assert(count);

	UINT32* ids = heap_alloc(0, sizeof(*ids) * count);
	if (!ids) return NULL;
	for (size_t i = 0; i < count; ++i) {
		int id = dskid_parse((*names) + 13); // 13 is the length of "PhysicalDrive".
		if (id < 0) {
			heap_free(0, ids);
			return NULL;
		}
		ids[i] = (UINT32)id;
		++names;
	}
	return ids;
}

static bool
hasDupIds(const UINT32* ids, size_t count) {
	for (size_t i = 0; i < count - 1; ++i) {
		for (size_t j = i + 1; j < count; ++j) {
			if (ids[i] == ids[j]) return true;
		}
	}
	return false;
}

// Check whether each and every lids exists in rids.
static bool
hasNonexistentId(const UINT32* lids, size_t lcount, const UINT32* rids, size_t rcount) {
	for (size_t i = 0; i < lcount; ++i) {
		bool found = false;
		for (size_t j = 0; j < rcount; ++j) {
			if (lids[i] == rids[j]) {
				found = true;
				break;
			}
		}
		if (!found) return true;
	}
	return false;
}

// Given diskIds considered valid if the following conditions are all met:
//   1. Given idCount is less or equal to nameCount.
//   2. No duplicates in diskIds.
//   3. Each and every diskIds exists in given names.
// Return: If diskIds checked valid, return diskIds back.
//         If diskIds checked not valid, return NULL.
//         If diskIds is NULL, return pointer to ids buffer generated from names. User must call HeapFree() after use.
static UINT32*
validateDiskIds(const UINT32* diskIds, size_t idCount, const wchar_t** names, size_t nameCount, const wchar_t** errmsg) {
	static const wchar_t* kBadIdCount = L"Disk numbers exceeds physical drive count.";
	static const wchar_t* kDupIds = L"Duplicate disk numbers not allowed.";
	static const wchar_t* kLowMem = L"Low memory to generate disk number list.";
	static const wchar_t* kBadId = L"No such physical drive number.";

	if (diskIds) {
		if (idCount > nameCount) {
			*errmsg = kBadIdCount;
			return NULL;
		}
		if (hasDupIds(diskIds, idCount)) {
			*errmsg = kDupIds;
			return NULL;
		}
	}

	UINT32* nameIds = manuDiskIds(names, nameCount);
	if (!nameIds) {
		*errmsg = kLowMem;
		return NULL;
	}
	if (!diskIds) return nameIds;

	UINT32* rlt = (UINT32*)diskIds;
	if (hasNonexistentId(diskIds, idCount, nameIds, nameCount)) {
		*errmsg = kBadId;
		rlt = NULL;
	}
	heap_free(0, nameIds);
	return rlt;
}

DiskSet*
dskset_create(const UINT32* diskIds, size_t count, const wchar_t* dosDevices, const wchar_t** errmsg)
{
	static const wchar_t* kNoPhyDrive = L"No physical drive.";

	size_t nameCount;
	const wchar_t** names = msz_manuStringListStartsWith(&nameCount, L"PhysicalDrive", dosDevices);
	if (!names) {
		*errmsg = kNoPhyDrive;
		return NULL;
	}

	UINT32* ids = validateDiskIds(diskIds, count, names, nameCount, errmsg);
	heap_free(0, names);
	if (!ids) return NULL;
	
	if (ids != diskIds) count = nameCount;
	DiskSet* ds = dskset_createFromIds(ids, count, dosDevices);

	if (ids != diskIds) heap_free(0, ids);
	return ds;
}

bool
dsk_eject(DiskInfo* di)
{
	for (UINT32 i = 0; i < di->volumeCount; ++i) {
		VolumeInfo* vi = di->volumes[i];
		if (vi->isLocked) continue;

		vi->isLocked = vol_lock(vi->handle);
		if (!vi->isLocked) return false;

		if (vi->mountPoints) vol_dismount(vi->handle);
		vol_offline(vi->handle);
	}
	return true;
}

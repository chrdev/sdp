#include "unit.h"

#include <winioctl.h>
#define _NTSCSI_USER_MODE_
#if defined(__GNUC__)
#include <ddk/scsi.h>
#else
#include <scsi.h>
#include <intrin.h> // _byteswap_*
#endif
#undef _NTSCSI_USER_MODE_
#include <ntddscsi.h>

#include <strsafe.h>


enum {
	kTimeOut = 60,
};

typedef enum ModeType {
	kModeCurrent,
	kModeChangeable,
	kModeDefault,
	KModeSaved
}ModeType;

#pragma pack(push, scsidata, 1)
// P.208, sbc4r22.pdf - 5.20.2 READ CAPACITY (10) parameter data
typedef struct ReadCapacityData10 {
	DWORD lbLast;
	DWORD lbSize;
}ReadCapacityData10;

// P.210, sbc4r22.pdf - 5.21.2 READ CAPACITY (16) parameter data
typedef struct ReadCapacityData16 {
	uint64_t lbLast;
	uint32_t lbSize;
	BYTE prot_en : 1;
	BYTE p_type : 3;
	BYTE zbc2Restricted : 2;
	BYTE reserved12 : 2;
	BYTE logBlkPerPhyBlkExp : 4;
	BYTE p_i_exponent : 4;
	BYTE lowestAlignedLogBlkAddrHi : 6;
	BYTE lbprz : 1;
	BYTE lbpme : 1;
	BYTE lowestAlignedLogBlkAddrLo;
	BYTE reserved16[16];
}ReadCapacityData16;

// P.209, sbc4r22.pdf - 5.21 READ CAPACITY (16) command
typedef struct Cdb16ServiceActionIn {
	BYTE operationCode;
	BYTE serviceAction : 5;
	BYTE reserved1 : 3;
	BYTE obsolete2[8];
	BYTE allocationLength[4];
	BYTE reserved14;
	BYTE control;
}Cdb16ServiceActionIn;

// P.289, spc5r22.pdf - 6.7.2 Standard INQUIRY data
typedef struct StandardInquiryData {
	BYTE type : 5;
	BYTE qualifier : 3;
	BYTE reserved1 : 6;
	BYTE isConglomerate : 1;
	BYTE isRemovable : 1;
	BYTE version;
	BYTE responseDataFormat : 4;
	BYTE supportHistoricalLUN : 1;
	BYTE supportNormACA : 1;
	BYTE reserved3 : 2;
	BYTE additionalLength;
	// byte-5
	BYTE supportProtection : 1;
	BYTE reserved5 : 2;
	BYTE support3rdPartyCopy : 1;
	BYTE TPGS : 2;
	BYTE obsolete5 : 1;
	BYTE supportSCC : 1;
	// byte-6
	BYTE reserved6_0 : 4;
	BYTE withMultiPorts : 1;
	BYTE vs6 : 1;
	BYTE hasEnclosureServices : 1;
	BYTE obsolete6_7 : 1;
	// byte-7
	BYTE vs7 : 1;
	BYTE supportCommandQueue : 1;
	BYTE reserved7 : 6;
	// byte-8
	BYTE vendorId[8];
	BYTE productId[16];
	BYTE revision[4];
}StandardInquiryData;

// P.744, spc5r22.pdf - 7.7.19 Unit Serial Number VPD page
typedef struct SerialNumberData {
	BYTE deviceType : 5;
	BYTE qualifier : 3;
	BYTE pageCode;
	BYTE pageLength[2];
	BYTE serialNumber[1];
}SerialNumberData;

// P.370, sbc4r22.pdf - 6.6.2 Block Device Characteristics VPD page
typedef struct CharacteristicsData {
	BYTE deviceType : 5;
	BYTE qualifier : 3;
	BYTE pageCode; // 0xB1
	BYTE pageLength[2];
	WORD rpm;
	BYTE type;
	BYTE formFactor : 4;
	BYTE waceReq : 2; // write after cryptographic erase required
	BYTE wabeReq : 2; // write after block erase required
	// byte-8
	BYTE vbuls : 1; // verify byte check unmapped LBA supported
	BYTE fuab : 1; // force unit access behavior
	BYTE bocs : 1; // background operation control supported
	BYTE rbwz : 1; // reassign blocks write zero
	BYTE zoned : 2;
	BYTE reserved8 : 2;
	BYTE reserved9[3];
	DWORD depopulationTime;
	BYTE reserved[48];
}CharacteristicsData;

// P.624, spc5r22.pdf - Table 444 - Mode parameter header(10)
typedef struct ModeHeader10 {
	BYTE modeDataLength[2];
	BYTE mediumType;
	BYTE deviceParameter; // See P.342, sbc4r22.pdf - Table 230 - DEVICE-SPECIFIC PARAMETER field for direct access block devices
	BYTE longLba : 1;
	BYTE mh_reserved4 : 7;
	BYTE mh_reserved5;
	BYTE blockDescriptorLength[2];
}ModeHeader10;

// P.623, spc5r22.pdf - 7.5.6 Mode parameter header formats
typedef struct ModeHeader6 {
	BYTE modeDataLength;
	BYTE mediumType;
	BYTE deviceParameter; // See P.342, sbc4r22.pdf - Table 230 - DEVICE-SPECIFIC PARAMETER field for direct access block devices
	BYTE blockDescriptorLength;
}ModeHeader6;

// P642, spc5r22.pdf - 7.5.16 Power Condition mode page
#define POWER_CONDITION_MODE_PAGE_MEMBERS 	BYTE pageCode : 6;/* 0x1A */\
	BYTE subPageFormat : 1;\
	BYTE parametersSaveable : 1;\
	BYTE pageLength;/* 0x26, 38 */\
	BYTE standbyY : 1;\
	BYTE reserved2 : 5;\
	BYTE pmBgPrecedence : 2;\
	BYTE standbyZ : 1;\
	BYTE idleA : 1;\
	BYTE idleB : 1;\
	BYTE idleC : 1;\
	BYTE reserved3 : 4;\
	DWORD timerIdleA;\
	DWORD timerStandbyZ;\
	DWORD timerIdleB;\
	DWORD timerIdleC;\
	DWORD timerStandbyY;\
	BYTE reserved24[15];\
	BYTE reserved39 : 2;\
	BYTE ccfStopped : 2;\
	BYTE ccfStandby : 2;\
	BYTE ccfIdle : 2

typedef struct PowerConditionModePage {
	POWER_CONDITION_MODE_PAGE_MEMBERS;
}PowerConditionModePage;

#define POWER_CONDITION_MODE_PAGE_UNION union {\
	struct {\
		POWER_CONDITION_MODE_PAGE_MEMBERS;\
	};\
	PowerConditionModePage modePage;\
}

typedef struct PowerConditionData10 {
	ModeHeader10;
	POWER_CONDITION_MODE_PAGE_UNION;
}PowerConditionData10;

typedef struct PowerConditionData6 {
	ModeHeader6;
	POWER_CONDITION_MODE_PAGE_UNION;
}PowerConditionData6;

typedef struct Cdb10ModeSense {
	BYTE operationCode; // 0x5A
	BYTE reserved1 : 3;
	BYTE disableBlockDescriptors : 1;
	BYTE longLbaAccepted : 1;
	BYTE reserved1_2 : 3;
	BYTE pageCode : 6;
	BYTE pageControl : 2;
	BYTE subPageCode;
	BYTE reserved4[3];
	BYTE allocLength[2];
	BYTE control;
}Cdb10ModeSense;

typedef struct Cdb6ModeSense {
	BYTE operationCode; // 0x1A
	BYTE reserved1 : 3;
	BYTE disableBlockDescriptors : 1;
	BYTE reserved1_2 : 4;
	BYTE pageCode : 6;
	BYTE pageControl : 2;
	BYTE subPageCode;
	BYTE allocLength;
	BYTE control;
}Cdb6ModeSense;

typedef struct Cdb10ModeSelect {
	BYTE operationCode; // 0x55
	BYTE savePages : 1;
	BYTE revertToDefaults : 1;
	BYTE reserved1_1 : 2;
	BYTE pageFormat : 1;
	BYTE reserved1_2 : 3;
	BYTE reserved2[5];
	BYTE parameterListLength[2];
	BYTE control;
}Cdb10ModeSelect;

typedef struct Cdb6ModeSelect {
	BYTE operationCode; // 0x15
	BYTE savePages : 1;
	BYTE revertToDefaults : 1;
	BYTE reserved1_1 : 2;
	BYTE pageFormat : 1;
	BYTE reserved1_2 : 3;
	BYTE reserved2[2];
	BYTE parameterListLength;
	BYTE control;
}Cdb6ModeSelect;
#pragma pack(pop, scsidata)

// This function has no practical use. So it's commented out.
//bool
//unit_start(HANDLE h) {
//	SCSI_PASS_THROUGH spt = {
//		.Length = sizeof(SCSI_PASS_THROUGH),
//		.DataIn = SCSI_IOCTL_DATA_OUT,
//		.TimeOutValue = kTimeOut,
//		.CdbLength = CDB6GENERIC_LENGTH,
//		.Cdb[0] = SCSIOP_START_STOP_UNIT,
//		.Cdb[4] = 1,
//	};
//
//	DWORD cb = 0;
//	return DeviceIoControl(
//		h, IOCTL_SCSI_PASS_THROUGH,
//		&spt, sizeof(SCSI_PASS_THROUGH),
//		&spt, sizeof(SCSI_PASS_THROUGH),
//		&cb, FALSE
//	);
//}

bool
unit_stop(HANDLE h)
{
	SCSI_PASS_THROUGH spt = {
		.Length = sizeof(SCSI_PASS_THROUGH),
		.DataIn = SCSI_IOCTL_DATA_OUT,
		.TimeOutValue = kTimeOut,
		.CdbLength = CDB6GENERIC_LENGTH,
		.Cdb[0] = SCSIOP_START_STOP_UNIT,
	};

	DWORD cb = 0;
	return DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH,
		&spt, sizeof(SCSI_PASS_THROUGH),
		&spt, sizeof(SCSI_PASS_THROUGH),
		&cb, FALSE
	);
}

#ifdef _DEBUG
static bool
dump(const wchar_t* path, const BYTE* bin, DWORD size) {
	HANDLE f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) return false;

	DWORD cb;
	BOOL ok = WriteFile(f, bin, size, &cb, NULL);

	CloseHandle(f);
	return ok;
}
#endif // _DEBUG

// Since CreateFile activates the drive, it's pointless to check power condition using this function
//static const SENSE_DATA*
//getSense(HANDLE h) {
//	static SENSE_DATA data;
//
//	SCSI_PASS_THROUGH_DIRECT sptd = {
//		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
//		.CdbLength = CDB6GENERIC_LENGTH,
//		.DataBuffer = &data,
//		.DataTransferLength = sizeof(data),
//		.TimeOutValue = kTimeOut,
//		.DataIn = SCSI_IOCTL_DATA_IN,
//		.Cdb[0] = SCSIOP_REQUEST_SENSE,
//		.Cdb[4] = sizeof(data),
//	};
//
//	DWORD cb = 0;
//	BOOL ok = DeviceIoControl(
//		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
//		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
//		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
//		&cb, FALSE
//	);
//
//	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
//		return NULL;
//	}
//
//	return &data;
//}

// Return pointer to inner static buffer
static const ReadCapacityData10*
getCapacity10(HANDLE h) {
	static ReadCapacityData10 data;

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB10GENERIC_LENGTH,
		.DataBuffer = &data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
		.Cdb[0] = SCSIOP_READ_CAPACITY,
	};

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}
	return &data;
}

static const ReadCapacityData16*
getCapacity16(HANDLE h) {
	static ReadCapacityData16 data;

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = 16,
		.DataBuffer = &data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
	};
	Cdb16ServiceActionIn* cdb = (Cdb16ServiceActionIn*)sptd.Cdb;
	cdb->operationCode = SCSIOP_READ_CAPACITY16;
	cdb->serviceAction = 0x10;
	cdb->allocationLength[3] = sizeof(data);

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}
	return &data;
}

static bool
getCapacity(HANDLE h, uint32_t* lbSize, uint64_t* lbCount) {
	const ReadCapacityData10* p10 = getCapacity10(h);
	if (p10 && p10->lbLast != ~(DWORD)0) {
		*lbSize = _byteswap_ulong(p10->lbSize);
		*lbCount = 1ULL + _byteswap_ulong(p10->lbLast);
		return true;
	}
	const ReadCapacityData16* p16 = getCapacity16(h);
	if (p16 && p16->lbLast != ~(uint64_t)0) {
		*lbSize = _byteswap_ulong(p16->lbSize);
		*lbCount = 1ULL + _byteswap_uint64(p16->lbLast);
		return true;
	}
	return false;
}

// Return pointer to inner static buffer
static const StandardInquiryData*
getStandardInquiry(HANDLE h) {
	static StandardInquiryData data;

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB6GENERIC_LENGTH,
		.DataBuffer = &data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
		.Cdb[0] = SCSIOP_INQUIRY,
		.Cdb[4] = sizeof(data),
	};

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}
	return &data;
}

// Return pointer to inner static buffer
static const PowerConditionData10*
getPowerCondition10(HANDLE h, ModeType type) {
	static PowerConditionData10 data;

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB10GENERIC_LENGTH,
		.DataBuffer = &data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
	};
	Cdb10ModeSense* cdb = (Cdb10ModeSense*)sptd.Cdb;
	cdb->operationCode = SCSIOP_MODE_SENSE10;
	cdb->disableBlockDescriptors = 1;
	cdb->pageCode = 0x1A;
	cdb->pageControl = type;
	cdb->allocLength[1] = sizeof(data);

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}

	return &data;
}

// Return pointer to inner static buffer
static const PowerConditionData6*
getPowerCondition6(HANDLE h, ModeType type) {
	static PowerConditionData6 data;

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB6GENERIC_LENGTH,
		.DataBuffer = &data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
	};
	Cdb6ModeSense* cdb = (Cdb6ModeSense*)sptd.Cdb;
	cdb->operationCode = SCSIOP_MODE_SENSE;
	cdb->disableBlockDescriptors = 1;
	cdb->pageCode = 0x1A;
	cdb->pageControl = type;
	cdb->allocLength = sizeof(data);

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}

	return &data;
}

static const PowerConditionModePage*
getPowerCondition(HANDLE h, ModeType type) {
	const PowerConditionData10* p10 = getPowerCondition10(h, type);
	if (p10) return &p10->modePage;
	const PowerConditionData6* p6 = getPowerCondition6(h, type);
	return p6 ? &p6->modePage : NULL;
}

static bool
setPowerCondition10(HANDLE h, const PowerConditionData10* p) {
	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB10GENERIC_LENGTH,
		.DataBuffer = (PVOID)p,
		.DataTransferLength = sizeof(PowerConditionData10),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_OUT,
	};
	Cdb10ModeSelect* cdb = (Cdb10ModeSelect*)sptd.Cdb;
	cdb->operationCode = SCSIOP_MODE_SELECT10;
	cdb->savePages = 1;
	cdb->pageFormat = 1;
	cdb->parameterListLength[1] = sizeof(PowerConditionData10);

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return false;
	}

	return true;
}

static bool
setPowerCondition6(HANDLE h, const PowerConditionData6* p) {
	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB6GENERIC_LENGTH,
		.DataBuffer = (PVOID)p,
		.DataTransferLength = sizeof(PowerConditionData6),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_OUT,
	};
	Cdb6ModeSelect* cdb = (Cdb6ModeSelect*)sptd.Cdb;
	cdb->operationCode = SCSIOP_MODE_SELECT;
	cdb->savePages = 1;
	cdb->pageFormat = 1;
	cdb->parameterListLength = sizeof(PowerConditionData6);

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok || sptd.ScsiStatus != SCSISTAT_GOOD) {
		return false;
	}

	return true;
}

// Return pointer to inner static buffer
static const BYTE*
getVpdPage(HANDLE h, ULONG* size, BYTE pageCode) {
	static BYTE data[128];

	SCSI_PASS_THROUGH_DIRECT sptd = {
		.Length = sizeof(SCSI_PASS_THROUGH_DIRECT),
		.CdbLength = CDB6GENERIC_LENGTH,
		.DataBuffer = data,
		.DataTransferLength = sizeof(data),
		.TimeOutValue = kTimeOut,
		.DataIn = SCSI_IOCTL_DATA_IN,
		.Cdb[0] = SCSIOP_INQUIRY,
		.Cdb[1] = 1, // EVPD
		.Cdb[2] = pageCode,
		.Cdb[4] = (BYTE)sizeof(data),
	};

	DWORD cb = 0;
	BOOL ok = DeviceIoControl(
		h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&sptd, sizeof(SCSI_PASS_THROUGH_DIRECT),
		&cb, FALSE
	);

	if (!ok
		|| sptd.ScsiStatus != SCSISTAT_GOOD) {
		return NULL;
	}

	*size = sptd.DataTransferLength;
	return data;
}

// CharacteristicsData is fixed in length
static inline const CharacteristicsData*
getCharacteristics(HANDLE h) {
	ULONG size;
	const BYTE* p = getVpdPage(h, &size, 0xB1);
	return (const CharacteristicsData*)p;
}

static inline const SerialNumberData*
getSerialNumber(HANDLE h) {
	ULONG size;
	const BYTE* data = getVpdPage(h, &size, 0x80);
	return (const SerialNumberData*)data;
}

// Count string length excluding tailing spaces and NULs
static inline int
getStringLen(const UCHAR* str, int cb) {
	if (!str) return 0;

	for (int i = cb - 1; i >= 0; --i) {
		if (str[i] == 0x20) continue;
		if (str[i] == 0x00) continue;
		return i + 1;
	}
	return 0;
}

static inline void
normalizeString(wchar_t* t, const UCHAR* str, int cb) {
	const int len = getStringLen(str, cb);
	int i = 0;
	for (; i < len; ++i) {
		t[i] = str[i];
	}
	t[i] = L'\0';
}

static void
fillInquiry(UnitInfo* info, const StandardInquiryData* inquiry) {
	normalizeString(info->vendor, inquiry->vendorId, unit_kLenVendorId);
	normalizeString(info->product, inquiry->productId, unit_kLenProductId);
	normalizeString(info->revision, inquiry->revision, unit_kLenRevision);
}

static void
fillSerial(UnitInfo* info, const SerialNumberData* serial) {
	if (serial) {
		const int serialLen = min(serial->pageLength[1], unit_kLenSerial);
		normalizeString(info->serial, serial->serialNumber, serialLen);
	}
	else {
		info->serial[0] = L'\0';
	}
}

static void
fillCharacteristics(UnitInfo* info, const CharacteristicsData* p) {
	if (p) {
		info->formFactor = p->formFactor;
		info->rpm = _byteswap_ushort(p->rpm);
	}
	else {
		info->formFactor = unit_kFormFactorNA;
		info->rpm = 0;
	}
}

// Since CreateFile activates the drive, it's pointless to use this function
//static void
//fillPowerCondition(UnitInfo* info, const SENSE_DATA* p) {
//	info->powerCondition = unit_kPowerConditionUnknown;
//	if (p->ErrorCode != SCSI_SENSE_ERRORCODE_FIXED_CURRENT) return;
//
//	// See P.760, spc5r22.pdf - Annex F.2 Additional sense codes
//
//	if (p->SenseKey == SCSI_SENSE_NOT_READY
//		&& p->AdditionalSenseCode == 0x04
//		&& p->AdditionalSenseCodeQualifier == 0x02) {
//		info->powerCondition = unit_kStopped;
//		return;
//	}
//
//	if (p->SenseKey != SCSI_SENSE_NO_SENSE) return;
//
//	switch (p->AdditionalSenseCode) {
//	case 0x00:
//		if (!p->AdditionalSenseCodeQualifier) info->powerCondition = unit_kActive;
//		break;
//	case 0x5E:
//		switch (p->AdditionalSenseCodeQualifier) {
//		case 0x00:
//		case 0x02:
//		case 0x04:
//			info->powerCondition = unit_kStandbyZ;
//			break;
//		case 0x01:
//		case 0x03:
//			info->powerCondition = unit_kIdleA;
//			break;
//		case 0x05:
//		case 0x06:
//			info->powerCondition = unit_kIdleB;
//			break;
//		case 0x07:
//		case 0x08:
//			info->powerCondition = unit_kIdleC;
//			break;
//		case 0x09:
//		case 0x0A:
//			info->powerCondition = unit_kStandbyY;
//			break;
//		}
//		break;
//	}
//}

bool
unit_getInfo(HANDLE h, UnitInfo* info)
{
	const StandardInquiryData* inquiry = getStandardInquiry(h);
	if (!inquiry) return false;

	if (!getCapacity(h, &info->blockSize, &info->blockCount)) {
		info->blockSize = 0;
		info->blockCount = 0;
	}

	fillInquiry(info, inquiry);
	const SerialNumberData* serial = getSerialNumber(h);
	fillSerial(info, serial);
	const CharacteristicsData* charas = getCharacteristics(h);
	fillCharacteristics(info, charas);

	return true;
}

static void
fillTimerMask(UnitInfo* info, const PowerConditionModePage* p) {
	info->timerIdleA = p->idleA;
	info->timerIdleB = p->idleB;
	info->timerIdleC = p->idleC;
	info->timerStandbyY = p->standbyY;
	info->timerStandbyZ = p->standbyZ;
}

static void
doFillTimers(DWORD* timers, const PowerConditionModePage* p) {
	timers[unit_kIdleA] = p->idleA ? _byteswap_ulong(p->timerIdleA) : 0;
	timers[unit_kIdleB] = p->idleB ? _byteswap_ulong(p->timerIdleB) : 0;
	timers[unit_kIdleC] = p->idleC ? _byteswap_ulong(p->timerIdleC) : 0;
	timers[unit_kStandbyY] = p->standbyY ? _byteswap_ulong(p->timerStandbyY) : 0;
	timers[unit_kStandbyZ] = p->standbyZ ? _byteswap_ulong(p->timerStandbyZ) : 0;
}

static void
fillTimers(UnitInfo* info, ModeType type, const PowerConditionModePage* p) {
	DWORD* timers = NULL;
	switch (type) {
	case kModeCurrent:
		timers = info->timers;
		break;
	case kModeChangeable:
		timers = info->timersModMask;
		break;
	case kModeDefault:
		timers = info->timersDefault;
		break;
	}
	if (!timers) return;

	doFillTimers(timers, p);
}

bool
unit_getTimers(HANDLE h, UnitInfo* info)
{
	info->timerMask = 0;
	const PowerConditionModePage* p = getPowerCondition(h, kModeCurrent);
	if (!p) return false;

	info->timerWritable = p->parametersSaveable;
	fillTimerMask(info, p);
	fillTimers(info, kModeCurrent, p);

	p = getPowerCondition(h, kModeChangeable);
	if (p) fillTimers(info, kModeChangeable, p);
	
	p = getPowerCondition(h, kModeDefault);
	if (p) fillTimers(info, kModeDefault, p);

	return true;
}

static bool
timersWritable(const UnitInfo* info, BYTE mask, const DWORD* timers) {
	if (!info->timerWritable) return false;
	if (!mask) return false;
	//if (!info->timerMask) return false; // covered by the next line
	if ((info->timerMask | mask) != info->timerMask) return false;

	for (int i = 0; i < unit_kPowerConditionCount && mask; ++i) {
		if (mask & 1) {
			if ((timers[i] & info->timersModMask[i]) != timers[i]) return false;
		}
		mask >>= 1;
	}

	return true;
}

static void
setPowerConditionModePage(PowerConditionModePage* p, BYTE mask, const DWORD* timers) {
	const TimerMask* tm = (TimerMask*)&mask;
	if (tm->timerIdleA) p->timerIdleA = _byteswap_ulong(timers[unit_kIdleA]);
	if (tm->timerIdleB) p->timerIdleB = _byteswap_ulong(timers[unit_kIdleB]);
	if (tm->timerIdleC) p->timerIdleC = _byteswap_ulong(timers[unit_kIdleC]);
	if (tm->timerStandbyY) p->timerStandbyY = _byteswap_ulong(timers[unit_kStandbyY]);
	if (tm->timerStandbyZ) p->timerStandbyZ = _byteswap_ulong(timers[unit_kStandbyZ]);
}

static bool
writeTimers10(HANDLE h, BYTE mask, const DWORD* timers) {
	PowerConditionData10* p = (PowerConditionData10*)getPowerCondition10(h, kModeCurrent);
	if (!p) return false;
	setPowerConditionModePage(&p->modePage, mask, timers);
	// bit reserved, P.342, sbc4r22.pdf - Table 230 - DEVICE-SPECIFIC PARAMETER field for direct access block devices
	p->deviceParameter = 0;
	// P.626, spc5r22.pdf - "When using the MODE SELECT command, the PS bit is reserved."
	p->parametersSaveable = 0;
	return setPowerCondition10(h, p);
}

static bool
writeTimers6(HANDLE h, BYTE mask, const DWORD* timers) {
	PowerConditionData6* p = (PowerConditionData6*)getPowerCondition6(h, kModeCurrent);
	if (!p) return false;
	setPowerConditionModePage(&p->modePage, mask, timers);
	// see writeTimers10() for these 2 flags
	p->deviceParameter = 0;
	p->parametersSaveable = 0;
	return setPowerCondition6(h, p);
}

static bool
writeTimers(HANDLE h, BYTE mask, const DWORD* timers) {
	if (writeTimers10(h, mask, timers)) return true;
	return writeTimers6(h, mask, timers);
}

bool
unit_setTimers(HANDLE h, BYTE mask, const DWORD timers[unit_kPowerConditionCount], const wchar_t** errmsg)
{
	static const wchar_t* kNoTimer = L"Device has no power condition timers.";
	static const wchar_t* kNotWritable = L"Timers not writable.";

	*errmsg = NULL;
	UnitInfo info;
	if (!unit_getTimers(h, &info)) {
		*errmsg = kNoTimer;
		return false;
	}
	if (!timersWritable(&info, mask, timers)) {
		*errmsg = kNotWritable;
		return false;
	}
	return writeTimers(h, mask, timers);
}

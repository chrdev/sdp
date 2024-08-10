#include "cap.h"

#include <strsafe.h>
#include <stdlib.h> // _countof


const wchar_t*
cap_getShortText(uint64_t n, wchar_t t[5])
{
	enum { kCch = 5 };
	static const wchar_t kT[] = L" KMGTPEZY";
	static const wchar_t kErr[] = L"OVER";
	static wchar_t buf[kCch]; // longest format: ###S

	if (!t) t = buf;

	int i = 0;
	for (; n >= 1000; ++i) {
		n /= 1000;
	}
	if (i > _countof(kT) - 2) {
		StringCchCopy(t, kCch, kErr);
		return t;
	}
	StringCchPrintf(t, kCch, L"%u%c", (unsigned int)n, kT[i]);
	return t;
}

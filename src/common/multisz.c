#include "multisz.h"

#include <sdkddkver.h>
#include <Windows.h> // MAX_PATH
#include <strsafe.h>
#pragma comment(lib, "strsafe.lib")

#include <assert.h>

#include "heap.h"


// Param buf: Preallocated buffer that can hold at least bufSize pointers.
//         If buf is NULL, return count.
// Return: count of matching strings. If failed, return 0.
size_t
msz_getStringsStartWith(const wchar_t** buf, size_t bufSize, const wchar_t* startsWith, const wchar_t* multisz)
{
	assert(startsWith);
	assert(multisz);

	size_t len;
	HRESULT hr = StringCchLength(startsWith, MAX_PATH, &len);
	if (FAILED(hr)) return 0;

	size_t c = 0;
	for (const wchar_t* p = multisz; *p; p += (lstrlen(p) + 1)) {
		if (wcsncmp(p, startsWith, len)) continue;
		++c;
		if (buf) {
			if (c > bufSize) return 0;
			*buf++ = p;
		}
	}
	return c;
}

// Return: pointers to multisz parts that sting matches.
// Param count: CAN NOT be NULL.
const wchar_t**
msz_manuStringListStartsWith(size_t* count, const wchar_t* startsWith, const wchar_t* multisz)
{
	assert(count);
	assert(startsWith);
	assert(multisz);

	size_t c = msz_getStringsStartWith(NULL, 0, startsWith, multisz);
	if (!c) return NULL;

	const wchar_t** p = heap_alloc(0, sizeof(*p) * c);
	if (!p) return NULL;
	*count = msz_getStringsStartWith(p, c, startsWith, multisz);
	assert(*count == c);
	return p;
}

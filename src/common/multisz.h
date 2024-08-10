#pragma once

#include <wchar.h>


// Param buf: Preallocated buffer that can hold at least bufSize pointers.
//         If buf is NULL, return count.
// Return: count of matching strings. If failed, return 0.
size_t
msz_getStringsStartWith(const wchar_t** buf, size_t bufSize, const wchar_t* startsWith, const wchar_t* multisz);


// Return: pointers to multisz parts that sting matches.
// Param count: CAN NOT be NULL.
const wchar_t**
msz_manuStringListStartsWith(size_t* count, const wchar_t* startsWith, const wchar_t* multisz);

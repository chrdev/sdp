#pragma once

#include <sdkddkver.h>
#include <Windows.h>

#include <stdbool.h>


static inline void*
heap_alloc(DWORD flags, size_t cb) {
	return HeapAlloc(GetProcessHeap(), flags, cb);
}

static inline bool
heap_free(DWORD flags, void* mem) {
	return HeapFree(GetProcessHeap(), flags, mem);
}

// SPDX-FileCopyrightText: 2023 chrdev
//
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>


// Format: 000S
// If t[] is not NULL, t must be no smaller than 5 wchar_t, result stored in t[]
// If t[] is NULL, return pointer to inner static buffer
const wchar_t*
cap_getShortText(uint64_t n, wchar_t t[5]);

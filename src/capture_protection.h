// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QtCore/QDebug>
#include <QtCore/QOperatingSystemVersion>
#include <windows.h>

namespace pass::capture {
// Numeric value also permits compilation with SDKs predating Windows 10 2004.
constexpr DWORD excludeFromCapture = 0x00000011;
enum class Protection { Excluded, MonitorOnly, Unprotected };
using SetAffinity = BOOL (WINAPI *)(HWND, DWORD);

inline bool supportsExclusion() {
    return QOperatingSystemVersion::current() >=
           QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 19041);
}

inline Protection apply(HWND hwnd, SetAffinity setAffinity = &::SetWindowDisplayAffinity,
                        bool exclusionSupported = supportsExclusion()) {
    // Only a live, process-owned top-level HWND is passed by Window.
    if (setAffinity(hwnd, excludeFromCapture)) {
        if (exclusionSupported) {
            qInfo("Pass capture protection: WDA_EXCLUDEFROMCAPTURE applied.");
            return Protection::Excluded;
        }
        qWarning("Pass capture protection: pre-2004 Windows; WDA_MONITOR semantics only.");
        return Protection::MonitorOnly;
    }
    const DWORD error = GetLastError();
    qWarning().nospace() << "Pass capture protection: WDA_EXCLUDEFROMCAPTURE failed; Win32 error=" << error;
    // Compatibility retry only for unsupported flags/APIs, not arbitrary failures.
    if (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED || error == ERROR_CALL_NOT_IMPLEMENTED) {
        if (setAffinity(hwnd, WDA_MONITOR)) {
            qWarning("Pass capture protection: fallback WDA_MONITOR applied (blank capture only).");
            return Protection::MonitorOnly;
        }
        const DWORD fallbackError = GetLastError();
        qWarning().nospace() << "Pass capture protection: WDA_MONITOR failed; Win32 error=" << fallbackError;
    }
    qWarning("Pass capture protection unavailable; continuing without capture exclusion.");
    return Protection::Unprotected;
}
}

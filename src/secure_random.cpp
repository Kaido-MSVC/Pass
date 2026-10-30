// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#include "secure_random.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
namespace pass {
Random::~Random() { SecureZeroMemory(buffer.data(), buffer.size()); }
unsigned char Random::byte() {
        if (index == buffer.size()) {
            const NTSTATUS status = BCryptGenRandom(nullptr, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (status < 0) {
                const QString code = QString::number(static_cast<quint32>(status), 16).rightJustified(8, QLatin1Char('0')).toUpper();
                throw std::runtime_error((QStringLiteral("Windows 安全亂數服務失敗，已停止產生。NTSTATUS=0x") + code).toUtf8().constData());
            }
            index = 0;
        }
        return buffer[index++];
    }
int Random::below(int n, const std::function<bool()>& cancelled) {
        if (n < 1 || n > 256) throw std::runtime_error("亂數取樣範圍無效。");
        const int limit = 256 - 256 % n;
        // Bound each draw without accepting rejected bytes or changing their weights.
        // Public pools have at most 94 characters: 1024 rejections are extraordinarily unlikely.
        for (int attempt = 0; attempt < 1024; ++attempt) {
            if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
            const int x = byte();
            if (x < limit) return x % n;
        }
        throw std::runtime_error("安全亂數拒絕取樣次數已達上限，已停止產生。");
    }
int Random::below16(int n, const std::function<bool()>& cancelled) {
    if (n < 1 || n > 65536) throw std::runtime_error("亂數取樣範圍無效。");
    const int limit = 65536 - 65536 % n;
    for (int attempt = 0; attempt < 1024; ++attempt) {
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        unsigned int value = byte();
        struct Wiper { unsigned int& value; ~Wiper() { SecureZeroMemory(&value, sizeof(value)); } } wipe{value};
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        value |= static_cast<unsigned int>(byte()) << 8;
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        if (value < static_cast<unsigned int>(limit)) return static_cast<int>(value % n);
    }
    throw std::runtime_error("安全亂數拒絕取樣次數已達上限，已停止產生。");
}
}

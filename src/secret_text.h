// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QString>
#include <windows.h>
#include <stdexcept>
#include <vector>
// Own exactly one allocation; never share secret text through QString/QVariant.
class SecretText {
    std::vector<QChar> data;
public:
    SecretText() = default;
    SecretText(const SecretText&) = delete;
    SecretText& operator=(const SecretText&) = delete;
    ~SecretText() { clear(); }
    void clear() noexcept { if (!data.empty()) SecureZeroMemory(data.data(), data.size()*sizeof(QChar)); data.clear(); }
    void reserve(size_t size) { clear(); data.reserve(size); }
    void append(const QString& value) {
        // Never reallocate an allocation containing secrets.
        if (static_cast<size_t>(value.size()) > data.capacity()-data.size()) throw std::length_error("Secret capacity exceeded");
        data.insert(data.end(), value.constData(), value.constData()+value.size());
    }
    QString text() const { return QString(data.data(), static_cast<qsizetype>(data.size())); }
    bool empty() const { return data.empty(); }
    void swap(SecretText& other) noexcept { data.swap(other.data); }
};
inline void wipeText(QString& text) {
    // Best effort only: Qt may retain other copies outside our ownership.
    if (!text.isEmpty()) SecureZeroMemory(text.data(), static_cast<SIZE_T>(text.size())*sizeof(QChar));
    text.clear();
}
class CandidateWiper {
    QChar* const data;
    const SIZE_T bytes;
    bool active = true;
public:
    explicit CandidateWiper(QString& candidate)
        : data(candidate.data()), bytes(static_cast<SIZE_T>(candidate.size()) * sizeof(QChar)) {}
    CandidateWiper(const CandidateWiper&) = delete;
    CandidateWiper& operator=(const CandidateWiper&) = delete;
    ~CandidateWiper() { if (active) SecureZeroMemory(data, bytes); }
    void release() noexcept { active = false; }
};

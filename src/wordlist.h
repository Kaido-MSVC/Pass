// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QByteArray>
#include <QStringList>
#include <QSet>
namespace pass {
inline constexpr int WordCount = 7776;
inline constexpr char WordlistSha256[] = "addd35536511597a02fa0a9ff1e5284677b8883b83e986e43f15a3db996b903e";
struct Wordlist {
    QStringList words;
    QSet<QString> members;
    int maxLength = 0;
    QString error;
    bool available() const { return error.isEmpty() && words.size() == WordCount; }
};
Wordlist validateWordlist(const QByteArray& original);
const Wordlist& wordlist();
}

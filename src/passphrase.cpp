// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#include "passphrase.h"
#include "wordlist.h"
#include "secure_random.h"
#include "secret_text.h"
#include <cmath>
#include <stdexcept>
namespace pass {
QString validate(const PassphraseRules& rules) {
    if (rules.words < 4 || rules.words > 12) return QStringLiteral("單詞數必須介於 4 到 12。");
    if (rules.separator != QLatin1Char('-') && rules.separator != QLatin1Char('_') && rules.separator != QLatin1Char(' '))
        return QStringLiteral("分隔符必須為連字號、底線或空白。");
    return wordlist().available() ? QString() : wordlist().error;
}
int maxLength(const PassphraseRules& rules) {
    const auto error = validate(rules);
    if (!error.isEmpty()) throw std::runtime_error(error.toUtf8().constData());
    return rules.words * wordlist().maxLength + rules.words - 1;
}
QString generate(const PassphraseRules& rules, const std::function<bool()>& cancelled) {
    // One allocation; CandidateWiper owns it through every exceptional path.
    QString candidate(maxLength(rules), QChar(0));
    CandidateWiper wipe(candidate); Random rng; qsizetype offset = 0;
    for (int i = 0; i < rules.words; ++i) {
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        int index = rng.below16(WordCount, cancelled);
        struct IndexWiper { int& value; ~IndexWiper() { SecureZeroMemory(&value, sizeof(value)); } } indexWiper{index};
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        if (i) candidate[offset++] = rules.separator;
        for (const QChar c : wordlist().words[index]) candidate[offset++] = c;
    }
    if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
    // Shrinking does not reallocate; unused tail was initialized to zero.
    candidate.truncate(offset); wipe.release(); return candidate;
}
bool matches(const QString& text, const PassphraseRules& rules) {
    if (!validate(rules).isEmpty() || text.size() > maxLength(rules)) return false;
    // Parse whole dictionary words: four official words contain a hyphen.
    QVector<QVector<bool>> reachable(rules.words + 1, QVector<bool>(text.size() + 1, false));
    reachable[0][0] = true;
    for (int used = 0; used < rules.words; ++used) {
        for (qsizetype pos = 0; pos < text.size(); ++pos) {
            if (!reachable[used][pos]) continue;
            for (int size = 1; size <= wordlist().maxLength && pos + size <= text.size(); ++size) {
                QString word = text.mid(pos, size);
                struct Wiper { QString& value; ~Wiper() { wipeText(value); } } wipe{word};
                if (!wordlist().members.contains(word)) continue;
                const qsizetype end = pos + size;
                if (used + 1 == rules.words) { if (end == text.size()) return true; }
                else if (end < text.size() && text[end] == rules.separator) reachable[used + 1][end + 1] = true;
            }
        }
    }
    return false;
}
double entropy(const PassphraseRules& rules) {
    return validate(rules).isEmpty() ? rules.words * std::log2(double(WordCount)) : 0;
}
}

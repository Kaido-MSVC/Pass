// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QString>
#include <functional>
namespace pass {
struct PassphraseRules {
    int words = 6;
    QChar separator = QLatin1Char('-');
};
QString validate(const PassphraseRules& rules);
QString generate(const PassphraseRules& rules, const std::function<bool()>& cancelled = {});
bool matches(const QString& text, const PassphraseRules& rules);
double entropy(const PassphraseRules& rules);
int maxLength(const PassphraseRules& rules);
}

#pragma once
#include "random_algorithm.h"
#include <QString>
#include <functional>
namespace pass {
struct PassphraseRules {
    RandomAlgorithm algorithm = RandomAlgorithm::Ctr;
    int words = 6;
    QChar separator = QLatin1Char('-');
};
QString validate(const PassphraseRules& rules);
QString generate(const PassphraseRules& rules, const std::function<bool()>& cancelled = {});
bool matches(const QString& text, const PassphraseRules& rules);
double entropy(const PassphraseRules& rules);
int maxLength(const PassphraseRules& rules);
}

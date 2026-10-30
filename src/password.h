#pragma once
#include "random_algorithm.h"
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
namespace pass {
struct Rules {
    RandomAlgorithm algorithm = RandomAlgorithm::Ctr;
    int length = 20;
    bool upper = true, lower = true, digits = true, symbols = true;
    bool excludeAmbiguous = true;
    QString custom, excluded;
    int minUpper = 1, minLower = 1, minDigits = 1, minSymbols = 1;
};
struct Analysis { int score; QString label; QStringList feedback; };
QString validate(const Rules& rules);
bool matches(const QString& password, const Rules& rules);
// Synchronous core: cancellation is cooperative and cannot interrupt an OpenSSL call itself.
// The desktop UI uses generation.h to bound waiting without terminating a thread.
QString generate(const Rules& rules, const std::function<bool()>& cancelled = {});
double entropy(const Rules& rules);
Analysis analyze(const QString& password);
}

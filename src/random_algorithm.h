#pragma once
#include <QString>
namespace pass {
enum class RandomAlgorithm { Ctr, Hash, Hmac, Hmac512 };
inline const char* algorithmName(RandomAlgorithm a) {
    switch (a) {
    case RandomAlgorithm::Ctr: return "CTR-DRBG";
    case RandomAlgorithm::Hash: return "HASH-DRBG";
    case RandomAlgorithm::Hmac: case RandomAlgorithm::Hmac512: return "HMAC-DRBG";
    }
    return nullptr;
}
inline const char* algorithmId(RandomAlgorithm a) {
    return a == RandomAlgorithm::Hmac512 ? "HMAC-DRBG-SHA512" : algorithmName(a);
}
inline bool parseAlgorithm(const QString& name, RandomAlgorithm& value) {
    for (auto a : {RandomAlgorithm::Ctr, RandomAlgorithm::Hash, RandomAlgorithm::Hmac, RandomAlgorithm::Hmac512})
        if (name == QLatin1String(algorithmId(a))) { value = a; return true; }
    return false;
}
}

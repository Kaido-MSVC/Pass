#pragma once
#include <QString>
#include <array>
#include <memory>
#include "random_algorithm.h"
#include <functional>
namespace pass {
class Random {
    std::array<unsigned char, 256> buffer{};
    size_t index = buffer.size();
    struct State;
    std::unique_ptr<State> state;
    RandomAlgorithm algorithm;
    bool failed = false;
    size_t refills = 0;
public:
    explicit Random(RandomAlgorithm selected = RandomAlgorithm::Ctr);
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;
    ~Random();
    unsigned char byte();
    void reseed();
    int below(int n, const std::function<bool()>& cancelled = {});
    int below16(int n, const std::function<bool()>& cancelled = {});
};
}

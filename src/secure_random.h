// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QString>
#include <array>
#include <functional>
namespace pass {
class Random {
    std::array<unsigned char, 256> buffer{};
    size_t index = buffer.size();
public:
    Random() = default;
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;
    ~Random();
    unsigned char byte();
    int below(int n, const std::function<bool()>& cancelled = {});
    int below16(int n, const std::function<bool()>& cancelled = {});
};
}

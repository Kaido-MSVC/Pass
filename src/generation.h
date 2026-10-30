// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include "password.h"
#include "passphrase.h"
#include "secret_text.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
struct Generation {
    std::atomic_bool cancelled{false}, done{false};
    std::mutex mutex;
    SecretText secret;
    QString error;
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    void abandon() { cancelled.store(true); std::lock_guard<std::mutex> lock(mutex); secret.clear(); }
};
inline std::atomic_bool generationBusy{false};
size_t batchCapacity(size_t maxLength, int count);
std::shared_ptr<Generation> startGeneration(const pass::Rules& rules, int count);
std::shared_ptr<Generation> startGeneration(const pass::PassphraseRules& rules, int count);
#ifdef PASS_TESTING
// Compiled only into test targets. Production has no provider selection API.
using Generator = QString (*)(const pass::Rules&, const std::function<bool()>&);
using WordGenerator = QString (*)(const pass::PassphraseRules&, const std::function<bool()>&);
std::shared_ptr<Generation> startGeneration(const pass::Rules& rules, int count, Generator generate);
std::shared_ptr<Generation> startGeneration(const pass::PassphraseRules& rules, int count, WordGenerator generate);
#endif

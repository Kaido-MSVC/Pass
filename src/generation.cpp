// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#include "generation.h"
#include <thread>
#include <limits>
#include <stdexcept>
size_t batchCapacity(size_t maxLength, int count) {
    if (count < 1 || count > 100) throw std::invalid_argument("產生數量必須介於 1 到 100。");
    const size_t separators = static_cast<size_t>(count - 1);
    const size_t limit = (std::numeric_limits<size_t>::max)() / sizeof(QChar);
    if (!maxLength || maxLength > (limit - separators) / static_cast<size_t>(count))
        throw std::length_error("批次容量無效。");
    return maxLength * static_cast<size_t>(count) + separators;
}
namespace {
using One = std::function<QString(const std::function<bool()>&)>;
std::shared_ptr<Generation> startBackground(size_t maxLength, int count, One generate) {
    const size_t capacity = batchCapacity(maxLength, count);
    auto job = std::make_shared<Generation>();
    bool expected = false;
    if (!generationBusy.compare_exchange_strong(expected, true)) return {};
    try {
        std::thread([job,count,capacity,generate=std::move(generate)] {
            QString error;
            try {
                { std::lock_guard<std::mutex> lock(job->mutex); job->secret.reserve(capacity); }
                auto cancelled = [job] { return job->cancelled.load() || std::chrono::steady_clock::now()-job->started >= std::chrono::seconds(10); };
                for (int i = 0; i < count; ++i) {
                    if (cancelled()) throw std::runtime_error("已取消或等待逾時。");
                    QString value = generate(cancelled);
                    struct Wiper { QString& value; ~Wiper() { wipeText(value); } } wipe{value};
                    if (cancelled()) throw std::runtime_error("已取消或等待逾時。");
                    std::lock_guard<std::mutex> lock(job->mutex);
                    if (cancelled()) throw std::runtime_error("已取消或等待逾時。");
                    if (i) job->secret.append(QStringLiteral("\n"));
                    job->secret.append(value);
                }
            } catch (const std::exception& e) { error = QString::fromUtf8(e.what()); }
              catch (...) { error = QStringLiteral("產生失敗，已停止。"); }
            {
                std::lock_guard<std::mutex> lock(job->mutex);
                if (!job->cancelled.load()) job->error = error;
                if (job->cancelled.load() || !error.isEmpty()) job->secret.clear();
            }
            generationBusy.store(false); job->done.store(true);
        }).detach();
    } catch (...) { generationBusy.store(false); throw; }
    return job;
}
size_t characterLength(const pass::Rules& rules) {
    const auto error = pass::validate(rules);
    if (!error.isEmpty()) throw std::invalid_argument(error.toUtf8().constData());
    return static_cast<size_t>(rules.length);
}
}
std::shared_ptr<Generation> startGeneration(const pass::Rules& rules, int count) {
    return startBackground(characterLength(rules), count, [rules](const auto& cancel) { return pass::generate(rules, cancel); });
}
std::shared_ptr<Generation> startGeneration(const pass::PassphraseRules& rules, int count) {
    return startBackground(static_cast<size_t>(pass::maxLength(rules)), count, [rules](const auto& cancel) { return pass::generate(rules, cancel); });
}
#ifdef PASS_TESTING
std::shared_ptr<Generation> startGeneration(const pass::Rules& rules, int count, Generator generate) {
    return startBackground(characterLength(rules), count, [rules,generate](const auto& cancel) { return generate(rules, cancel); });
}
std::shared_ptr<Generation> startGeneration(const pass::PassphraseRules& rules, int count, WordGenerator generate) {
    return startBackground(static_cast<size_t>(pass::maxLength(rules)), count, [rules,generate](const auto& cancel) { return generate(rules, cancel); });
}
#endif

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kaido-MSVC
#pragma once
#include <QString>
namespace pass {
// Conservative ceiling for the existing UI score, not a measure of RNG entropy.
int dictionaryScoreLimit(const QString& text) noexcept;
}

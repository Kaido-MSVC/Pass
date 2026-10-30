# Pass-SSL v2.2.1 integration

Upstream: https://github.com/tsyrogit/zxcvbn-c
Commit: 4011582de2ee7885c8f29560f86716829cfdfd44 (2026-10-03).
MIT. See LICENSE.txt and ../../LICENSES/zxcvbn-Dropbox-MIT.txt.

The estimator source and six dictionary text files are vendored. CMake builds
the dictionary generator locally and embeds its trie in the executable. There
is no runtime script engine, external dictionary, update mechanism or network
dependency. No prebuilt third-party executable is downloaded or executed.

Local upstream changes: an empty stdafx.h satisfies its Windows PCH include;
the dictionary generator uses %zu for its size_t diagnostic under MSVC LLP64;
test.c uses steady_clock instead of POSIX sys/time.h on Windows. Estimation
logic and dictionaries are unchanged. The parent CMake file replaces the
upstream GNU-specific build settings. Original file hashes are recorded in
UPSTREAM-SHA256.json.

src/strength_estimator.cpp compiles zxcvbn.c as C++ with tracked allocation
macros. Every allocated graph/reversed-password buffer is wiped on normal
release or exception unwinding, and UTF-8/phrase normalization temporaries are
wiped. An allocation exception returns a zero score. The detailed input bound
is 512 bytes, covering the complete UTF-8 encoding of the UI's 128 UTF-16 units;
embedded NUL and over-limit input are not evaluated. ASAN and exhaustive/sampled
allocation-failure tests are separate development executables, never shipped.

The original UI's 0..100 heuristic score is capped by the native estimator's
finite bit estimate clamped to that range. Inputs already rated weak by the
original matcher do not build a second graph. This is a conservative UI scale,
not calibrated guesses, crack time, actual entropy or a security guarantee.
The C port has documented differences from Dropbox's modern guessing model.
Its dictionaries focus on English and names; it cannot know every public,
personal or breached phrase, and UTF-8 bytes are not a Unicode language model.

Three public tutorial examples are added as user dictionary entries, with a
small normalized guard for case, punctuation/separators and common leet forms.
Exact normalized examples cap at 20; up to eight additional normalized letters
cap at 34. A sufficiently long unrelated random suffix still receives credit.
This is a finite offline list, not a comprehensive breach database. The existing
keyboard/repeat matcher remains because it already has dedicated regression
coverage and the library uses a different set of keyboard patterns.

Alternatives assessed: Dropbox's original MIT JavaScript implementation requires
a JavaScript engine or port in this native Qt application; the referenced
zxcvbn-cpp GitHub repository is archived and redirects elsewhere. The C port
has no library dependencies beyond the CRT and is directly MSVC-compatible.
Vendoring pins the reviewed code and avoids build-time dependency downloads.
This assessment is not a certification that upstream has no vulnerabilities.

# Third-party components

Pass-SSL is Copyright (c) 2026 Kaido-MSVC (author) and is released under the
MIT License; see LICENSE.txt. The components below keep their own licenses.

## zxcvbn-c (embedded estimator and dictionaries)

Source: https://github.com/tsyrogit/zxcvbn-c
Pinned commit: 4011582de2ee7885c8f29560f86716829cfdfd44 (2026-10-03).
MIT license, Copyright (c) 2015-2017 Tony Evans; see LICENSES/zxcvbn-c-MIT.txt.
Its word dictionaries derive from Dropbox zxcvbn; the original MIT notice is
included as LICENSES/zxcvbn-Dropbox-MIT.txt. No network lookup occurs at runtime.
See third_party/zxcvbn-c/PASS-INTEGRATION.md in the source distribution for local
build adaptations, score mapping, limitations and source hashes.

## Qt 6.12.0

Pass-SSL dynamically links Qt 6.12.0 (Qt Core, Qt Gui, Qt Widgets, Qt Svg and the
plugins deployed by windeployqt, plus any module those depend on).
Qt is Copyright (C) The Qt Company Ltd. and other contributors.
License: GNU Lesser General Public License version 3 (LGPLv3), which incorporates
the GNU GPL version 3. Full texts: LICENSES/LICENSE.LGPLv3 and LICENSES/LICENSE.GPLv3.
Notices for third-party code inside Qt: LICENSES/qt-third-party/.
Qt source and licensing information:
https://www.qt.io/licensing/open-source-lgpl-obligations
https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/

The Qt DLLs are shipped unmodified, remain separate files and can be replaced with
compatible versions. Help > About Qt in the application shows Qt's own notice.

## Microsoft Visual C++ Runtime

Microsoft Visual C++ runtime DLLs are distributed app-local in both the portable
package and the installer; no system-wide Redistributable is installed.
Microsoft license terms apply; see LICENSES/Microsoft-Redist.txt.

## EFF Long Wordlist

Copyright Electronic Frontier Foundation. Created by Joseph Bonneau.
Source: https://www.eff.org/files/2016/07/18/eff_large_wordlist.txt
SHA-256 (original bytes): addd35536511597a02fa0a9ff1e5284677b8883b83e986e43f15a3db996b903e
License: Creative Commons Attribution 4.0 International (CC BY 4.0), according to
https://www.eff.org/copyright (no alternate notice in the source file).
https://creativecommons.org/licenses/by/4.0/
Full legal text is included in LICENSES/EFF-CC-BY-4.0.txt.
The original wordlist is embedded unchanged; runtime parsing removes dice codes
without changing the words or order. Pass is not endorsed by EFF.

## OpenSSL 4.0.3

Copyright The OpenSSL Project Authors and contributors.
Pass-SSL dynamically links libcrypto-4-x64.dll (VC-WIN64A, release /MD).
License: Apache License 2.0. The installed distribution's complete license and
author list are included as LICENSES/OpenSSL-Apache-2.0.txt and OpenSSL-authors.txt.
https://www.openssl.org/ and https://github.com/openssl/openssl

The built-in default provider supplies SEED-SRC, CTR-DRBG, HASH-DRBG and HMAC-DRBG.
No legacy, external or FIPS provider is shipped or required. This application
does not claim FIPS certification. No libssl DLL is needed by the application.

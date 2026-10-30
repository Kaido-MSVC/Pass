# Third-party components

Pass is Copyright (c) 2026 Kaido-MSVC (author) and is released under the
MIT License; see LICENSE.txt. The components below keep their own licenses.

## Qt 6.12.0

Pass dynamically links Qt 6.12.0 (Core, Gui, Widgets and deployed plugins (including their Svg/Network dependencies)).
Qt is Copyright (C) The Qt Company Ltd. and other contributors.
License: GNU Lesser General Public License version 3 (LGPLv3), which incorporates
the GNU GPL version 3. Full texts: LICENSES/LICENSE.LGPLv3 and LICENSES/LICENSE.GPLv3.
Notices for third-party code inside Qt: LICENSES/qt-third-party/.
Qt source and licensing information:
https://www.qt.io/licensing/open-source-lgpl-obligations
https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/

Qt DLLs are shipped unmodified, remain separate and can be replaced with compatible
versions. Help > About Qt in the application shows Qt's own notice.

## Microsoft Visual C++ Redistributable

Microsoft Visual C++ Redistributable is distributed through its official installer;
its Microsoft license terms apply. Windows BCrypt is a system component.

Additional deployed Qt plugins may have their own third-party notices; see the
Qt distribution and source modules for the corresponding component notices.

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


## Offline strength estimator (zxcvbn-c)

Copyright (c) 2015-2017 Tony Evans; dictionaries derive from Dropbox zxcvbn (MIT).
Vendored zxcvbn-c (MIT), adapted from Pass-SSL v2.2.2; see third_party/zxcvbn-c/PASS-INTEGRATION.md and LICENSES/zxcvbn-*-MIT.txt. No OpenSSL dependency.

## Mesa llvmpipe 11.2.2 (opengl32sw.dll)

The unmodified software OpenGL DLL comes from the installed Qt 6.12.0 Windows
x64 package `opengl32sw-64-mesa_11_2_2-signed_sha256`. It is a graphics fallback,
not Pass's random-number core. The DLL matches the installed Qt copy byte for
byte; its SHA-256 is recorded in LICENSES/Mesa-llvmpipe.txt.

Complete Brian Paul / Mesa MIT, Khronos MIT and yohhoy C11 threads / Boost
Software License 1.0 notices are included in **LICENSES/Mesa-llvmpipe.txt**.
They were transcribed from Qt 6.12.0's official attribution, not replaced by
license names or links. The original wording is retained.
https://doc.qt.io/qt-6/qt-attribution-llvmpipe.html

Qt's legacy Mesa 11.2.2 build documentation identifies LLVM 3.6. The complete
legacy University of Illinois/NCSA LLVM license is in LICENSES/LLVM-3.6-LICENSE.txt;
supplemental LLVM notices from Qt are also preserved separately. This does not
claim an exact LLVM patch version from a DLL without version metadata.
https://wiki.qt.io/index.php?title=MesaLlvmpipe&oldid=33647
https://releases.llvm.org/3.6.0/LICENSE.TXT

## Microsoft D3DCompiler_47.dll

Copyright Microsoft Corporation. This unmodified graphics helper is deployed
by Qt, file version 6.3.9600.16384, SHA-256
e994847e01a6f1e4cbdc5a864616ac262f67ee4f14db194984661a8d927ab7f4.
It is not licensed under Pass's MIT license. Microsoft's Windows SDK REDIST
list identifies Redist/D3D/x64/d3dcompiler_47.dll as redistributable for desktop
applications. The full installed SDK license, SDK third-party notices and the
official REDIST list are included in LICENSES/Microsoft-Windows-SDK-*.
The included SDK terms are from SDK 10.0.26100.0; this does not assert that the
older Qt-provided DLL is the same build as the current Windows SDK copy.
https://learn.microsoft.com/en-us/legal/windows-sdk/redist
https://learn.microsoft.com/en-us/windows/win32/directx-sdk--august-2009-

## Qt corresponding source and replacement

Qt DLLs and plugins remain unmodified, separate and replaceable. To use a
compatible user-built Qt, close Pass and replace the Qt DLLs and matching
plugins together in the extracted application directory; keep their relative
paths. Rebuild Pass against that Qt if its ABI changes. Reverse engineering
for debugging modifications to these libraries is permitted. No signature or
activation check prevents running a compatible replacement.

Corresponding source for the shipped Qt 6.12.0 modules is available from Qt's
official server (these are third-party source downloads, separate from the
Pass application source in this repository):
- https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz
- https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtsvg-everywhere-src-6.12.0.tar.xz
- https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtimageformats-everywhere-src-6.12.0.tar.xz

All three are unmodified upstream modules. Pass's source, CMake project,
resources and build scripts are published under the Plus tag. See README.md
for the pinned Windows toolchain and LICENSES/INDEX.md for the license files.

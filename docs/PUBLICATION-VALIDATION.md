# Pass 2.0 Plus public release validation — 2026-10-30

This publication retains the original Windows BCryptGenRandom core. Pass 2.1
onward is the OpenSSL line; existing main and v2.2.3 remain unchanged.

## Scope and accepted functional evidence

Only license/documentation and packaging files changed in this publication.
Application source, tests, resources, vendored estimator and accepted Pass.exe
are byte-identical to the local input reviewed before publication. No rebuild
or complete functional/boundary/stress rerun was performed. The existing
2026-10-30 CTest log records 11/11 test groups passed after the About/license
UI was added; those accepted results are reused. Historical local logs and
backups containing private absolute paths are not part of the public source.

Accepted Pass.exe SHA-256:
`da3ab0ace77e918f070169ce4b2cdd5e698fca08b7908c1113399ab10dad87b5`

## License and package checks

- Mesa DLL is identical to the installed Qt 6.12.0 x64 copy. Qt's installed
  package identity is opengl32sw-64-mesa_11_2_2-signed_sha256; Mesa is 11.2.2.
- All four official Qt 6.12.0 llvmpipe attribution blocks, including the full
  Brian Paul, Khronos and yohhoy/C11 threads grants and disclaimers, are
  reproduced in LICENSES/Mesa-llvmpipe.txt. Legacy LLVM 3.6 and supplemental
  notices are separately included. Exact LLVM patch version is not asserted.
- Qt LGPL/GPL, module attributions, zxcvbn-c/Dropbox MIT and EFF CC BY 4.0
  legal texts remain included. Microsoft SDK terms/REDIST material accompanies
  the D3D helper. The VC++ Redistributable remains an unmodified installer.
- Qt 6.12.0 qtbase, qtsvg and qtimageformats corresponding-source archive
  URLs each returned HTTP 200. Source and DLL replacement instructions are
  in THIRD_PARTY_NOTICES.md.
- Rebuilt NSIS installer packages the accepted EXE and revised documents;
  it is not an MSI. The generated uninstall file list includes added notices.
- Extracted ZIP and NSIS payloads match all 95 staging Portable files
  by SHA-256, including every license and binary. All application/Qt binaries
  and the bundled VC++ redist remain unchanged. The generated uninstall list
  covers every payload file. Final checksums are in Release SHA256SUMS.txt.

## Known limits

Pass.exe and setup.exe are unsigned. Clean-VM install/uninstall and VC++
redist installation, Windows 10 1809 hardware, and manual Plus Snipping Tool/
OBS checks are not newly verified. Clipboard and memory cleanup and display
affinity have platform limits; strength scores are offline hints, not a
security probability or breach lookup. See README.md and PLUS.md.

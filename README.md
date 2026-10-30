# Equalizer APO — independent maintenance fork

A system-wide audio equalizer for Windows. This fork starts from the **official SourceForge Git repository**, retaining its history, release tags, copyright notices and GPL license. It is maintained independently and is not an official upstream release.

- [Official project and downloads](https://sourceforge.net/projects/equalizerapo/)
- [Official source](https://sourceforge.net/p/equalizerapo/code/ci/main/tree/)
- [Configuration documentation](https://sourceforge.net/p/equalizerapo/wiki/Configuration%20reference/)
- [First-pass review and follow-up priorities](docs/first-pass-review.md)

## First changes

Harden convolution impulse-response loading: stop on incomplete reads, close files on failure, validate lengths before allocating or narrowing, manage temporary buffers automatically, and safely bypass a failed first-block reinitialization. No EQ curves or plugin settings are changed by this patch.

The focused regression harness compiles the actual production filter with test doubles for file IO and FFT operations. It runs on Windows, Linux and macOS; **this does not make Equalizer APO itself cross-platform**.

```sh
cmake -S tests -B build-regression
cmake --build build-regression --config Debug
ctest --test-dir build-regression -C Debug --output-on-failure
```

On Clang/GCC, add `-DENABLE_SANITIZERS=ON` at configuration time for AddressSanitizer and UndefinedBehaviorSanitizer. See [test scope](tests/README.md).

## Building the Windows application

The existing Visual Studio solution, Qt projects and `build.bat` are retained. Consult the [upstream build guide](https://sourceforge.net/p/equalizerapo/wiki/Developer%20documentation/), but check the project files for current versions: the published guide still describes older Visual Studio/Qt versions. The current build script references Qt 6.7.2 and produces x86, x64 and ARM64 builds with FFTW, libsndfile, muParserX, TCLAP and NSIS dependencies.

The regression workflow does **not** build or package the full APO, Qt editor or installer. No modified installer has been released, and these source changes have not replaced the existing validated audio installation.

## Provenance and license

Imported from `https://git.code.sf.net/p/equalizerapo/code`, branch `main`, at `bbfcc3e5024cbb9d61ba75fc88d78605cc4c9687` (2025-11-28). This is newer than the `mirror/equalizerapo` GitHub mirror checked during the review. Since upstream is hosted on SourceForge, this GitHub repository is a history-preserving source fork, not a GitHub fork-network child of that stale mirror.

Equalizer APO's source headers specify **GPL-2.0-or-later**; the full license is in [License.txt](License.txt). Keep upstream attribution and third-party notices when redistributing. No proprietary plugins, private speaker profiles or credentials are included here.

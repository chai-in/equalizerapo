# First-pass review — 2026-10-31

Baseline: official SourceForge `main`, `bbfcc3e5024cbb9d61ba75fc88d78605cc4c9687`, following release 1.4.2. This is a targeted source review, not a complete security audit or certification. Reviewed areas include convolution/GraphicEQ loading, VST initialization, configuration loading/saving, build scripts and repository provenance.

## Implemented: convolution loading and ownership

| Trigger | Upstream behavior | Change |
| --- | --- | --- |
| Decoder reports fewer frames than metadata, then EOF/error | Read loop never advances or moves backwards | Reject incomplete responses without installing a partial filter |
| IR sample rate differs from device | Open file is not closed | RAII closes it on every exit, including exceptions |
| Normal IR load | Two `new[]` buffers use scalar `delete` | Replace temporary buffers with vectors |
| Empty, invalid or unrepresentable IR metadata | Narrowing, invalid engine lengths, division/indexing hazards | Validate dimensions and allocation multiplication before reading |
| First callback has a different block length and reopening the IR fails | Filter array is freed, then dereferenced | Recheck initialization and retain in-place bypass behavior |
| Zero-length callback | Can rebuild the convolution engine with a zero block | Return without processing or consuming the first real callback |

Positive short reads continue until the full IR has loaded. Valid IR coefficients and channel mapping are preserved. The source retains CRLF line endings to keep the patch focused.

The regression harness exercises production loading/lifecycle code. Local baseline checks reproduce the array-delete mismatch, wrong-rate handle leak and non-progress read failures; the patched harness passes all 16 cases with address/undefined-behavior sanitizers. Cross-platform CI is provided separately from the legacy full Windows application build.

## Follow-up findings, ranked

1. **VST initialization must reject invalid effects before channel arithmetic.** In `filters/VSTPluginFilter.cpp::initialize`, initialization failure sets `skipProcessing` but still reaches division by `effectChannelCount`. `helpers/VSTPluginInstance.cpp::initialize` can also clear an invalid-magic effect while returning success. Its channel getters then return zero. Next patch should add failing-plugin fixtures and test initialization, teardown, zero/negative/excessive IO counts and bypass. A source-level failure path is identified; no live malicious-plugin test was run.
2. **Configuration reads and saves need bounded retries and recoverable writes.** `FilterEngine.cpp::loadConfigFile` and `Editor/MainWindow.cpp::save` retry sharing violations indefinitely. The editor truncates the destination with `CREATE_ALWAYS` before writing. Plan cancellation/timeouts and atomic replacement with permissions/ACL preservation, plus tests for disk-full, sharing locks and interrupted saves. Windows integration testing is needed before changing these semantics.
3. **Keep initialization work off the real-time callback.** `ConvolutionFilter::process` can reopen files, allocate and plan FFTs on the first block-size mismatch. This patch prevents one failure crash but does not remove that work. Review variable-block buffering and preinitialization for both Convolution and its GraphicEQ subclass, including Bluetooth paths.
4. **Audit remaining raw array ownership and allocation failure.** `GraphicEQFilter.cpp::initializeFilters` still ends with scalar `delete buf` for an array. Add real FFT/GraphicEQ regression coverage before broad ownership conversion. `libHybridConv` uses signed `int` byte counts and unchecked low-level allocations; dimension guards here are not a complete resource-exhaustion solution.
5. **Make full Windows builds reproducible.** `build.bat` uses machine-specific paths and mixes Visual Studio/Qt generations. The wiki trails the project files. Pin source/dependency versions and checksums, then add native APO/editor/installer build jobs and separate packaging/signing. Current CI intentionally covers only the regression harness, including MSVC.
6. **Review native VST3 support as a separate feature.** The installed Edifier setup uses a compatibility plugin to host ZL Equalizer 2 and custom adaptive DSP. Native VST3 could reduce that extra layer, but needs SDK/license review, state restoration, channel layout, latency, lifetime and crash-containment tests. This pass does not copy that private bridge into the public fork or claim native VST3 support.

## Deployment status

Source changes only. The working Edifier output remains on its validated installed release and profiles. Before replacing it: build the full modified Windows APO, verify installer/rollback behavior, test actual headphone-output routing and non-silent playback, exercise profile changes, and compare latency/output against the existing installation. No new room calibration or EQ tuning is implied by this code review.

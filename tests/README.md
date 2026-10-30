# Convolution regression harness

The harness compiles `filters/ConvolutionFilter.cpp` and its real class and engine declarations. Small doubles replace libsndfile IO, FFTW calls, convolution execution, logging and allocation boundaries. The Windows precompiled-header dependencies are replaced only in this test target. The production build has no test conditionals or substituted headers.

Sixteen independently timed cases cover valid stereo IR deinterleaving and cycling to three output channels, repeated positive short reads, missing files, wrong sample rate, empty/negative frame counts, zero/negative channels, frame-count narrowing, invalid/zero blocks, premature EOF, read errors, repeated initialization, a failed reopen when the first device block is smaller, and a zero-length callback followed by the first real block. File-handle and filter-instance counts must balance. Assertions remain active in release builds.

The EOF/error double throws after repeated non-progress reads, turning the upstream infinite loop into a bounded test failure; CTest also imposes a five-second per-case timeout. AddressSanitizer catches the original array allocation/deallocation mismatch on the valid path. IO failures are injected deterministically; they do not require damaged user files.

These tests validate loading, ownership and lifecycle behavior. They do not verify libsndfile decoding, FFT accuracy, audio-engine registration, device switching, sound quality, full installer builds or real-time scheduling. The existing first-block rebuild still performs IO/allocation on the audio callback and needs a separate architectural change.

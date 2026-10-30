// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "sndfile.h"
#include "filters/ConvolutionFilter.h"
#include "helpers/LogHelper.h"

static SF_INFO metadata = {4, 48000, 2, 0, 0, 0};
static SNDFILE file;
static bool missing = false;
static int opened = 0, closed = 0, calls = 0, initialized = 0, destroyed = 0, processed = 0;
static sf_count_t position = 0, available = 4, chunk = 4;
static sf_count_t terminalRead = 0;
static std::vector<std::vector<float>> impulses;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

SNDFILE* sf_wchar_open(const wchar_t*, int, SF_INFO* info) {
  if (missing) return nullptr;
  ++opened; position = 0; *info = metadata; return &file;
}
sf_count_t sf_readf_float(SNDFILE*, float* data, sf_count_t count) {
  check(++calls < 20, "reader made no progress (would hang without test guard)");
  if (position >= available) return terminalRead;
  auto n = std::min(count, std::min(chunk, available - position));
  for (sf_count_t i = 0; i < n; ++i)
    for (int c = 0; c < metadata.channels; ++c)
      data[i * metadata.channels + c] = float((position + i) * 10 + c);
  position += n; return n;
}
int sf_close(SNDFILE*) { ++closed; return 0; }
const char* sf_strerror(SNDFILE*) { return "test IO error"; }
void fftwf_make_planner_thread_safe() {}
void* MemoryHelper::alloc(size_t n) {
  void* p = std::malloc(n); if (!p) throw std::bad_alloc(); return p;
}
void MemoryHelper::free(void* p) { std::free(p); }
void LogHelper::log(const char*, int, const void*, bool, const wchar_t*, ...) {}
void hcInitSingle(HConvSingle* f, float* h, int hlen, int flen, int) {
  check(hlen > 0 && flen > 0, "invalid convolution dimensions reached engine");
  f->framelength = flen; ++initialized; impulses.emplace_back(h, h + hlen);
}
void hcCloseSingle(HConvSingle*) { ++destroyed; }
void hcPutSingle(HConvSingle*, float*) { ++processed; }
void hcProcessSingle(HConvSingle*) {}
void hcGetSingle(HConvSingle*, float*) {}

int main(int argc, char** argv) {
  try {
    check(argc == 2, "supply a test case");
    const std::string test = argv[1];
    unsigned block = 4;
    bool valid = test == "valid" || test == "short_reads" || test == "reinitialize"
      || test == "reopen_failure" || test == "zero_process";
    if (test == "short_reads") chunk = 1;
    else if (test == "missing") missing = true;
    else if (test == "wrong_rate") metadata.samplerate = 44100;
    else if (test == "empty") metadata.frames = 0;
    else if (test == "negative_frames") metadata.frames = -1;
    else if (test == "zero_channels") metadata.channels = 0;
    else if (test == "negative_channels") metadata.channels = -1;
    else if (test == "oversized") metadata.frames = static_cast<sf_count_t>(UINT_MAX) + 1;
    else if (test == "invalid_block") block = UINT_MAX;
    else if (test == "zero_block") block = 0;
    else if (test == "eof" || test == "read_error") {
      available = 1; terminalRead = test == "eof" ? 0 : -1;
    } else check(valid, "unknown test case");
    {
      ConvolutionFilter filter(L"test.wav");
      const std::vector<std::wstring> channels = {L"L", L"R", L"C"};
      check(filter.initialize(48000, block, channels) == channels, "channel selection changed");
      check(opened == closed, "impulse response file handle leaked");
      if (valid) {
        check(initialized == 3, "expected one convolution per output channel");
        check(impulses[0] == std::vector<float>({0,10,20,30}), "left channel incorrectly deinterleaved");
        check(impulses[1] == std::vector<float>({1,11,21,31}), "right channel incorrectly deinterleaved");
        check(impulses[2] == impulses[0], "IR channel cycling changed");
      } else check(initialized == 0, "invalid/partial impulse response installed");

      float a[4] = {1,2,3,4}, b[4] = {2,3,4,5}, c[4] = {3,4,5,6};
      float* audio[] = {a,b,c};
      if (test == "reopen_failure") {
        missing = true;
        filter.process(audio, audio, 2);
        check(processed == 0 && destroyed == 3, "failed reload must bypass instead of touching freed filters");
        check(a[0] == 1 && a[1] == 2, "bypass altered audio");
      } else if (test == "zero_process") {
        filter.process(audio, audio, 0);
        check(initialized == 3 && processed == 0, "empty callback rebuilt or processed filters");
        filter.process(audio, audio, 2);
        check(initialized == 6 && processed == 3, "first real callback did not adapt block size");
      } else if (test == "reinitialize") {
        filter.initialize(48000, 4, channels);
        check(initialized == 6 && destroyed == 3, "reinitialize leaked old filters");
      } else if (valid) {
        filter.process(audio, audio, 4);
        check(processed == 3, "valid filter did not process all channels");
      } else {
        filter.process(audio, audio, 4);
        check(processed == 0 && a[0] == 1, "invalid filter did not bypass in place");
      }
    }
    check(opened == closed, "file handle count differs after destruction");
    check(initialized == destroyed, "convolution instance leaked");
    std::cout << "PASS " << test << '\n';
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

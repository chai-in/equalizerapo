#pragma once
using sf_count_t = long long;
struct SNDFILE { int unused; };
struct SF_INFO {
  sf_count_t frames;
  int samplerate, channels, format, sections, seekable;
};
constexpr int SFM_READ = 0x10;
SNDFILE* sf_wchar_open(const wchar_t*, int, SF_INFO*);
sf_count_t sf_readf_float(SNDFILE*, float*, sf_count_t);
int sf_close(SNDFILE*);
const char* sf_strerror(SNDFILE*);

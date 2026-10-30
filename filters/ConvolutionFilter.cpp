/*
	This file is part of EqualizerAPO, a system-wide equalizer.
	Copyright (C) 2015  Jonas Thedering

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "stdafx.h"

#include <cmath>
#include <climits>
#include <memory>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include <fftw3.h>
#include <sndfile.h>

#include "helpers/LogHelper.h"
#include "helpers/MemoryHelper.h"
#include "ConvolutionFilter.h"

using namespace std;

ConvolutionFilter::ConvolutionFilter(wstring filename)
{
	this->filename = filename;
	filters = NULL;
}

ConvolutionFilter::~ConvolutionFilter()
{
	cleanup();
}

vector<wstring> ConvolutionFilter::initialize(float sampleRate, unsigned maxFrameCount, vector<wstring> channelNames)
{
	cleanup();

	this->sampleRate = sampleRate;
	this->maxFrameCount = maxFrameCount;
	channelCount = (unsigned) channelNames.size();
	beforeFirstProcess = true;

	initializeFilters(maxFrameCount);

	return channelNames;
}

#pragma AVRT_CODE_BEGIN
void ConvolutionFilter::process(float** output, float** input, unsigned frameCount)
{
	if (filters == NULL || frameCount == 0)
		return;

	if (beforeFirstProcess && frameCount != maxFrameCount)
	{
		// should hopefully not happen, but happens with merged Bluetooth devices on Windows 11
		cleanup();
		initializeFilters(frameCount);
	}

	// only allow reinitialization before first process call
	beforeFirstProcess = false;

	// Reopening the impulse response can fail after a device block-size change.
	if (filters == NULL)
		return;

	for (unsigned i = 0; i < channelCount; i++)
	{
		float* inputChannel = input[i];
		float* outputChannel = output[i];
		HConvSingle* filter = &filters[i];

		hcPutSingle(filter, inputChannel);
		hcProcessSingle(filter);
		hcGetSingle(filter, outputChannel);
	}
}
#pragma AVRT_CODE_END

void ConvolutionFilter::cleanup()
{
	if (filters != NULL)
	{
		for (unsigned i = 0; i < channelCount; i++)
			hcCloseSingle(&filters[i]);

		MemoryHelper::free(filters);
		filters = NULL;
	}
}

void ConvolutionFilter::initializeFilters(unsigned frameCount)
{
	// libHybridConv uses signed int lengths and byte counts for its FFT buffers.
	if (channelCount == 0 || frameCount == 0 || frameCount > INT_MAX / sizeof(fftwf_complex) - 1)
	{
		LogF(L"Invalid convolution block size or channel count: %u frames, %u channels", frameCount, channelCount);
		return;
	}

	SF_INFO info = {};
	unique_ptr<SNDFILE, decltype(&sf_close)> inFile(sf_wchar_open(filename.c_str(), SFM_READ, &info), &sf_close);
	if (!inFile)
	{
		LogF(L"Error while reading impulse response file: %S", sf_strerror(NULL));
		return;
	}
	if (abs(sampleRate - info.samplerate) > 1.0f)
	{
		LogF(L"Impulse response sample rate (%d Hz) does not match device sample rate (%f Hz)", info.samplerate,
			sampleRate);
		return;
	}
	if (info.channels <= 0 || info.frames <= 0 || info.frames > INT_MAX - frameCount
		|| static_cast<unsigned long long>(info.frames) > vector<float>().max_size() / info.channels)
	{
		LogF(L"Invalid or oversized impulse response: %s", filename.c_str());
		return;
	}

	TraceF(L"Convolving using impulse response file %s", filename.c_str());
	unsigned fileChannelCount = (unsigned) info.channels;
	unsigned fileFrameCount = (unsigned) info.frames;
	vector<float> interleavedBuf(static_cast<size_t>(fileFrameCount) * fileChannelCount);

	sf_count_t numRead = 0;
	while (numRead < info.frames)
	{
		sf_count_t read = sf_readf_float(inFile.get(),
			interleavedBuf.data() + static_cast<size_t>(numRead) * fileChannelCount, info.frames - numRead);
		if (read <= 0 || read > info.frames - numRead)
		{
			LogF(L"Incomplete impulse response file: %s", filename.c_str());
			return;
		}
		numRead += read;
	}
	inFile.reset();

	vector<vector<float>> bufs(fileChannelCount, vector<float>(fileFrameCount));
	for (unsigned i = 0; i < fileChannelCount; i++)
	{
		for (unsigned j = 0; j < fileFrameCount; j++)
			bufs[i][j] = interleavedBuf[static_cast<size_t>(j) * fileChannelCount + i];
	}

	fftwf_make_planner_thread_safe();
	filters = (HConvSingle*) MemoryHelper::alloc(sizeof(HConvSingle) * channelCount);
	for (unsigned i = 0; i < channelCount; i++)
	{
		hcInitSingle(&filters[i], bufs[i % fileChannelCount].data(), fileFrameCount, frameCount, 1);
	}
}

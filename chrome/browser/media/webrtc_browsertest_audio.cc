// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_audio.h"

#include <stddef.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_power_monitor.h"
#include "media/audio/sounds/wav_audio_handler.h"
#include "media/base/audio_bus.h"

namespace {
// Opens |wav_filename|, reads it and loads it as a wav file. This function will
// bluntly trigger CHECKs if we can't read the file or if it's malformed. The
// caller takes ownership of the returned data. The size of the data is stored
// in |read_length|.
std::unique_ptr<char[]> ReadWavFile(const base::FilePath& wav_filename,
                                    size_t* file_length) {
  base::File wav_file(
      wav_filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!wav_file.IsValid()) {
    CHECK(false) << "Failed to read " << wav_filename.value();
    return nullptr;
  }

  size_t wav_file_length = wav_file.GetLength();

  std::unique_ptr<char[]> data(new char[wav_file_length]);
  size_t read_bytes = wav_file.Read(0, data.get(), wav_file_length);
  if (read_bytes != wav_file_length) {
    LOG(ERROR) << "Failed to read all bytes of " << wav_filename.value();
    return nullptr;
  }
  *file_length = wav_file_length;
  return data;
}
}  // namespace

namespace test {

float ComputeAudioEnergyForWavFile(const base::FilePath& wav_filename,
                                   media::AudioParameters* file_parameters) {
  // Read the file, and put its data in a scoped_ptr so it gets deleted later.
  size_t file_length = 0;
  std::unique_ptr<char[]> wav_file_data =
      ReadWavFile(wav_filename, &file_length);
  auto wav_audio_handler = media::WavAudioHandler::Create(
      base::StringPiece(wav_file_data.get(), file_length));

  std::unique_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(
      wav_audio_handler->num_channels(), wav_audio_handler->total_frames());
  base::TimeDelta file_duration = wav_audio_handler->GetDuration();

  size_t bytes_written;
  wav_audio_handler->CopyTo(audio_bus.get(), 0, &bytes_written);
  CHECK_EQ(bytes_written, wav_audio_handler->data().size())
      << "Expected to write entire file into bus.";

  // Set the filter coefficient to the whole file's duration; this will make the
  // power monitor take the entire file into account.
  media::AudioPowerMonitor power_monitor(wav_audio_handler->sample_rate(),
                                         file_duration);
  power_monitor.Scan(*audio_bus, audio_bus->frames());

  file_parameters->Reset(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(wav_audio_handler->num_channels()),
      wav_audio_handler->sample_rate(), wav_audio_handler->bits_per_sample(),
      wav_audio_handler->total_frames());
  file_parameters->set_channels_for_discrete(wav_audio_handler->num_channels());

  return power_monitor.ReadCurrentPowerAndClip().first;
}

}  // namespace test

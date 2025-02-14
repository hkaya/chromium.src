// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_COMMON_GPU_VIDEO_ACCELERATOR_UTIL_H_
#define MEDIA_GPU_IPC_COMMON_GPU_VIDEO_ACCELERATOR_UTIL_H_

#include <vector>

#include "gpu/config/gpu_info.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class GpuVideoAcceleratorUtil {
 public:
  // Convert decoder gpu capabilities to media capabilities.
  static VideoDecodeAccelerator::Capabilities
  ConvertGpuToMediaDecodeCapabilities(
      const gpu::VideoDecodeAcceleratorCapabilities& gpu_capabilities);

  // Convert decoder gpu profiles to media profiles.
  static VideoDecodeAccelerator::SupportedProfiles
  ConvertGpuToMediaDecodeProfiles(
      const gpu::VideoDecodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert decoder media capabilities to gpu capabilities.
  static gpu::VideoDecodeAcceleratorCapabilities
  ConvertMediaToGpuDecodeCapabilities(
      const VideoDecodeAccelerator::Capabilities& media_capabilities);

  // Convert decoder media profiles to gpu profiles.
  static gpu::VideoDecodeAcceleratorSupportedProfiles
  ConvertMediaToGpuDecodeProfiles(
      const VideoDecodeAccelerator::SupportedProfiles& media_profiles);

  // Convert encoder gpu profiles to media profiles.
  static VideoEncodeAccelerator::SupportedProfiles
  ConvertGpuToMediaEncodeProfiles(
      const gpu::VideoEncodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert encoder media profiles to gpu profiles.
  static gpu::VideoEncodeAcceleratorSupportedProfiles
  ConvertMediaToGpuEncodeProfiles(
      const VideoEncodeAccelerator::SupportedProfiles& media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueDecodeProfiles(
      const VideoDecodeAccelerator::SupportedProfiles& new_profiles,
      VideoDecodeAccelerator::SupportedProfiles* media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueEncodeProfiles(
      const VideoEncodeAccelerator::SupportedProfiles& new_profiles,
      VideoEncodeAccelerator::SupportedProfiles* media_profiles);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_COMMON_GPU_VIDEO_ACCELERATOR_UTIL_H_

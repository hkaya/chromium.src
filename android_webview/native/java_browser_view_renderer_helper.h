// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
#define ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_

#include <jni.h>

#include <memory>

#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

class SkCanvas;
struct AwDrawSWFunctionTable;

namespace android_webview {

class SoftwareCanvasHolder {
 public:
  static std::unique_ptr<SoftwareCanvasHolder> Create(
      jobject java_canvas,
      const gfx::Vector2d& scroll_correction,
      const gfx::Size& auxiliary_bitmap_size,
      bool force_auxiliary_bitmap);

  virtual ~SoftwareCanvasHolder() {}

  // The returned SkCanvas is still owned by this holder.
  virtual SkCanvas* GetCanvas() = 0;
};

void RasterHelperSetAwDrawSWFunctionTable(AwDrawSWFunctionTable* table);

bool RegisterJavaBrowserViewRendererHelper(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_

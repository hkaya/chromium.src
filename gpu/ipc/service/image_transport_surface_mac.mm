// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/macros.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_osmesa.h"

namespace gpu {

scoped_refptr<gfx::GLSurface> ImageTransportSurfaceCreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle handle);

namespace {

// A subclass of GLSurfaceOSMesa that doesn't print an error message when
// SwapBuffers() is called.
class DRTSurfaceOSMesa : public gfx::GLSurfaceOSMesa {
 public:
  // Size doesn't matter, the surface is resized to the right size later.
  DRTSurfaceOSMesa()
      : GLSurfaceOSMesa(gfx::GLSurface::SURFACE_OSMESA_RGBA, gfx::Size(1, 1)) {}

  // Implement a subset of GLSurface.
  gfx::SwapResult SwapBuffers() override;

 private:
  ~DRTSurfaceOSMesa() override {}
  DISALLOW_COPY_AND_ASSIGN(DRTSurfaceOSMesa);
};

gfx::SwapResult DRTSurfaceOSMesa::SwapBuffers() {
  return gfx::SwapResult::SWAP_ACK;
}

bool g_allow_os_mesa = false;

}  //  namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle surface_handle,
    gfx::GLSurface::Format format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
    case gfx::kGLImplementationDesktopGLCoreProfile:
    case gfx::kGLImplementationAppleGL:
      return ImageTransportSurfaceCreateNativeSurface(manager, stub,
                                                      surface_handle);
    default:
      // Content shell in DRT mode spins up a gpu process which needs an
      // image transport surface, but that surface isn't used to read pixel
      // baselines. So this is mostly a dummy surface.
      if (!g_allow_os_mesa) {
        NOTREACHED();
        return nullptr;
      }
      scoped_refptr<gfx::GLSurface> surface(new DRTSurfaceOSMesa());
      if (!surface.get() || !surface->Initialize(format))
        return surface;
      return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
          manager, stub, surface.get()));
  }
}

// static
void ImageTransportSurface::SetAllowOSMesaForTesting(bool allow) {
  g_allow_os_mesa = allow;
}

}  // namespace gpu

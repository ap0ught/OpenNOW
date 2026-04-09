#import "opennow/native/macos_surface_renderer.hpp"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <SDL3/SDL.h>
#import <SDL3/SDL_metal.h>

namespace opennow::native {

struct MacosSurfaceRenderer::Impl {
  SDL_MetalView metal_view = nullptr;
  CAMetalLayer* metal_layer = nil;
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> command_queue = nil;
  id<MTLRenderPipelineState> pipeline_state = nil;
  CVMetalTextureCacheRef texture_cache = nullptr;
  bool available = false;
};

MacosSurfaceRenderer::MacosSurfaceRenderer() : impl_(std::make_unique<Impl>()) {}
MacosSurfaceRenderer::~MacosSurfaceRenderer() {
  Shutdown();
}

bool MacosSurfaceRenderer::Initialize(SDL_Window* window, std::string& error) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  if (impl_->available) {
    return true;
  }

  impl_->metal_view = SDL_Metal_CreateView(window);
  if (!impl_->metal_view) {
    error = SDL_GetError();
    return false;
  }

  void* layer = SDL_Metal_GetLayer(impl_->metal_view);
  if (!layer) {
    error = "SDL_Metal_GetLayer returned null";
    return false;
  }
  impl_->metal_layer = (__bridge CAMetalLayer*)layer;
  impl_->device = MTLCreateSystemDefaultDevice();
  if (!impl_->device) {
    error = "MTLCreateSystemDefaultDevice returned null";
    return false;
  }
  impl_->command_queue = [impl_->device newCommandQueue];
  if (!impl_->command_queue) {
    error = "Failed to create Metal command queue";
    return false;
  }
  impl_->metal_layer.device = impl_->device;
  impl_->metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  impl_->metal_layer.framebufferOnly = NO;
  static NSString* shader_source =
      @"#include <metal_stdlib>\n"
       "using namespace metal;\n"
       "struct VSOut { float4 position [[position]]; float2 texCoord; };\n"
       "vertex VSOut opennow_vertex_main(uint vid [[vertex_id]]) {\n"
       "  const float2 positions[4] = { float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0) };\n"
       "  const float2 tex[4] = { float2(0.0, 1.0), float2(1.0, 1.0), float2(0.0, 0.0), float2(1.0, 0.0) };\n"
       "  VSOut out;\n"
       "  out.position = float4(positions[vid], 0.0, 1.0);\n"
       "  out.texCoord = tex[vid];\n"
       "  return out;\n"
       "}\n"
       "fragment float4 opennow_fragment_main(VSOut in [[stage_in]], texture2d<float> yTex [[texture(0)]], texture2d<float> uvTex [[texture(1)]]) {\n"
       "  constexpr sampler s(address::clamp_to_edge, filter::linear);\n"
       "  float y = yTex.sample(s, in.texCoord).r;\n"
       "  float2 uv = uvTex.sample(s, in.texCoord).rg - float2(0.5, 0.5);\n"
       "  float r = saturate(y + 1.4020 * uv.y);\n"
       "  float g = saturate(y - 0.344136 * uv.x - 0.714136 * uv.y);\n"
       "  float b = saturate(y + 1.7720 * uv.x);\n"
       "  return float4(r, g, b, 1.0);\n"
       "}\n";
  NSError* shader_error = nil;
  id<MTLLibrary> library = [impl_->device newLibraryWithSource:shader_source options:nil error:&shader_error];
  if (!library) {
    error = shader_error.localizedDescription ? shader_error.localizedDescription.UTF8String : "Failed to compile Metal shaders";
    return false;
  }
  id<MTLFunction> vertex_function = [library newFunctionWithName:@"opennow_vertex_main"];
  id<MTLFunction> fragment_function = [library newFunctionWithName:@"opennow_fragment_main"];
  if (!vertex_function || !fragment_function) {
    error = "Failed to create Metal shader functions";
    return false;
  }
  MTLRenderPipelineDescriptor* pipeline = [[MTLRenderPipelineDescriptor alloc] init];
  pipeline.vertexFunction = vertex_function;
  pipeline.fragmentFunction = fragment_function;
  pipeline.colorAttachments[0].pixelFormat = impl_->metal_layer.pixelFormat;
  NSError* pipeline_error = nil;
  impl_->pipeline_state = [impl_->device newRenderPipelineStateWithDescriptor:pipeline error:&pipeline_error];
  if (!impl_->pipeline_state) {
    error = pipeline_error.localizedDescription ? pipeline_error.localizedDescription.UTF8String : "Failed to create Metal render pipeline";
    return false;
  }
  if (CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, impl_->device, nullptr, &impl_->texture_cache) != kCVReturnSuccess) {
    error = "CVMetalTextureCacheCreate failed";
    return false;
  }
  impl_->available = true;
  return true;
}

void MacosSurfaceRenderer::Shutdown() {
  if (!impl_) {
    return;
  }
  if (impl_->texture_cache) {
    CFRelease(impl_->texture_cache);
    impl_->texture_cache = nullptr;
  }
  if (impl_->metal_view) {
    SDL_Metal_DestroyView(impl_->metal_view);
    impl_->metal_view = nullptr;
  }
  impl_->metal_layer = nil;
  impl_->pipeline_state = nil;
  impl_->command_queue = nil;
  impl_->device = nil;
  impl_->available = false;
}

bool MacosSurfaceRenderer::IsAvailable() const {
  return impl_ && impl_->available;
}

bool MacosSurfaceRenderer::SupportsNativeSurfacePath() const {
  return IsAvailable();
}

std::shared_ptr<void> MacosSurfaceRenderer::RetainNativeSurface(void* native_surface) const {
  if (!native_surface) {
    return {};
  }
  auto buffer = static_cast<CVPixelBufferRef>(native_surface);
  CFRetain(buffer);
  return std::shared_ptr<void>(buffer, [](void* value) {
    if (value) {
      CFRelease(static_cast<CVPixelBufferRef>(value));
    }
  });
}

bool MacosSurfaceRenderer::PresentNativeSurface(const std::shared_ptr<void>& native_surface, int width, int height, std::string& error) {
  (void)width;
  (void)height;
  if (!impl_ || !impl_->available) {
    error = "macOS native surface renderer is unavailable";
    return false;
  }
  if (!native_surface) {
    error = "No CVPixelBuffer surface provided";
    return false;
  }

  auto pixel_buffer = static_cast<CVPixelBufferRef>(native_surface.get());
  if (!CVPixelBufferIsPlanar(pixel_buffer) || CVPixelBufferGetPlaneCount(pixel_buffer) < 2) {
    error = "Expected 2-plane NV12 CVPixelBuffer for native surface presentation";
    return false;
  }

  impl_->metal_layer.drawableSize = CGSizeMake(width, height);
  id<CAMetalDrawable> drawable = [impl_->metal_layer nextDrawable];
  if (!drawable) {
    error = "CAMetalLayer failed to provide drawable";
    return false;
  }

  CVMetalTextureRef y_texture_ref = nullptr;
  CVMetalTextureRef uv_texture_ref = nullptr;
  const auto y_status = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault,
      impl_->texture_cache,
      pixel_buffer,
      nullptr,
      MTLPixelFormatR8Unorm,
      CVPixelBufferGetWidthOfPlane(pixel_buffer, 0),
      CVPixelBufferGetHeightOfPlane(pixel_buffer, 0),
      0,
      &y_texture_ref);
  const auto uv_status = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault,
      impl_->texture_cache,
      pixel_buffer,
      nullptr,
      MTLPixelFormatRG8Unorm,
      CVPixelBufferGetWidthOfPlane(pixel_buffer, 1),
      CVPixelBufferGetHeightOfPlane(pixel_buffer, 1),
      1,
      &uv_texture_ref);
  if (y_status != kCVReturnSuccess || uv_status != kCVReturnSuccess) {
    if (y_texture_ref) {
      CFRelease(y_texture_ref);
    }
    if (uv_texture_ref) {
      CFRelease(uv_texture_ref);
    }
    error = "CVMetalTextureCacheCreateTextureFromImage failed";
    return false;
  }

  id<MTLTexture> y_texture = CVMetalTextureGetTexture(y_texture_ref);
  id<MTLTexture> uv_texture = CVMetalTextureGetTexture(uv_texture_ref);
  if (!y_texture || !uv_texture) {
    if (y_texture_ref) {
      CFRelease(y_texture_ref);
    }
    if (uv_texture_ref) {
      CFRelease(uv_texture_ref);
    }
    error = "Failed to derive Metal textures from CVPixelBuffer";
    return false;
  }

  MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = drawable.texture;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

  id<MTLCommandBuffer> command_buffer = [impl_->command_queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
  [encoder setRenderPipelineState:impl_->pipeline_state];
  [encoder setFragmentTexture:y_texture atIndex:0];
  [encoder setFragmentTexture:uv_texture atIndex:1];
  [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
  [encoder endEncoding];
  [command_buffer presentDrawable:drawable];
  [command_buffer commit];

  if (y_texture_ref) {
    CFRelease(y_texture_ref);
  }
  if (uv_texture_ref) {
    CFRelease(uv_texture_ref);
  }
  CVMetalTextureCacheFlush(impl_->texture_cache, 0);
  return true;
}

}  // namespace opennow::native

#endif

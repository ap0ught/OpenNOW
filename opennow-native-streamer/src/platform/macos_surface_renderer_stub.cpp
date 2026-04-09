#include "opennow/native/macos_surface_renderer.hpp"

namespace opennow::native {

struct MacosSurfaceRenderer::Impl {};

MacosSurfaceRenderer::MacosSurfaceRenderer() = default;
MacosSurfaceRenderer::~MacosSurfaceRenderer() = default;

bool MacosSurfaceRenderer::Initialize(SDL_Window* window, std::string& error) {
  (void)window;
  error = "macOS native surface renderer is unavailable on this platform build";
  return false;
}

void MacosSurfaceRenderer::Shutdown() {}

bool MacosSurfaceRenderer::IsAvailable() const {
  return false;
}

bool MacosSurfaceRenderer::SupportsNativeSurfacePath() const {
  return false;
}

std::shared_ptr<void> MacosSurfaceRenderer::RetainNativeSurface(void* native_surface) const {
  (void)native_surface;
  return {};
}

bool MacosSurfaceRenderer::PresentNativeSurface(const std::shared_ptr<void>& native_surface, int width, int height, std::string& error) {
  (void)native_surface;
  (void)width;
  (void)height;
  error = "macOS native surface renderer is unavailable on this platform build";
  return false;
}

}  // namespace opennow::native

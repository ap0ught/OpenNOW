#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;

namespace opennow::native {

class MacosSurfaceRenderer {
 public:
  MacosSurfaceRenderer();
  ~MacosSurfaceRenderer();

  bool Initialize(SDL_Window* window, std::string& error);
  void Shutdown();

  bool IsAvailable() const;
  bool SupportsNativeSurfacePath() const;
  std::shared_ptr<void> RetainNativeSurface(void* native_surface) const;
  bool PresentNativeSurface(const std::shared_ptr<void>& native_surface, int width, int height, std::string& error);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace opennow::native

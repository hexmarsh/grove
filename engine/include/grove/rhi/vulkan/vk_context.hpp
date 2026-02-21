#pragma once
#include "grove/core/core.hpp"
#include <span>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_profiles.hpp>

namespace grove
{
    class Window;

    class VKContext
    {
    public:
        static Result<VKContext> Create(const Window& window);

        [[nodiscard]] const vk::raii::Instance& GetInstance() const;
        static constexpr VpProfileProperties GetVPProfileProperties()
        {
            return VpProfileProperties
            {
                VP_KHR_ROADMAP_2024_NAME,
                VP_KHR_ROADMAP_2024_SPEC_VERSION
            };
        }

        const vk::raii::SurfaceKHR& GetSurface() const;

    private:
        Status CreateInstance();
        Status CreateSurface(const Window& window);
        Status SetupDebugMessenger();
        Status AreLayersSupported(std::span<const char* const> requiredLayers) const;
        Status AreExtensionsSupported(std::span<const char* const> requiredExtensions) const;

    private:
        vk::raii::Context                context_;
        vk::raii::Instance               instance_       { nullptr };
        vk::raii::DebugUtilsMessengerEXT debugMessenger_ { nullptr };
        vk::raii::SurfaceKHR             surface_        { nullptr };
    };
}

#pragma once
#include "grove/core/typedefs.hpp"
#include "grove/core/error.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace grove
{
	class VKContext;
	class Window;
	class CommandBuffer;
	class BarrierCommand;

	struct VKDeviceCreateInfo
	{
		const VKContext* vkContext{ nullptr };
		const Window* window{ nullptr };
	};

	class VKDevice
	{
	public:
		static Result<VKDevice> Create(const VKDeviceCreateInfo& createInfo);

		VKDevice();
		~VKDevice();

		VKDevice(const VKDevice&) = delete;
		VKDevice& operator=(const VKDevice&) = delete;

		VKDevice(VKDevice&& other) noexcept;
		VKDevice& operator=(VKDevice&& other) noexcept;

		Status BeginFrame();
		Status EndFrame();

		Status RecordCommands(const CommandBuffer& commandBuffer);

	private:
		// Nested types
		struct QueueFamilyIndices
		{
			u32 graphics;
			u32 present;
		};

		struct Queues
		{
			vk::raii::Queue graphicsQueue { nullptr };
			vk::raii::Queue presentQueue  { nullptr };
		};

		struct Swapchain
		{
			vk::raii::SwapchainKHR           swapchain{ nullptr };
			std::vector<vk::Image>           images;
			std::vector<vk::raii::ImageView> imageViews;
			vk::SurfaceFormatKHR             surfaceFormat;
			vk::Extent2D                     extent;
		};

		struct GraphicsPipeline
		{
			vk::raii::PipelineLayout layout{ nullptr };
			vk::raii::Pipeline       pipeline{ nullptr };
		};

		struct CommandData
		{
			vk::raii::CommandPool                commandPool { nullptr };
			std::vector<vk::raii::CommandBuffer> commandBuffers;
		};

		struct Semaphores
		{
			std::vector<vk::raii::Semaphore>     imageAvailableSemaphores;
			std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
		};

		struct Fences
		{
			std::vector<vk::raii::Fence> inFlightFences;
		};

		struct PerFrame
		{
			u32   currentFrame            { 0 };
			u32   currentImageIndex       { static_cast<u32>(-1) };
			const u8 MAX_FRAMES_IN_FLIGHT { 2 };
		};

		// Initialization methods
		Status PickPhysicalDevice();
		Status CreateLogicalDevice();
		Status CreateSwapChain();
		Status CreateImageViews();
		Status CreateGraphicsPipeline();
		Status CreateCommandPool();
		Status CreateCommandBuffers();
		Status CreateSyncObjects();

		Status RecordCommandBuffer(u32 imageIndex);

		struct TransitionInfo
		{
			grove::u32 imageIndex;
			vk::ImageLayout oldLayout;
			vk::ImageLayout newLayout;
			vk::AccessFlags2 srcAccessMask;
			vk::AccessFlags2 dstAccessMask;
			vk::PipelineStageFlags2 srcStageMask;
			vk::PipelineStageFlags2 dstStageMask;
		};

		Status TransitionImageLayout(const TransitionInfo& transitionInfo);
		TransitionInfo GetTransitionInfo(const BarrierCommand& barrierCommand);

		// Shader helpers
		[[nodiscard]] Result<std::vector<char>> ReadFile(const std::string& filename) const;
		[[nodiscard]] Result<vk::raii::ShaderModule> CreateShaderModule(const std::vector<char>& code) const;

		// Helper methods - Physical device selection
		u64 RankPhysicalDevice(const vk::raii::PhysicalDevice& physicalDevice) const;
		Status GetQueueFamilyIndices();

		// Helper methods - Swapchain configuration
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const;
		vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes, vk::PresentModeKHR requestedMode) const;
		vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

		// Member variables - Context references
		const VKContext* vkContext_{ nullptr };
		const Window* window_{ nullptr };

		// Member variables - Vulkan objects
		vk::raii::PhysicalDevice physicalDevice_ { nullptr };
		vk::raii::Device         device_         { nullptr };
		QueueFamilyIndices       queueFamilyIndices_;
		Queues                   queues_;
		Swapchain                swapchain_;
		GraphicsPipeline         graphicsPipeline_;
		CommandData              commandData_;
		Semaphores               semaphores_;
		Fences                   fences_;
		PerFrame                 perFrame_;
	};
}

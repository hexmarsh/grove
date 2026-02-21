#pragma once
#include "grove/rhi/color.hpp"

namespace grove
{
	enum class CommandType : u8
	{
		Invalid = 0,
		BeginRendering,
		EndRendering,
		Barrier,
		Draw
	};

	struct BeginRenderingCommand
	{
		RGBAColor clearColor;
	};

	enum class ResourceState : u8
	{
		Undefined = 0,
		General,
		RenderTarget,
		Present
	};

	using ResourceHandle = u32;
	struct BarrierCommand
	{
		ResourceHandle handle;
		ResourceState before;
		ResourceState after;
	};

	struct DrawCommand
	{
		u32 vertexCount   { 0 };
		u32 instanceCount { 1 };
		u32 firstVertex   { 0 };
		u32 firstInstance { 0 };
	};

	union CommandPayload
	{
		BeginRenderingCommand beginRendering;
		BarrierCommand        barrier;
		DrawCommand           draw;
	};

	struct Command
	{
		CommandType    type = CommandType::Invalid;
		CommandPayload payload;
	};
}
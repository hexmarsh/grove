#pragma once
#include "grove/core/typedefs.hpp"
#include "grove/rhi/command.hpp"
#include <vector>

namespace grove
{
	class CommandBuffer
	{
	public:
		void Reset() { commands_.clear(); }

		void BeginRendering(RGBAColor clearColor)
		{
			commands_.push_back(
				Command
				{
					.type = CommandType::BeginRendering,
					.payload = { .beginRendering = { .clearColor = clearColor }}
				}
			);
		}

		void EndRendering()
		{
			commands_.push_back(Command{ .type = CommandType::EndRendering });
		}

		void Barrier(const BarrierCommand& barrierCommand)
		{
			Command barrier
			{
				.type = CommandType::Barrier,
				.payload =
				{
					.barrier = barrierCommand
                }
			};

			commands_.push_back(barrier);
		}

		void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0)
		{
			commands_.push_back(Command
				{
					.type = CommandType::Draw,
					.payload = {.draw = { vertexCount, instanceCount, firstVertex, firstInstance }}
				});
		}

		const std::vector<Command>& GetCommands() const { return commands_; }
	private:
		std::vector<Command> commands_;
	};
}


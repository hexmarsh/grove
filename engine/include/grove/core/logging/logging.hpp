#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace grove
{
	class LogManager
	{
	public:
		static void Init();
		static std::shared_ptr<spdlog::logger>& GetLogger();
		static std::shared_ptr<spdlog::logger>& GetFunctionLogger();
	};
}


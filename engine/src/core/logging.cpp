#include "grove/core/logging.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace grove
{
	namespace
	{
		std::shared_ptr<spdlog::logger> logger_ = nullptr;
		std::shared_ptr<spdlog::logger> functionLogger = nullptr;
	}

	void LogManager::Init()
	{
		logger_ = spdlog::stderr_color_mt("console");
		logger_->set_pattern("[%^%l%$] %v");
		logger_->set_level(spdlog::level::trace);

		functionLogger = spdlog::stderr_color_mt("function");
		functionLogger->set_pattern("[%^%l%$] %v\n\t\tat: %! (%s:%#)");
		functionLogger->set_level(spdlog::level::trace);
	}

	std::shared_ptr<spdlog::logger>& LogManager::GetLogger()
	{
		return logger_;
	}

	std::shared_ptr<spdlog::logger>& LogManager::GetFunctionLogger()
	{
		return functionLogger;
	}
}

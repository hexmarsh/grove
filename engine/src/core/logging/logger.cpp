#include "grove/core/logging/logger.hpp"
#include "grove/core/logging/log_channels.hpp"

namespace grove::log
{
	namespace
	{
		Logger* logger_ = nullptr;
		Logger defaultLogger_{ level::info };
	}

	void LogManager::Init(Logger& logger)
	{
		logger_ = &logger;
		logger.Log(grove::log::channel::System, level::debug, "LogManager initialized");
	}

	void LogManager::Shutdown()
	{
		if (logger_)
		{
			logger_->Log(grove::log::channel::System, level::debug, "LogManager terminated");
		}
		else
		{
			defaultLogger_.Log(grove::log::channel::System, level::debug, "LogManager terminated (default logger)");
		}

		logger_ = nullptr;
	}

	Logger& LogManager::GetLogger()
	{
		return logger_ ? *logger_ : defaultLogger_;
	}
}

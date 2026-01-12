#include "grove/core/logging/logger.hpp"
#include "grove/core/logging/log_channels.hpp"

namespace grove::log
{
	namespace
	{
		Logger* logger_ = nullptr;
		Logger defaultLogger_{ Level::Info };
	}

	void LogManager::Init(Logger& logger)
	{
		logger_ = &logger;
		logger.Log(grove::log::channel::System, Level::Debug, "event=logManager.initialized level={}", ToString(logger_->GetMinLogLevel()));
	}

	void LogManager::Shutdown()
	{
		if (logger_)
		{
			logger_->Log(grove::log::channel::System, Level::Debug, "event=logManager.terminated loggerType=custom");
		}
		else
		{
			defaultLogger_.Log(grove::log::channel::System, Level::Debug, "event=logManager.terminated loggerType=default");
		}

		logger_ = nullptr;
	}

	Logger& LogManager::GetLogger()
	{
		return logger_ ? *logger_ : defaultLogger_;
	}
}

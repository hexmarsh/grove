#pragma once

#include <format>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>

#include "grove/core/typedefs.hpp"

namespace grove::log
{
	enum class level : u8
	{
		trace = 0,
		debug ,
		info,
		warn,
		error,
		fatal,
		none
	};

	constexpr std::string_view LogLevelToString(level level) noexcept
	{
		switch (level)
		{
		case level::trace: return "Trace";
		case level::debug: return "Debug";
		case level::info:  return "Info";
		case level::warn:  return "Warn";
		case level::error: return "Error";
		case level::fatal: return "Fatal";
		default:              return "Unknown";
		}
	}

	class Logger
	{
	public:
		explicit Logger(level minLevel = level::info)
			: minLogLevel_(minLevel), enabled_(true)
		{}

		template<typename... Args>
		void Log(std::string_view channel, level lvl, std::format_string<Args...> fmt, Args&& ...args)
		{
			if (!enabled_ || lvl < minLogLevel_)
			{
				return;
			}

			std::scoped_lock lock{ mutex_ };

			std::cout << std::format(
				"[{}][{}] {}\n",
				channel,
				LogLevelToString(lvl),
				std::format(fmt, std::forward<Args>(args)...)
			);
		}

		void SetMinLevel(level level) noexcept
		{
			minLogLevel_ = level;
		}

		void Enable() { enabled_ = true; }
		void Disable() { enabled_ = false; }

		[[nodiscard]] level GetMinLogLevel() const noexcept
		{
			return minLogLevel_;
		}

		[[nodiscard]] bool IsEnabled() const noexcept
		{
			return enabled_;
		}

	private:
		std::mutex mutex_;
		level minLogLevel_;
		bool enabled_;
	};

	class LogManager
	{
	public:
		LogManager() = delete;
		~LogManager() = delete;
		LogManager(const LogManager&) = delete;
		LogManager& operator=(const LogManager&) = delete;
		LogManager(LogManager&&) = delete;
		LogManager& operator=(LogManager&&) = delete;

		static void Init(Logger& logger);
		static void Shutdown();
		static Logger& GetLogger();
	};
}


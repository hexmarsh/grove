#pragma once

#include <format>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>

#include "grove/core/typedefs.hpp"

namespace grove::log
{
	enum class Level : u8
	{
		Trace = 0,
		Debug ,
		Info,
		Warn,
		Error,
		Fatal,
		None
	};

	constexpr std::string_view ToString(Level level) noexcept
	{
		switch (level)
		{
			using enum Level;
		case Trace: return "T";
		case Debug: return "D";
		case Info:  return "I";
		case Warn:  return "W";
		case Error: return "E";
		case Fatal: return "F";
		default:    return "Unknown";
		}
	}

	constexpr std::string_view ToColorCode(Level level)
	{
		switch (level)
		{
			using enum Level;
		case Trace: return "90";
		case Debug: return "36";
		case Info:  return "32";
		case Warn:  return "33";
		case Error: return "31";
		case Fatal: return "1;31";
		default:    return "0";
		}
	}

	class Logger
	{
	public:
		explicit Logger(Level minLevel = Level::Info)
			: minLogLevel_(minLevel), enabled_(true)
		{}

		template<typename... Args>
		void Log(std::string_view channel, Level lvl, std::format_string<Args...> fmt, Args&& ...args)
		{
			if (!enabled_ || lvl < minLogLevel_)
			{
				return;
			}

			std::scoped_lock lock{ mutex_ };

			std::cout << std::format(
				"\033[{}m[{}][{}] {}\033[0m\n",
				ToColorCode(lvl),
				channel,
				ToString(lvl),
				std::format(fmt, std::forward<Args>(args)...)
			);
		}

		void SetMinLevel(Level level) noexcept
		{
			minLogLevel_ = level;
		}

		void Enable() { enabled_ = true; }
		void Disable() { enabled_ = false; }

		[[nodiscard]] Level GetMinLogLevel() const noexcept
		{
			return minLogLevel_;
		}

		[[nodiscard]] bool IsEnabled() const noexcept
		{
			return enabled_;
		}

	private:
		std::mutex mutex_;
		Level minLogLevel_;
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


#pragma once

#include "grove/core/logging/logger.hpp"
#include "grove/core/logging/log_channels.hpp"

#define GRV_DECLARE_LOG_CHANNEL(name) \
	namespace grove::log::channel { inline constexpr std::string_view name = #name; }

#define GRV_CHANNEL(name) (::grove::log::channel::name)
#define GRV_LOG_LEVEL(lvl) (::grove::log::Level::lvl)

#define GRV_LOG_IMPL(channelExpr, lvl, fmt, ...)                                                              \
	do                                                                                                        \
	{                                                                                                         \
		::grove::log::LogManager::GetLogger().Log(channelExpr, GRV_LOG_LEVEL(lvl), fmt, ##__VA_ARGS__); \
	} while (0)

#define GRV_LOG(channel, lvl, fmt, ...) GRV_LOG_IMPL(channel, lvl, fmt, ##__VA_ARGS__)

#define GRV_LOG_TRACE(channel, fmt, ...) GRV_LOG_IMPL(channel, Trace, fmt, ##__VA_ARGS__)
#define GRV_LOG_DEBUG(channel, fmt, ...) GRV_LOG_IMPL(channel, Debug, fmt, ##__VA_ARGS__)
#define GRV_LOG_INFO(channel, fmt, ...)  GRV_LOG_IMPL(channel, Info, fmt, ##__VA_ARGS__)
#define GRV_LOG_WARN(channel, fmt, ...)  GRV_LOG_IMPL(channel, Warn, fmt, ##__VA_ARGS__)
#define GRV_LOG_ERROR(channel, fmt, ...) GRV_LOG_IMPL(channel, Error, fmt, ##__VA_ARGS__)
#define GRV_LOG_FATAL(channel, fmt, ...) GRV_LOG_IMPL(channel, Fatal, fmt, ##__VA_ARGS__)

#define GRV_SET_LOG_LEVEL(lvl) ::grove::log::LogManager::GetLogger().SetMinLevel(GRV_LOG_LEVEL(lvl))



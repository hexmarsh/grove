#pragma once

#define GRV_LOG_TRACE(...) \
	SPDLOG_LOGGER_TRACE(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_DEBUG(...) \
	SPDLOG_LOGGER_DEBUG(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_INFO(...) \
	SPDLOG_LOGGER_INFO(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_WARN(...) \
	SPDLOG_LOGGER_WARN(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_ERROR(...) \
	SPDLOG_LOGGER_ERROR(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_FATAL(...) \
	SPDLOG_LOGGER_CRITICAL(::grove::LogManager::GetLogger(), __VA_ARGS__)

#define GRV_LOG_TRACE_FUNC(...) \
	SPDLOG_LOGGER_TRACE(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

#define GRV_LOG_DEBUG_FUNC(...) \
	SPDLOG_LOGGER_DEBUG(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

#define GRV_LOG_INFO_FUNC(...) \
	SPDLOG_LOGGER_INFO(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

#define GRV_LOG_WARN_FUNC(...) \
	SPDLOG_LOGGER_WARN(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

#define GRV_LOG_ERROR_FUNC(...) \
	SPDLOG_LOGGER_ERROR(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

#define GRV_LOG_FATAL_FUNC(...) \
	SPDLOG_LOGGER_CRITICAL(::grove::LogManager::GetFunctionLogger(), __VA_ARGS__)

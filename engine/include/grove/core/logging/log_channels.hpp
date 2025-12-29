#pragma once
#include <string_view>

namespace grove::log::channel
{
	inline constexpr std::string_view Assert = "Assert";
	inline constexpr std::string_view System = "System";
	inline constexpr std::string_view Core   = "Core";
}

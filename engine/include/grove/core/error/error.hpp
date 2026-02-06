#pragma once
#include <expected>

namespace grove
{
	enum class Error
	{
		Ok,
		Failed,
		CantCreate
	};

	using enum Error;

	template<typename T>
	using Result = std::expected<T, Error>;
}


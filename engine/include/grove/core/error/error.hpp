#pragma once
#include <expected>

namespace grove
{
	enum class Error
	{
		Failed,
		NotFound,
		CantCreate,
		FileCantOpen
	};

	using enum Error;

	template<typename T>
	using Result = std::expected<T, Error>;
	using Status = Result<void>;
}


#pragma once
#include "grove/core/typedefs.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace grove
{
	template<u32 N>
	struct VecStorage;

	template<u32 N>
	struct Vector : VecStorage<N>
	{
		using Base = VecStorage<N>;
		using Base::v;
		using Base::Base;

		constexpr Vector() noexcept = default;

		constexpr Vector& operator+=(const Vector& rhs) noexcept 
		{ 
			v += rhs.v; 
			return *this; 
		}

		constexpr Vector& operator-=(const Vector& rhs) noexcept 
		{ 
			v -= rhs.v; 
			return *this; 
		}

		constexpr Vector operator+(const Vector& rhs) const noexcept 
		{ 
			Vector result = *this;
			result.v += rhs.v;
			return result;
		}

		constexpr Vector operator-(const Vector& rhs) const noexcept 
		{ 
			Vector result = *this;
			result.v -= rhs.v;
			return result;
		}

		constexpr Vector operator*(f32 scalar) const noexcept 
		{ 
			Vector result = *this;
			result.v *= scalar;
			return result;
		}

		constexpr Vector operator/(f32 scalar) const noexcept 
		{ 
			Vector result = *this;
			result.v /= scalar;
			return result;
		}

		constexpr Vector& operator*=(f32 scalar) noexcept 
		{ 
			v *= scalar; 
			return *this; 
		}

		constexpr Vector& operator/=(f32 scalar) noexcept 
		{ 
			v /= scalar; 
			return *this; 
		}

		[[nodiscard]] f32 Magnitude() const noexcept { return glm::length(v); }
		[[nodiscard]] f32 MagnitudeSquared() const noexcept { return glm::length2(v); }

		[[nodiscard]] std::string ToString() const { return glm::to_string(v); }
	};

	template<>
	struct VecStorage<2>
	{
		union
		{
			struct { f32 x, y; };
			glm::vec2 v;
		};

		constexpr VecStorage(f32 x = 0, f32 y = 0) noexcept : x(x), y(y) {}
	};

	template<>
	struct VecStorage<3>
	{
		union
		{
			struct { f32 x, y, z; };
			glm::vec3 v;
		};

		constexpr VecStorage(f32 x = 0, f32 y = 0, f32 z = 0) noexcept : x(x), y(y), z(z) {}
	};

	template<>
	struct VecStorage<4>
	{
		union
		{
			struct { f32 x, y, z, w; };
			glm::vec4 v;
		};

		constexpr VecStorage(f32 x = 0, f32 y = 0, f32 z = 0, f32 w = 0) noexcept : x(x), y(y), z(z), w(w) {}
	};

	using Vector2 = Vector<2>;
	using Vector3 = Vector<3>;
	using Vector4 = Vector<4>;

	static_assert(sizeof(Vector2) == sizeof(glm::vec2));
	static_assert(sizeof(Vector3) == sizeof(glm::vec3));
	static_assert(sizeof(Vector4) == sizeof(glm::vec4));
}

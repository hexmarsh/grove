#pragma once

#include <utility>

namespace grove
{
	template <class T>
	class BoxPtr
	{
	public:
		BoxPtr() noexcept
			: resource_(nullptr)
		{}

		BoxPtr(std::nullptr_t) noexcept
			: resource_(nullptr)
		{}

		explicit BoxPtr(T* ptr) noexcept
			: resource_(ptr)
		{}

		// BoxPtr<T>(BoxPtr<U>)
		template <class U>
			requires std::is_convertible_v<U*, T*>
		BoxPtr(BoxPtr<U>&& other) noexcept
			: resource_(other.Release())
		{}

		// BoxPtr<T> = BoxPtr<U>
		template <class U>
			requires std::is_convertible_v<U*, T*>
		BoxPtr& operator=(BoxPtr<U>&& other) noexcept
		{
			Reset(other.Release());
			return *this;
		}

		~BoxPtr() noexcept
		{
			delete resource_;
			resource_ = nullptr;
		}

		BoxPtr(const BoxPtr&) = delete;
		BoxPtr& operator=(const BoxPtr&) = delete;

		BoxPtr(BoxPtr&& other)
			: resource_(std::exchange(other.resource_, nullptr))
		{}

		BoxPtr& operator=(BoxPtr&& other) noexcept
		{
			if (this != &other)
			{
				Reset(std::exchange(other.resource_, nullptr));
			}
			return *this;
		}

		template <class... Args>
		static BoxPtr<T> Create(Args &&...args)
		{
			return BoxPtr<T>(new T(std::forward<Args>(args)...));
		}

		T* Get() noexcept { return resource_; }
		const T* Get() const noexcept { return resource_; }

		explicit operator bool() const noexcept { return resource_ != nullptr; }

		T& operator*() noexcept { return *resource_; }
		const T& operator*() const noexcept { return *resource_; }

		T* operator->() noexcept { return resource_; }
		const T* operator->() const noexcept { return resource_; }

		T* Release() noexcept { return std::exchange(resource_, nullptr); }

		void Reset(T* ptr) noexcept
		{
			if (resource_ != ptr)
			{
				delete resource_;
				resource_ = ptr;
			}
		}

		void Reset() noexcept
		{
			Reset(nullptr);
		}

	private:
		T* resource_ = nullptr;
	};
}
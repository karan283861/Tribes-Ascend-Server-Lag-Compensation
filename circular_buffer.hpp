#pragma once

#include <memory>

template <typename T>
class CircularBuffer
{
	size_t capacity_{};
	size_t size_{};
	std::unique_ptr<T[]> data_{};
	size_t back_index_{};

	void AddedElement(void)
	{
		back_index_ = (back_index_ + 1) % capacity_;
		if (size_ < capacity_)
		{
			size_++;
		}
	}

public:
	CircularBuffer(size_t Capacity) : capacity_(Capacity), data_(std::make_unique<T[]>(Capacity))
	{
	}

	void push_back(T &t)
	{
		data_[back_index_] = t;
		AddedElement();
	}

	void push_back(T &&t)
	{
		data_[back_index_] = std::move(t);
		AddedElement();
	}

	template <typename... Args>
	void emplace_back(Args &&...args)
	{
		if (size_ >= capacity_)
		{
			(&data_[back_index_])->~T();
		}
		new (static_cast<void *>(&data_[back_index_])) T(std::forward<Args>(args)...);
		AddedElement();
	}

	// The latest inserted element will be at index 0.
	T &at(size_t index) const
	{
		size_t internal_index{back_index_ - 1 - index};
		if ((1 + index) > back_index_)
		{
			internal_index += capacity_;
		}
		return data_[internal_index];
	}

	T &back(void)
	{
		return at(0);
	}

	T &operator[](size_t index) const
	{
		return at(index);
	}

	size_t size(void) const
	{
		return size_;
	}

	size_t capacity(void) const
	{
		return capacity_;
	}
};
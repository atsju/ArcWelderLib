////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Gcode Processor Library
//
// Tools for parsing gcode and calculating printer state from parsed gcode commands.

//
// Copyright(C) 2020 - Brad Hochgesang
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU Affero General Public License for more details.
//
//
// You can contact the author at the following email address: 
// FormerLurker@pm.me
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <exception>
template <typename T>
class array_list
{
public:
	array_list()
	{
		auto_grow_ = true;
		max_size_ = 50;
		front_index_ = 0;
		count_ = 0;
		items_ = new T[max_size_];
	}
	array_list(int max_size)
	{
		auto_grow_ = false;
		max_size_ = max_size;
		front_index_ = 0;
		count_ = 0;
		items_ = new T[max_size];
	}
	virtual ~array_list() {
		delete[] items_;
	}
	void resize(int max_size)
	{
		T* new_items = new T[max_size];
		for (int index = 0; index < count_; index++)
		{
			new_items[index] = items_[(front_index_ + index + max_size_) % max_size_];
		}
		front_index_ = 0;
		delete[] items_;
		items_ = new_items;
		max_size_ = max_size;
	}
	void push_front(T object)
	{
		if (count_ == max_size_)
		{
			if (auto_grow_)
			{
				resize(max_size_ * 2);
			}
			else {
				throw std::exception();
			}
		}
		front_index_ = (front_index_ - 1 + max_size_) % max_size_;
		count_++;
		items_[front_index_] = object;
	}
	void push_back(T object)
	{
		if (count_ == max_size_)
		{
			if (auto_grow_)
			{
				resize(max_size_ * 2);
			}
			else {
				throw std::exception();
			}
		}
		items_[(front_index_ + count_ + max_size_) % max_size_] = object;
		count_++;
	}
	T pop_front()
	{
		if (count_ == 0)
		{
			throw std::exception();
		}

		int prev_start = front_index_;
		front_index_ = (front_index_ + 1 + max_size_) % max_size_;
		count_--;
		return items_[prev_start];
	}

	T pop_back()
	{
		if (count_ == 0)
		{
			throw std::exception();
		}

		return items_[--count_];
	}
	T& operator[](int index)
	{
		return items_[(front_index_ + index + max_size_) % max_size_];
	}
	const T& operator[] (const int index) const
	{
		return items_[(front_index_ + index + max_size_) % max_size_];
	}

	const T get(int index)
	{
		return items_[(front_index_ + index + max_size_) % max_size_];
	}
	
	int  count() const
	{
		return count_;

	}
	int  get_max_size()
	{
		return max_size_;
	}
	void clear()
	{
		count_ = 0;
		front_index_ = 0;
	}
	void copy(const array_list<T>& source)
	{
		if (max_size_ < source.max_size_)
		{
			resize(source.max_size_);
		}
		clear();
		for (int index = 0; index < source.count_; index++)
		{
			items_[index] = source[index];
		}
		front_index_ = source.front_index_;
		count_ = source.count_;
	}

protected:
	T* items_;
	int  max_size_;
	int  front_index_;
	int  count_;
	bool auto_grow_;
};
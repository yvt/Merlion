/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <type_traits>
#include <algorithm>
#include <cstring>

namespace vslim
{
	template <class T>
	class vector_slim
	{
		static_assert(std::is_pod<T>::value, "Element type must be POD.");
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using const_reference = const value_type&;
		using iterator = value_type *;
		using const_iterator = const value_type *;
		
		inline vector_slim() { }
		inline vector_slim(std::size_t nelems)
		{
			if (nelems) {
				storage_ = new T[nelems];
				capacity_ = size_ = nelems;
			}
		}
		inline vector_slim(const vector_slim &v) :
		vector_slim(v.size())
		{
			if (size())
				std::memcpy(storage_, v.storage_, sizeof(T) * v.size());
		}
		inline vector_slim(vector_slim &&v) :
		storage_(v.storage_),
		size_(v.size_),
		capacity_(v.capacity_)
		{
			v.storage_ = nullptr;
			v.size_ = v.capacity_ = 0;
		}
		void operator = (const vector_slim &v)
		{
			if (this == &v) return;
			if (storage_) {
				delete[] storage_;
				storage_ = nullptr;
			}
			capacity_ = size_ = v.size();
			if (size_) {
				storage_ = new T[size_];
				std::memcpy(storage_, v.storage_, sizeof(T) * size_);
			}
		}
		void operator = (vector_slim &&v)
		{
			if (this == &v) return;
			if (storage_) {
				delete[] storage_;
				storage_ = nullptr;
			}
			std::swap(storage_, v.storage_);
			std::swap(capacity_, v.capacity_);
			std::swap(size_, v.size_);
		}
		~vector_slim()
		{
			if (storage_) {
				delete[] storage_;
				storage_ = nullptr;
			}
		}
		
		inline const T& operator [] (std::size_t index) const
		{
			assert(index < size_);
			return storage_[index];
		}
		inline T& operator [] (std::size_t index)
		{
			assert(index < size_);
			return storage_[index];
		}
		
		inline std::size_t size() const { return size_; }
		inline std::size_t capacity() const { return capacity_; }
		
		void reserve(std::size_t newCapacity)
		{
			if (newCapacity > capacity()) {
				std::size_t newSize = std::max<std::size_t>(capacity(), 16);
				while (newSize < newCapacity) newSize <<= 1;
				T *newStorage = new T[newSize];
				if (storage_) {
					std::memcpy(newStorage, storage_, size_ * sizeof(T));
					delete[] storage_;
				}
				storage_ = newStorage;
				capacity_ = newSize;
			}
		}
		
		inline void resize(std::size_t newSize)
		{
			reserve(newSize);
			size_ = newSize;
		}
		
		inline const T& front() const { return *this[0]; }
		inline T& front() { return *this[0]; }
		inline const T& back() const { return *this[size() - 1]; }
		inline T& back() { return *this[size() - 1]; }
		
		inline T *data() { return storage_; }
		inline const T *data() const { return storage_; }
		
		inline iterator begin() { return data(); }
		inline const_iterator begin() const { return data(); }
		inline const_iterator cbegin() const { return data(); }
		
		inline iterator end() { return data() + size(); }
		inline const_iterator end() const { return data() + size(); }
		inline const_iterator cend() const { return data() + size(); }
		
	private:
		T *storage_ = nullptr;
		std::size_t capacity_ = 0;
		std::size_t size_ = 0;
	};
}
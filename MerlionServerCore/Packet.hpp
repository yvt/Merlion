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

#include <vector>
#include "Exceptions.hpp"
#include <cstdint>
#include <type_traits>
#include <cstring>

namespace mcore
{
    class PacketReader
    {
        char *cursor;
        std::size_t length;
    public:
        inline PacketReader(char *cursor, std::size_t length):
                cursor(cursor), length(length) { }
		
		inline bool canReadBytes(std::size_t bytes)
		{
			return bytes <= length;
		}

        inline const void *readBytes(std::size_t bytes)
        {
            if (bytes > length) {
                MSCThrow(InvalidDataException("EOF detected while reading a packet data."));
            }
            void *ptr = cursor;
            cursor += bytes;
            length -= bytes;
            return ptr;
        }

        template <class T> inline
        typename std::enable_if<std::is_pod<T>::value,const T*>::type
        readElements(std::size_t count)
        {
            return reinterpret_cast<const T*>(readBytes(sizeof(T) * count));
        }

        template <class T> inline
        typename std::enable_if<std::is_pod<T>::value,const T&>::type
        read()
        {
            return *reinterpret_cast<const T*>(readBytes(sizeof(T)));
        }

        inline std::string readString()
        {
            auto len = read<std::uint32_t>();
            return std::string(readElements<char>(len), len);
        }
    };

    class PacketWriterVectorTarget
    {
        std::vector<char> _vector;
    public:
        PacketWriterVectorTarget() { }
        std::vector<char>& vector() { return _vector; }
        inline void *write(std::size_t bytes)
        {
            auto originalSize = _vector.size();
            _vector.resize(originalSize + bytes);
            return _vector.data() + originalSize;
        }
    };

    template <class Target>
    class PacketWriter
    {
        Target _target;
    public:
        template <class ...TArgs>
        inline PacketWriter(TArgs&& ...args):
                _target(std::forward<TArgs>(args)...)
        { }
        inline Target& target() { return _target; }

        inline void writeBytes(void *data, std::size_t bytes)
        {
            void *ptr = target().write(bytes);
            std::memcpy(ptr, data, bytes);
        }

        template <class T> inline
        typename std::enable_if<std::is_pod<T>::value>::type
        write(const T& e)
        {
            *reinterpret_cast<T*>(target().write(sizeof(T))) = e;
        }

        template <class T> inline
        typename std::enable_if<std::is_pod<T>::value>::type
        writeElements(const T *e, std::size_t count)
        {
            auto *dest = reinterpret_cast<T*>(target().write(sizeof(T) * count));
            std::memcpy(dest, e, sizeof(T) * count);
        }

        inline void writeString(const char *str, std::size_t bytes)
        {
            write(static_cast<std::uint32_t>(bytes));
            writeElements(str, bytes);
        }
        inline void writeString(const std::string& str)
        {
            writeString(str.data(), str.size());
        }
    };

    class PacketGenerator: public PacketWriter<PacketWriterVectorTarget>
    {
    public:
        std::vector<char>& vector() { return target().vector(); }
        void *data() { return reinterpret_cast<void *>(vector().data()); }
        std::size_t size() { return vector().size(); }
        void clear() { vector().clear(); }
    };

}
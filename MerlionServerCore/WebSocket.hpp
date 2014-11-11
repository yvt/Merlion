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

#include <boost/exception/exception.hpp>
#include <boost/algorithm/string.hpp>
#include <list>
#include <unordered_map>
#include <regex>
#include <boost/uuid/sha1.hpp>
#include <boost/detail/endian.hpp>
#include <boost/predef/other/endian.h>

namespace asiows
{
	namespace _asio = boost::asio;
	
	namespace detail
	{
		
		// Endian conversion (we want Boost Endian library...)
		static_assert(sizeof(std::int8_t) == 1, "Platform not supported.");
		static_assert(sizeof(std::uint8_t) == 1, "Platform not supported.");
		static_assert(static_cast<unsigned int>(-1) == ~0U, "Platform not supported.");
		
		namespace
		{
			template <class T> inline T swap_endian(const T&)
			{ static_assert(sizeof(T) != sizeof(T), "Unsupported type."); }
			template<> inline std::int8_t swap_endian<std::int8_t>(const std::int8_t &c) { return c; }
			template<> inline std::uint8_t swap_endian<std::uint8_t>(const std::uint8_t &c) { return c; }
			template<> inline std::uint16_t swap_endian<std::uint16_t>(const std::uint16_t &v)
			{
				return (v << 8) | (v >> 8);
			}
			template<> inline std::int16_t swap_endian<std::int16_t>(const std::int16_t &v)
			{ return static_cast<std::int16_t>(swap_endian(static_cast<std::uint16_t>(v))); }
			template<> inline std::uint32_t swap_endian<std::uint32_t>(const std::uint32_t &v)
			{
				return (v << 24) | (v >> 24) | ((v & 0xff0000) >> 8) | ((v & 0xff00) << 8);
			}
			template<> inline std::int32_t swap_endian<std::int32_t>(const std::int32_t &v)
			{ return static_cast<std::uint32_t>(swap_endian(static_cast<std::uint32_t>(v))); }
			template<> inline std::uint64_t swap_endian<std::uint64_t>(const std::uint64_t &v)
			{
				if (sizeof(void *) >= 8) {
					// Make use of fast 64bit arithmetic
					return (v << 56) | (v >> 56) |
					((v & 0xff000000000000) >> 40) | ((v & 0xff00) << 40) |
					((v & 0xff0000000000) >> 24) | ((v & 0xff0000) << 24) |
					((v & 0xff00000000) >> 8) | ((v & 0xff000000) << 8);
				} else {
					union {
						struct {
							std::uint32_t low, high;
						} u32;
						std::uint64_t u64;
					} u;
					u.u64 = v;
					std::swap(u.u32.low, u.u32.high);
					u.u32.low = swap_endian(u.u32.low);
					u.u32.high = swap_endian(u.u32.high);
					return u.u64;
				}
			}
			template<> inline std::int64_t swap_endian<std::int64_t>(const std::int64_t &v)
			{ return static_cast<std::uint64_t>(swap_endian(static_cast<std::uint64_t>(v))); }
		
#if BOOST_BYTE_ORDER == 4321
			template <class T>
			inline T host_to_network(const T &v) { return v; }
#elif BOOST_BYTE_ORDER == 1234
			template <class T>
			inline T host_to_network(const T &v) { return swap_endian(v); }
#else
			static_assert(false, "Cannot determine the endianness.");
#endif
			template <class T>
			inline T network_to_host(const T &v) { return host_to_network(v); }
		}
		
		template <class Callback>
		struct intermediate_op
		{
			using intermediate_op_type = intermediate_op;
			Callback callback;
			
			intermediate_op(const Callback& cb): callback(cb) { }
			intermediate_op(Callback&& cb): callback(cb) { }
		};
		
		template <class Callback>
		struct ignoring_intermediate_op :
		public intermediate_op<Callback>
		{
			ignoring_intermediate_op(const Callback& cb) :
			intermediate_op<Callback>(cb) { }
			ignoring_intermediate_op(Callback&& cb) :
			intermediate_op<Callback>(cb) { }
			
			template <class ...Args>
			void operator () (Args&&...) { }
		};
		
		template <class Func, class Callback>
		inline void asio_handler_invoke(Func &func, intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		template <class Func, class Callback>
		inline void asio_handler_invoke(const Func &func, intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		template <class Func, class Callback>
		inline void asio_handler_invoke(Func &&func, intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		template <class Func, class Callback>
		inline void asio_handler_invoke(Func &func, const intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		template <class Func, class Callback>
		inline void asio_handler_invoke(const Func &func, const intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		template <class Func, class Callback>
		inline void asio_handler_invoke(Func &&func, const intermediate_op<Callback> *callback)
		{
			boost_asio_handler_invoke_helpers::invoke(func, callback->callback);
		}
		
		template <class Buffer>
		class infinitely_repeated_buffer_sequence
		{
			Buffer buffer;
		public:
			infinitely_repeated_buffer_sequence
			(const infinitely_repeated_buffer_sequence&) = default;
			infinitely_repeated_buffer_sequence
			(infinitely_repeated_buffer_sequence&&) = default;
			infinitely_repeated_buffer_sequence(const Buffer& b): buffer(b) { }
			
			// note: this iterator might not satisfy some requirements...
			struct const_iterator :
			public std::iterator<std::bidirectional_iterator_tag, Buffer>
			{
				using value_type = Buffer;
				const infinitely_repeated_buffer_sequence *seq;
				std::uintmax_t index;
				const_iterator() = default;
				const_iterator(const const_iterator& o) = default;
				const_iterator(const infinitely_repeated_buffer_sequence& seq,
							   std::uintmax_t index): seq(&seq), index(index) {}
				const_iterator & operator -- () { --index; return *this; }
				const_iterator & operator ++ () { ++index; return *this; }
				const_iterator operator -- (int) { auto old = index--; return const_iterator(*seq, old); }
				const_iterator operator ++ (int) { auto old = index++; return const_iterator(*seq, old); }
				bool operator == (const const_iterator& other) const { return index == other.index; }
				bool operator != (const const_iterator& other) const { return index != other.index; }
				value_type operator * () const { return seq->buffer; }
				std::ptrdiff_t operator - (const const_iterator& other) const
				{ return static_cast<std::ptrdiff_t>(index - other.index); }
			};
			
			const_iterator begin() const { return const_iterator(*this, 0); }
			const_iterator end() const { return const_iterator(*this, std::numeric_limits<std::uintmax_t>::max()); }
		};
		
		template <class Buffer, class BaseBufferSequence>
		class buffer_clipper
		// satisfies MutableBufferSequence or ConstBufferSequence
		{
			BaseBufferSequence baseSequence;
			std::uintmax_t const length;
			std::size_t endIndex;
			std::size_t finalLength = 0;
			typename BaseBufferSequence::const_iterator baseEnd;
			void init()
			{
				std::uintmax_t remaining = length;
				endIndex = 0;
				baseEnd = baseSequence.begin();
				while (baseEnd != baseSequence.end()) {
					auto sz = boost::asio::buffer_size(*baseEnd);
					finalLength = static_cast<std::size_t>(sz);
					++baseEnd; ++endIndex;
					if (sz >= remaining) {
						finalLength = static_cast<std::size_t>(remaining);
						break;
					} else {
						remaining -= sz;
					}
				}
			}
		public:
			struct const_iterator :
			public std::iterator<std::bidirectional_iterator_tag, typename BaseBufferSequence::value_type>
			{
				const buffer_clipper *clipper;
				std::size_t index = 0;
				typename BaseBufferSequence::const_iterator baseIt;
				using difference_type = std::ptrdiff_t;
				using value_type = typename BaseBufferSequence::value_type;
				const_iterator() = default;
				const_iterator(const const_iterator&) = default;
				const_iterator(const_iterator&&) = default;
				explicit const_iterator(const buffer_clipper& clipper,
										const typename BaseBufferSequence::const_iterator& baseIt,
										std::size_t index):
				baseIt(baseIt), clipper(&clipper), index(index) { }
				const_iterator & operator -- () { --baseIt; --index; return *this; }
				const_iterator & operator ++ () { ++baseIt; ++index; return *this; }
				value_type operator * () const {
					if (index + 1 == clipper->endIndex) {
						// FIXME: support const buffer
						void *ptr = boost::asio::buffer_cast<void *>(*baseIt);
						return _asio::mutable_buffer(ptr, clipper->finalLength);
					} else {
						return *baseIt;
					}
				}
				bool operator == (const const_iterator& other) const { return index == other.index; }
				bool operator != (const const_iterator& other) const { return index != other.index; }
				const_iterator operator -- (int) { auto old = baseIt--; auto oldI = index--; return const_iterator(clipper, old, oldI); }
				const_iterator operator ++ (int) { auto old = baseIt++; auto oldI = index++; return const_iterator(clipper, old, oldI); }
				std::ptrdiff_t operator - (const const_iterator& other) const
				{ return index - other.index; }
			};
			
			buffer_clipper(buffer_clipper&&) = default;
			buffer_clipper(const buffer_clipper&) = default;
			buffer_clipper(BaseBufferSequence&& seq, std::uintmax_t length):
			baseSequence(seq),
			length(length)
			{ init(); }
			buffer_clipper(const BaseBufferSequence& seq, std::uintmax_t length):
			baseSequence(seq),
			length(length)
			{ init(); }
			
			const_iterator begin() const { return const_iterator(*this, baseSequence.begin(), 0); }
			const_iterator end() const { return const_iterator(*this, baseEnd, endIndex); }
		};
		
		template <class Function>
		class wrapped_function
		{
			Function f;
		public:
			template <class ...Args>
			wrapped_function(Args&& ...args): f(std::forward<Args>(args)...)
			{ }
			
			template <class ...Args>
			void operator () () {
				
			}
		};
		
		class web_socket_masker
		{
			union {
				std::uint32_t u32;
				char u8[4];
			} mask_;
			boost::uint_t<2>::fast pos = 0;
		public:
			web_socket_masker(std::uint32_t mask)
			{
				this->mask_.u32 = mask;
			}
			
			std::uint32_t mask() const { return mask_.u32; }
			
			void apply(char *buffer, std::size_t len) BOOST_NOEXCEPT
			{
				while (len > 0 && pos != 0) {
					*buffer = *buffer ^ mask_.u8[pos];
					pos = (pos + 1) & 3;
					--len; ++buffer;
				}
				while (len >= 4) {
					*reinterpret_cast<std::uint32_t*>(buffer) =
					*reinterpret_cast<std::uint32_t*>(buffer) ^ mask_.u32;
					len -= 4; buffer += 4;
				}
				while (len > 0) {
					*buffer = *buffer ^ mask_.u8[pos];
					pos = (pos + 1) & 3;
					--len; ++buffer;
				}
			}
			
			void apply(const boost::asio::mutable_buffer& buffer)
			{
				apply(boost::asio::buffer_cast<char *>(buffer),
					  boost::asio::buffer_size(buffer));
			}
			
			template <class MutableBufferSequence>
			void apply(const MutableBufferSequence& sequence)
			{
				for (auto it = sequence.begin(); it != sequence.end(); ++it)
					apply(*it);
			}
		};
	}
	
	enum class web_socket_opcode : unsigned int
	{
		continuation = 0x0,
		text_frame = 0x1,
		binary_frame = 0x2,
		connection_close = 0x8,
		ping = 0x9,
		pong = 0xa
	};
	
	static inline bool is_control_frame(web_socket_opcode opcode)
	{
		return (static_cast<unsigned int>(opcode) & 0x8) != 0;
	}
	
	struct web_socket_frame_header
	{
		bool fin: 1;
		bool reserved1: 1;
		bool reserved2: 1;
		bool reserved3: 1;
		web_socket_opcode opcode: 4;
		boost::optional<std::uint32_t> masking_key;
		std::uint64_t payload_length;
	};
	
	struct web_socket_message_header
	{
		bool reserved1: 1;
		bool reserved2: 1;
		bool reserved3: 1;
		web_socket_opcode opcode: 4;
		
		web_socket_message_header():
		reserved1(false),
		reserved2(false),
		reserved3(false),
		opcode(web_socket_opcode::binary_frame) {}
		
		web_socket_message_header(const web_socket_frame_header& h):
		reserved1(h.reserved1),
		reserved2(h.reserved2),
		reserved3(h.reserved3),
		opcode(h.opcode) { }
		
		web_socket_frame_header frame_header()
		{
			web_socket_frame_header h;
			h.reserved1 = reserved1;
			h.reserved2 = reserved2;
			h.reserved3 = reserved3;
			h.opcode = opcode;
			return h;
		}
	};
	
	template <class NextLayer>
	class web_socket_frame_reader
	{
		enum class state_t {
			initial,
			reading_header,
			header_read,
			fail
		};
		NextLayer next_;
		state_t state_ = state_t::initial;
		std::vector<char> buffer_;
		
		bool header_read_ = false;
		web_socket_frame_header header_;
		std::size_t remaining_header_bytes()
		{
			std::size_t lenSize = 0;
			if (header_.payload_length == 126) {
				lenSize = 2;
			} else if (header_.payload_length == 127) {
				lenSize = 8;
			}
			if (header_.masking_key)
				lenSize += 4;
			return lenSize;
		}
		
		boost::optional<detail::web_socket_masker> masker;
		
		std::uint64_t remainingBytes;
	public:
		using next_layer_type = typename boost::remove_reference<NextLayer>::type;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;
		
		template <class ...Arg>
		web_socket_frame_reader(Arg&& ...t): next_(std::forward<Arg>(t)...) {}
		
		next_layer_type& next_layer() { return next_; }
		lowest_layer_type& lowest_layer() { return next_layer().lowest_layer(); }
		
		bool header_read() const { return header_read_; }
		
		std::uint64_t remaining_bytes() const { assert(header_read()); return remainingBytes; }
		
		const web_socket_frame_header& header() const
		{
			assert(header_read_);
			return header_;
		}
		
		void reset()
		{
			if (state_ == state_t::fail) {
				// Cannot recover.
				return;
			}
			state_ = state_t::initial;
			header_read_ = false;
		}
		
		void fail()
		{
			state_ = state_t::fail;
		}
		
		template <class Callback>
		struct read_header_op: public detail::intermediate_op<Callback>
		{
			enum class phase_t
			{
				read_flags_payload_len,
				read_remaining,
				done
			};
			phase_t phase_ = phase_t::read_flags_payload_len;
			web_socket_frame_reader& parent;
			
			read_header_op(web_socket_frame_reader &parent, const Callback& cb):
			detail::intermediate_op<Callback>(cb),
			parent(parent) { }
			read_header_op(web_socket_frame_reader &parent, Callback&& cb):
			detail::intermediate_op<Callback>(cb),
			parent(parent)  { }
			
			void operator () (const boost::system::error_code& error, std::size_t count)
			{
				auto& bufferVec = parent.buffer_;
				switch (phase_) {
					case phase_t::read_flags_payload_len:
						if (error) {
							parent.state_ = state_t::fail;
							this->callback(error);
							break;
						}
						if (count == 2) {
							std::uint16_t headerVal = *reinterpret_cast<std::uint16_t *>(bufferVec.data());
							headerVal = detail::network_to_host(headerVal);
							auto& hdr = parent.header_;
							hdr.fin = (headerVal & 0x8000) != 0;
							hdr.reserved1 = (headerVal & 0x4000) != 0;
							hdr.reserved2 = (headerVal & 0x2000) != 0;
							hdr.reserved3 = (headerVal & 0x1000) != 0;
							hdr.opcode = static_cast<web_socket_opcode>((headerVal >> 8) & 15);
							if ((headerVal & 0x80) != 0)
								hdr.masking_key = 0;
							else
								hdr.masking_key = boost::none;
							hdr.payload_length = headerVal & 0x7f;
							
							if (is_control_frame(hdr.opcode)) {
								if (hdr.payload_length > 125 ||
									!hdr.fin) {
									// Control frame cannot be fragmented nor
									// be more than 125 bytes. [RFC6455 5.5]
									parent.state_ = state_t::fail;
									this->callback(make_error_code(boost::system::errc::protocol_error));
									return;
								}
							}
							
							phase_ = phase_t::read_remaining;
							perform();
						} else {
							parent.state_ = state_t::fail;
							this->callback(make_error_code(boost::system::errc::protocol_error));
						}
						break;
					case phase_t::read_remaining:
						if (error) {
							parent.state_ = state_t::fail;
							this->callback(error);
							break;
						}
						if (count == parent.remaining_header_bytes()) {
							char const *buffer = bufferVec.data();
							auto& hdr = parent.header_;
							
							// FIXME: unaligned memory access possible
							if (hdr.payload_length == 126) {
								hdr.payload_length = detail::network_to_host(*reinterpret_cast<std::uint16_t const *>(buffer));
								buffer += 2;
								
								if (hdr.payload_length < 126) {
									// Not compact encoding
									parent.state_ = state_t::fail;
									this->callback(make_error_code(boost::system::errc::protocol_error));
									break;
								}
							} else if (hdr.payload_length == 127) {
								hdr.payload_length = detail::network_to_host(*reinterpret_cast<std::uint64_t const *>(buffer));
								buffer += 8;
								
								if (hdr.payload_length < 65536 ||
									hdr.payload_length >= 0x8000000000000000ULL) {
									// Not compact encoding / Too big [RFC 6455 5.2]
									parent.state_ = state_t::fail;
									this->callback(make_error_code(boost::system::errc::protocol_error));
									break;
								}
							}
							
							if (hdr.masking_key) {
								hdr.masking_key = *reinterpret_cast<std::uint32_t const *>(buffer);
								buffer += 4;
							}
							
							phase_ = phase_t::done;
							perform();
						} else {
							parent.state_ = state_t::fail;
							this->callback(make_error_code(boost::system::errc::protocol_error));
						}
						break;
					case phase_t::done:
						assert(false);
						break;
				}
			}
			
			void perform()
			{
				auto& bufferVec = parent.buffer_;
				switch (phase_) {
					case phase_t::read_flags_payload_len:
						bufferVec.resize(2);
						_asio::async_read(parent.next_layer(),
										  _asio::buffer(bufferVec), std::move(*this));
						break;
					case phase_t::read_remaining:
						bufferVec.resize(parent.remaining_header_bytes());
						_asio::async_read(parent.next_layer(),
										  _asio::buffer(bufferVec), std::move(*this));
						break;
					case phase_t::done:
						parent.state_ = state_t::header_read;
						parent.remainingBytes = parent.header_.payload_length;
						parent.header_read_ = true;
						if(parent.header_.masking_key)
							parent.masker.emplace(*parent.header_.masking_key);
						this->callback(boost::system::error_code());
						break;
				}
			}
		};
		
		template <class Callback>
		void async_read_header(Callback&& callback)
		{
			if (state_ != state_t::initial) {
				callback(make_error_code(boost::system::errc::operation_not_permitted));
				return;
			}
			
			state_ = state_t::reading_header;
			read_header_op<typename std::remove_reference<Callback>::type>
			op(*this, std::forward<Callback>(callback));
			op.perform();
		}
		
		
		template <class MutableBufferSequence, class Callback>
		struct read_some_op: public detail::intermediate_op<Callback>
		{
			web_socket_frame_reader& parent;
			MutableBufferSequence bufferSequence;
			
			read_some_op(web_socket_frame_reader &parent, const Callback& cb, const MutableBufferSequence &buffer):
			detail::intermediate_op<Callback>(cb),
			parent(parent),
			bufferSequence(buffer) { }
			read_some_op(web_socket_frame_reader &parent, Callback&& cb, const MutableBufferSequence &buffer):
			detail::intermediate_op<Callback>(cb),
			parent(parent),
			bufferSequence(buffer)  { }
			
			void operator () (const boost::system::error_code& error, std::size_t count)
			{
				if (error) {
					parent.state_ = state_t::fail;
					this->callback(make_error_code(boost::system::errc::protocol_error), 0);
				} else {
					assert(count <= parent.remainingBytes);
					parent.remainingBytes -= count;
					
					if (parent.masker)
						parent.masker->apply(detail::buffer_clipper<_asio::mutable_buffer, MutableBufferSequence>( bufferSequence, count));
					
					this->callback(boost::system::error_code(), count);
				}
			}
			
			void perform()
			{
				detail::buffer_clipper<boost::asio::mutable_buffer, MutableBufferSequence>
				clipper(bufferSequence, parent.remainingBytes);
				parent.next_layer().async_read_some(std::move(clipper), std::move(*this));
			}
		};
		
		template <class MutableBufferSequence, class Callback>
		void async_read_some(MutableBufferSequence&& buffer, Callback&& callback)
		{
			
			if (state_ != state_t::header_read) {
				callback(make_error_code(boost::system::errc::operation_not_permitted), 0);
				return;
			}
			
			read_some_op<typename std::remove_reference<MutableBufferSequence>::type,
			typename std::remove_reference<Callback>::type> op
			(*this, std::forward<Callback>(callback),
			 std::forward<MutableBufferSequence>(buffer));
			op.perform();
		}
	};
	
	enum class web_socket_close_mode
	{
		not_closed,
		normal_closure,
		fail
	};
	
	namespace web_socket_close_status_codes
	{
		enum web_socket_close_status_code {
			normal_closure = 1000,
			going_away = 1001,
			protocol_error = 1002,
			cannot_accept = 1003,
			no_status_code = 1005, // must not be sent as actual error code
			closed_abnormally = 1006, // must not be sent as actual error code
			not_consistent = 1007,
			violated_policy = 1008,
			too_big = 1009,
			no_extension = 1010,
			unexpected_condition = 1011,
			service_restart = 1012,
			try_again_later = 1013,
			tls_handshake_failure = 1015 // must not be sent as actual error code
		};
	}
	
	// An asynchronous WebSocket implementation.
	template <class NextLayer>
	class web_socket
	{
		enum class state_t {
			// CONNECTED state.
			connected,
			
			// Sent the Close frame to the peer. Waiting for the peer's response.
			closing_active,
			
			// Sent the Close frame to the peer. <boost::asio::async_write> has completed
			// and waiting for the peer's response.
			closing_active_sent_close,
			
			// Received the Close frame. Waiting for <async_shutdown> to be called.
			closing_passive,
			
			// Sent the Close frame to the peer. Waiting for the completion of <boost::asio::async_write>.
			closing_sending_close,
			
			// Socket is closed. When this becomes the active state, <close_done> must be
			// called.
			closed
		};
		NextLayer next_;
		state_t state_ = state_t::connected;
		
		/* --- Receiving Messages --- */
		
		std::array<char, 4096> read_buffer_;
		
		enum class read_state_t {
			// Not reading nor waiting for a message.
			not_reading,
			
			// Finding a message frame. Might be processing a control frame.
			finding_message,
			
			// Reading a message frame. Might be processing a control frame.
			reading_message
		};
		read_state_t read_state_ = read_state_t::not_reading;
		
		web_socket_message_header last_message_header_;
		
		template <class Callback>
		class receive_message_op;
		
		template <class Callback>
		class read_to_end_before_receiving_message_op;
		
		template <class MutableBufferSequence, class Callback>
		class receive_message_continuation_op;
		
		template <class Callback>
		class handle_control_frame_op;
		
		template <class Callback>
		void handle_control_frame(Callback&&);
		
		/* --- Sending Messages --- */
		
		std::array<char, 4096> write_buffer_;
		std::size_t write_buffer_pos_ = 0;
		std::size_t write_buffer_real_start_pos; // computed by async_send_frame
		
		web_socket_message_header sent_message_header_;
		bool sending_final_frame_ = false;
		bool sending_first_frame_ = false;
		std::function<boost::optional<std::uint32_t>()> masking_key_provider;
		
		// Largest possible frame header size.
		const std::size_t write_buffer_start_pos = 14;
		
		enum class write_state_t {
			// Not writing.
			not_writing,
			
			writing_message,
			
			waiting_to_write
		};
		write_state_t write_state_ = write_state_t::not_writing;
		
		// List of queued writers.
		std::list<std::function<void()>> write_queue;
		
		template <class Callback>
		void add_write_queue(const web_socket_message_header &header, Callback&& cb)
		{
			auto cbbox = std::make_shared<typename std::remove_reference<Callback>::type>(std::forward<Callback>(cb));
			auto hdr = header;
			write_queue.push_back([this, cbbox, hdr] () {
				write_state_ = write_state_t::waiting_to_write;
				boost_asio_handler_invoke_helpers::invoke([this, cbbox, hdr] {
					assert(write_state_ == write_state_t::waiting_to_write);
					write_state_ = write_state_t::not_writing;
					async_begin_write(hdr, *cbbox);
				}, *cbbox);
			});
		}
		
		template <class Callback>
		class end_write_op;
		
		template <class Callback>
		class send_frame_op;
		
		template <class Callback>
		void async_send_frame(Callback&&);
		
		template <class MutableBufferSequence, class Callback>
		class send_buffer_full_op;
		
		template <class ConstBufferSequence, class Callback>
		class send_message_op;
		
		/* --- Handling Ping --- */
		boost::optional<std::string> pending_pong;
		
		template <class Callback>
		class pong_op;
		
		/* --- Sending Ping --- */
		struct awaited_pong :
		public std::enable_shared_from_this<awaited_pong> {
			using ptr = std::shared_ptr<awaited_pong>;
			using weak_ptr = std::weak_ptr<awaited_pong>;
			std::function<void(const boost::system::error_code&)> handler;
			std::string data;
			bool linked;
			ptr previous;
			weak_ptr next;
		};
		typename awaited_pong::ptr last_awaited_pong;
		std::unordered_map<std::string,
		typename awaited_pong::ptr> awaited_pong_map;
		std::vector<typename awaited_pong::ptr> awaited_pong_pool;
		typename awaited_pong::ptr alloc_awaited_pong();
		void return_awaited_pong(const typename awaited_pong::ptr &);
		void enqueue_awaited_pong(const typename awaited_pong::ptr &);
		void received_pong(const std::string&);
		void fail_all_awaited_pong(const boost::system::error_code&);
		void fail_awaited_pong(const typename awaited_pong::ptr &,
							   const boost::system::error_code &);
		
		template <class Callback>
		class ping_op;
		
		/* --- Connection Closure --- */
		web_socket_close_mode close_mode_ = web_socket_close_mode::not_closed;
		std::uint16_t close_status_code_ = web_socket_close_status_codes::no_status_code;
		std::string close_reason_;
		
		std::list<std::function<void(const boost::system::error_code&)>> onshutdown;
		
		template <class Callback>
		void add_shutdown_handler(Callback&& cb)
		{
			auto cbbox = std::make_shared<typename std::remove_reference<Callback>::type>(std::forward<Callback>(cb));
			onshutdown.push_back([this, cbbox] (const boost::system::error_code& ec) {
				boost_asio_handler_invoke_helpers::invoke([this, ec, cbbox] {
					(*cbbox)(ec);
				}, *cbbox);
			});
		}
		
		void close_done(const boost::system::error_code& ec)
		{
			decltype(onshutdown) handlers;
			handlers.swap(onshutdown);
			for (const auto& f: handlers)
				f(ec);
			
			fail_all_awaited_pong(make_error_code(boost::system::errc::not_connected));
		}
		
		
		// note: this op actually doesn't call the callback.
		template <class Callback>
		class discard_receive_message_op;
		
		template <class Callback>
		class close_op;
		
	public:
		using next_layer_type = typename boost::remove_reference<NextLayer>::type;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;
		
		// Creates <web_socket> with arguments to the next layer supplied.
		template <class ...Arg>
		web_socket(Arg&& ...t):
		next_(std::forward<Arg>(t)...),
		reader(next_) {}
		
		// Registers a function that generates the masking key. [RFCblah 5.3]
		// When the function returns <boost::none>, masking is not done.
		void set_masking_key_provider(std::function<boost::optional<std::uint32_t>()> fn)
		{ masking_key_provider = fn; }
		
		next_layer_type& next_layer() { return next_; }
		lowest_layer_type& lowest_layer() { return next_layer().lowest_layer(); }
		
		// Returns header of the most recently received message.
		const web_socket_message_header& last_message_header() const
		{ return last_message_header_; }
		
		web_socket_close_mode close_mode() const { return close_mode_; }
		std::uint32_t close_status_code() const { return close_status_code_; }
		const std::string& close_reason() const { return close_reason_; }
		
		bool shutdown_requested() const { return state_ == state_t::closing_passive; }
		
		// Listens for an incoming message.
		// Callback function will be called when a message is
		// available, or there was an error.
		// After receiving a message, its header can be retrived by
		// calling <last_message_header>, and its payload can read
		// read by <async_read_some>.
		template <class Callback>
		void async_receive_message(Callback&& cb);
		
		// Initiates WebSocket Close Handshake, which usually
		// causes the connection to be closed gracefully.
		// When <fail> is true, <web_socket> will
		// _Fail the WebSocket Connection_.
		template <class Callback>
		void async_shutdown(std::uint16_t status_code, const std::string& reason, bool fail, Callback&& cb);
		
		// Reads the payload of the most recently received message.
		template <class MutableBufferSequence, class Callback>
		void async_read_some(MutableBufferSequence&&, Callback&&);
		
		// Starts sending a message to the peer.
		template <class Callback>
		void async_begin_write(const web_socket_message_header &, Callback&&);
		
		// Writes data to the message being sent.
		template <class ConstBufferSequence, class Callback>
		void async_write_some(ConstBufferSequence&&, Callback&&);
		
		// Declares that no more data will be added to the
		// message being sent.
		template <class Callback>
		void async_end_write(Callback&&);
		
		// Sends a message with its application data provided
		// as ConstBufferSequence.
		// The lifetime of the <web_socket_message_header> and
		// <ConstBufferSequence> don't have to long
		// until the callback is called, but the actual buffer
		// pointed by <ConstBufferSequence> must.
		template <class ConstBufferSequence, class Callback>
		void async_send_message(const web_socket_message_header &, ConstBufferSequence&&, Callback&&);
		
		// Sends a Ping.
		// Callback will be called when <web_socket> receives
		// Pong, or an error occured.
		template <class Callback>
		void async_send_ping(const std::string& data,
							 Callback&&);
		
	private:
		web_socket_frame_reader<next_layer_type&> reader;
	};
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::receive_message_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		bool handling_control_frame_ = false;
	public:
		receive_message_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		receive_message_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec)
		{
			if (ec) {
				// Fail the connection.
				parent.read_state_ = read_state_t::not_reading;
				switch (parent.state_) {
					case state_t::closing_active:
						parent.state_ = state_t::closing_sending_close;
						break;
					case state_t::closing_active_sent_close:
						parent.state_ = state_t::closed;
						parent.close_done(boost::system::error_code());
						break;
					case state_t::closing_passive:
					case state_t::closing_sending_close:
					case state_t::connected:
					case state_t::closed:
						break;
				}
				this->callback(ec);
				return;
			}
			
			if (handling_control_frame_) {
				handling_control_frame_ = false;
				perform();
				return;
			}
			
			// Check state
			switch (parent.state_) {
				case state_t::closed:
				case state_t::closing_sending_close:
				case state_t::closing_passive:
					// We no longer accept any received frames.
					parent.read_state_ = read_state_t::not_reading;
					this->callback(make_error_code(boost::system::errc::not_connected));
					return;
				case state_t::closing_active_sent_close:
				case state_t::connected:
				case state_t::closing_active:
					break;
			}
			
			
			const web_socket_frame_header& hdr = parent.reader.header();
			if (!is_control_frame(hdr.opcode)) {
				// Message found.
				parent.last_message_header_ = hdr;
				parent.read_state_ = read_state_t::reading_message;
				this->callback(ec);
				return;
			}
			
			// Control frame.
			handling_control_frame_ = true;
			parent.handle_control_frame(std::move(*this));
		}
		
		void perform()
		{
			parent.reader.reset();
			parent.reader.async_read_header(std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::read_to_end_before_receiving_message_op :
	public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		
	public:
		read_to_end_before_receiving_message_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		read_to_end_before_receiving_message_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec, std::size_t)
		{
			if (ec) {
				this->callback(ec);
				return;
			}
			
			// Well done. Back to the receive-the-message work.
			parent.async_receive_message(this->callback);
		}
		
		void perform()
		{
			_asio::async_read(parent,
							  detail::infinitely_repeated_buffer_sequence<_asio::mutable_buffer>
							  (_asio::buffer(parent.read_buffer_.data(), parent.read_buffer_.size())),
							  std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_receive_message(Callback &&cb)
	{
		switch (state_) {
			case state_t::closed:
			case state_t::closing_sending_close:
			case state_t::closing_passive:
				// We no longer accept any received frames.
				cb(make_error_code(boost::system::errc::not_connected));
				return;
			case state_t::closing_active_sent_close:
			case state_t::connected:
			case state_t::closing_active:
				break;
		}
		
		// if we are in middle of reading message,
		// read to the end first
		if (read_state_ == read_state_t::reading_message) {
			read_to_end_before_receiving_message_op<typename std::remove_reference<Callback>::type>
			op(*this, std::forward<Callback>(cb));
			op.perform();
			return;
		}
		
		// Multiple read is prohibited
		assert(read_state_ == read_state_t::not_reading);
		
		read_state_ = read_state_t::finding_message;
		
		receive_message_op<typename std::remove_reference<Callback>::type>
		op(*this, std::forward<Callback>(cb));
		op.perform();
		
	}
	
	template <class NextLayer>
	template <class MutableBufferSequence, class Callback>
	class web_socket<NextLayer>::receive_message_continuation_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		MutableBufferSequence seq;
		bool handling_control_frame_ = false;
	public:
		template <class MutableBufferSequenceArg>
		receive_message_continuation_op(web_socket &parent, MutableBufferSequenceArg&& seq, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<MutableBufferSequenceArg>(seq)) { }
		template <class MutableBufferSequenceArg>
		receive_message_continuation_op(web_socket &parent, MutableBufferSequenceArg&& seq, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<MutableBufferSequenceArg>(seq)) { }
		
		void operator () (const boost::system::error_code& ec)
		{
			if (ec) {
				// Fail the connection.
				parent.read_state_ = read_state_t::not_reading;
				switch (parent.state_) {
					case state_t::closing_active:
						parent.state_ = state_t::closing_sending_close;
						break;
					case state_t::closing_active_sent_close:
						parent.state_ = state_t::closed;
						parent.close_done(boost::system::error_code());
						break;
					case state_t::closing_passive:
					case state_t::closing_sending_close:
					case state_t::connected:
					case state_t::closed:
						break;
				}
				this->callback(ec, 0);
				return;
			}
			
			if (handling_control_frame_) {
				handling_control_frame_ = false;
				perform();
				return;
			}
			
			// Check state
			switch (parent.state_) {
				case state_t::closed:
				case state_t::closing_sending_close:
				case state_t::closing_passive:
					// We no longer accept any received frames.
					parent.read_state_ = read_state_t::not_reading;
					this->callback(make_error_code(boost::system::errc::not_connected), 0);
					return;
				case state_t::closing_active_sent_close:
				case state_t::connected:
				case state_t::closing_active:
					break;
			}
			
			
			const web_socket_frame_header& hdr = parent.reader.header();
			if (!is_control_frame(hdr.opcode)) {
				// Message found.
				// Continue reading.
				parent.async_read_some(seq, this->callback);
				return;
			}
			
			// Control frame.
			handling_control_frame_ = true;
			parent.handle_control_frame(std::move(*this));
		}
		
		void perform()
		{
			parent.reader.reset();
			parent.reader.async_read_header(std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class MutableBufferSequence, class Callback>
	void web_socket<NextLayer>::async_read_some(MutableBufferSequence &&buffers, Callback &&callback)
	{
		
		if (read_state_ != read_state_t::reading_message) {
			// No message, no data.
			callback(boost::system::error_code(), 0);
			return;
		}
		
		if (reader.remaining_bytes() == 0) {
			if (reader.header().fin) {
				// This is the final part of the message.
				read_state_ = read_state_t::not_reading;
				callback(boost::system::error_code(), 0);
			} else {
				// Find the next message.
				receive_message_continuation_op<
				typename std::remove_reference<MutableBufferSequence>::type,
				typename std::remove_reference<Callback>::type>
				op(*this, std::forward<MutableBufferSequence>(buffers), std::forward<Callback>(callback));
				op.perform();
			}
		} else {
			reader.async_read_some(std::forward<MutableBufferSequence>(buffers),
								   std::forward<Callback>(callback));
		}
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::handle_control_frame_op :
	public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		
	public:
		handle_control_frame_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		handle_control_frame_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec, std::size_t count)
		{
			if (ec) {
				this->callback(ec);
				return;
			}
			if (count != parent.reader.header().payload_length) {
				this->callback(make_error_code(boost::system::errc::protocol_error));
				return;
			}
			
			// Check state
			switch (parent.state_) {
				case state_t::closed:
				case state_t::closing_sending_close:
				case state_t::closing_passive:
					// We no longer accept any received frames.
					this->callback(make_error_code(boost::system::errc::not_connected));
					return;
				case state_t::closing_active_sent_close:
				case state_t::connected:
				case state_t::closing_active:
					break;
			}
			
			const char *data = parent.read_buffer_.data();
			web_socket_opcode opcode = parent.reader.header().opcode;
			
			switch (opcode) {
				case web_socket_opcode::connection_close:
					// Passive close.
					if (count >= 2) {
						// FIXME: unaligned memory access possible
						parent.close_status_code_ = detail::network_to_host(*reinterpret_cast<const std::uint16_t *>(data));
						parent.close_reason_ = std::string(data + 2, count - 2);
					}
					switch (parent.state_) {
						case state_t::closing_active_sent_close:
						case state_t::closing_active:
							parent.state_ = state_t::closed;
							parent.close_done(boost::system::error_code());
							break;
						case state_t::closed:
							parent.state_ = state_t::closed;
							break;
						case state_t::closing_sending_close:
						case state_t::closing_passive:
							break;
						case state_t::connected:
							parent.state_ = state_t::closing_passive;
							break;
					}
					
					break;
				case web_socket_opcode::ping:
					if (parent.pending_pong) {
						// Pong is still queued. Replace with the newer app data.
						parent.pending_pong = std::string(data, count);
					} else {
						parent.pending_pong = std::string(data, count);
						pong_op<Callback> op(parent, this->callback);
						op.perform();
					}
					break;
				case web_socket_opcode::pong:
					parent.received_pong(std::string(data, count));
					break;
				default:
					// Unknown control opcode.
					this->callback(make_error_code(boost::system::errc::protocol_error));
					return;
			}
			
			this->callback(boost::system::error_code());
		}
		
		void perform()
		{
			_asio::async_read(parent.reader,
							  _asio::buffer(parent.read_buffer_.data(), parent.read_buffer_.size()),
							  std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::handle_control_frame(Callback &&cb)
	{
		handle_control_frame_op
		<typename std::remove_reference<Callback>::type>
		op(*this, std::forward<Callback>(cb));
		op.perform();
	}
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_begin_write
	(const web_socket_message_header &header, Callback &&cb)
		{
		if (write_state_ == write_state_t::not_writing) {
			// Queue is empty.
			
			switch (state_) {
				case state_t::closed:
				case state_t::closing_active_sent_close:
				case state_t::closing_active:
				case state_t::closing_sending_close:
					// We no longer send any frames.
					cb(make_error_code(boost::system::errc::not_connected));
					return;
				case state_t::closing_passive:
				case state_t::connected:
					break;
			}
			
			sent_message_header_ = header;
			write_buffer_pos_ = write_buffer_start_pos;
			write_state_ = write_state_t::writing_message;
			sending_final_frame_ = false;
			sending_first_frame_ = true;
			
			cb(boost::system::error_code());
		} else {
			// Writing currently. Enqueue the write queue.
			add_write_queue(header, std::forward<Callback>(cb));
		}
	}
	
	
	template <class NextLayer>
	template <class ConstBufferSequence, class Callback>
	class web_socket<NextLayer>::send_buffer_full_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		ConstBufferSequence seq;
		
	public:
		template <class ConstBufferSequenceArg>
		send_buffer_full_op(web_socket &parent, ConstBufferSequenceArg&& seq, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<ConstBufferSequenceArg>(seq)) { }
		template <class ConstBufferSequenceArg>
		send_buffer_full_op(web_socket &parent, ConstBufferSequenceArg&& seq, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<ConstBufferSequenceArg>(seq)) { }
		
		void operator () (const boost::system::error_code& ec)
		{
			if (ec) {
				// Buffer could not be flushed.
				this->callback(ec, 0);
				return;
			}
			
			// Succeeded. Try writing again.
			parent.async_write_some(seq, this->callback);
		}
		
		void perform()
		{
			parent.async_send_frame(std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class ConstBufferSequence, class Callback>
	void web_socket<NextLayer>::async_write_some(ConstBufferSequence &&buffers, Callback &&cb)
		{
		if (write_state_ != write_state_t::writing_message) {
			cb(make_error_code(boost::system::errc::invalid_argument), 0);
			return;
		}
		
		if (boost::asio::buffer_size(buffers) == 0) {
			cb(boost::system::error_code(), 0);
			return;
		}
		
		if (write_buffer_pos_ == write_buffer_.size()) {
			// Buffer full.
			send_buffer_full_op<
			typename std::remove_reference<ConstBufferSequence>::type,
			typename std::remove_reference<Callback>::type>
			op(*this, std::forward<ConstBufferSequence>(buffers), std::forward<Callback>(cb));
			op.perform();
			return;
		}
		
		// Copy to the internal buffer.
		auto it = buffers.begin();
		const std::size_t initialPos = write_buffer_pos_;
		std::size_t bufferLen = write_buffer_.size();
		
		if (is_control_frame(sent_message_header_.opcode)) {
			// All control frames must have a payload length of 125 bytes or less.
			// [RFC blah 5.5 Control Frames]
			// FIXME: extensions
			bufferLen = 125 + write_buffer_start_pos;
		}
		
		while (it != buffers.end() && write_buffer_pos_ < bufferLen) {
			const auto& buffer = *it;
			auto remaining = bufferLen - write_buffer_pos_;
			
			auto chunkSize = boost::asio::buffer_size(buffer);
			const auto *data = boost::asio::buffer_cast<const void *>(buffer);
			
			auto writtenSize = std::min<std::size_t>(chunkSize, remaining);
			
			std::memcpy(write_buffer_.data() + write_buffer_pos_, data, writtenSize);
			
			write_buffer_pos_ += writtenSize;
			++it;
		}
		
		cb(boost::system::error_code(), write_buffer_pos_ - initialPos);
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::end_write_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		
	public:
		end_write_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		end_write_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec)
		{
			
			// Check write queue
			parent.write_state_ = write_state_t::not_writing;
			if (!parent.write_queue.empty()) {
				auto fn = std::move(parent.write_queue.front());
				parent.write_queue.pop_front();
				fn();
			}
			
			this->callback(ec);
		}
		
		void perform()
		{
			parent.async_send_frame(std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_end_write
	(Callback &&cb)
		{
		if (write_state_ != write_state_t::writing_message) {
			cb(make_error_code(boost::system::errc::invalid_argument));
			return;
		}
		
		// This is the final frame of the message.
		sending_final_frame_ = true;
			
		end_write_op<typename std::remove_reference<Callback>::type> op
		(*this, std::forward<Callback>(cb));
		op.perform();
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::send_frame_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		
	public:
		send_frame_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		send_frame_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec, std::size_t count)
		{
			if (ec || count != parent.write_buffer_pos_ - parent.write_buffer_real_start_pos) {
				// Terrible failure. We might not even be able to send the Close frame.
				
				switch (parent.state_) {
					case state_t::closing_active:
					case state_t::closing_active_sent_close:
					case state_t::closing_passive:
					case state_t::closing_sending_close:
					case state_t::connected:
						parent.state_ = state_t::closed;
						parent.close_done(boost::system::error_code());
						break;
					case state_t::closed:
						break;
				}
				
				if (ec)
					this->callback(ec);
				else
					this->callback(make_error_code(boost::system::errc::protocol_error));
				
				return;
			}
			
			// Succeeded.
			parent.write_buffer_pos_ = parent.write_buffer_start_pos;
			this->callback(ec);
		}
		
		void perform()
		{
			assert(parent.write_buffer_pos_ >= parent.write_buffer_real_start_pos);
			_asio::async_write(parent.next_layer(),
							   _asio::buffer(parent.write_buffer_.data() + parent.write_buffer_real_start_pos,
											 parent.write_buffer_pos_ - parent.write_buffer_real_start_pos),
							   std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_send_frame
	(Callback &&cb)
	{
		assert(write_state_ == write_state_t::writing_message);
		
		boost::optional<detail::web_socket_masker> masker;
		boost::optional<std::uint32_t> key;
		
		if (masking_key_provider) {
			key = masking_key_provider();
		}
		if (key) {
			masker.emplace(*key);
		} else {
			masker = boost::none;
		}
		
		// Make a frame header.
		web_socket_frame_header hdr = sent_message_header_.frame_header();
		hdr.fin = sending_final_frame_;
		hdr.payload_length = static_cast<std::uint64_t>(write_buffer_pos_ - write_buffer_start_pos);
		hdr.masking_key = key;
		if (!sending_first_frame_) {
			hdr.opcode = web_socket_opcode::continuation;
		} else {
			sending_first_frame_ = false;
		}
		
		// Compute the header length.
		std::size_t headerSize = 2;
		if (hdr.masking_key) {
			headerSize += 4;
		}
		if (hdr.payload_length > 65535) {
			headerSize += 8;
		} else if (hdr.payload_length >= 126) {
			headerSize += 2;
		}
		assert(headerSize < write_buffer_start_pos);
		write_buffer_real_start_pos = write_buffer_start_pos - headerSize;
		
		// Make the header contents.
		char *data = write_buffer_.data() + write_buffer_real_start_pos;
		
		// FIXME: unaligned memory access possible
		std::uint16_t prologue = 0;
		if (hdr.fin) prologue |= 0x8000;
		if (hdr.reserved1) prologue |= 0x4000;
		if (hdr.reserved2) prologue |= 0x2000;
		if (hdr.reserved3) prologue |= 0x1000;
		prologue |= static_cast<std::uint16_t>(hdr.opcode) << 8;
		if (hdr.masking_key) prologue |= 0x80;
		if (hdr.payload_length > 65535) {
			prologue |= 127;
		} else if (hdr.payload_length >= 126) {
			prologue |= 126;
		} else {
			prologue |= static_cast<std::uint16_t>(hdr.payload_length);
		}
		*reinterpret_cast<std::uint16_t *>(data) = detail::host_to_network(prologue);
		data += 2;
		
		if (hdr.payload_length > 65535) {
			*reinterpret_cast<std::uint64_t *>(data) = detail::host_to_network(hdr.payload_length);
			data += 8;
		} else if (hdr.payload_length >= 126) {
			*reinterpret_cast<std::uint16_t *>(data) = detail::host_to_network(static_cast<std::uint16_t>(hdr.payload_length));
			data += 2;
		}
		
		if (hdr.masking_key) {
			*reinterpret_cast<std::uint32_t *>(data) = *hdr.masking_key;
			data += 4;
		}
		
		assert(data - write_buffer_.data() == write_buffer_start_pos);
		
		// Mask the payload.
		if (masker) {
			masker->apply(data, write_buffer_pos_ - write_buffer_start_pos);
		}
		
		send_frame_op<typename std::remove_reference<Callback>::type> op
		(*this, std::forward<Callback>(cb));
		op.perform();
	}
	
	
	template <class NextLayer>
	template <class ConstBufferSequence, class Callback>
	class web_socket<NextLayer>::send_message_op: public detail::intermediate_op<Callback>
	{
		enum class op_state_t {
			begin,
			write,
			end
		};
		web_socket& parent;
		ConstBufferSequence seq;
		op_state_t state = op_state_t::begin;
		
	public:
		template <class ConstBufferSequenceArg>
		send_message_op(web_socket &parent, ConstBufferSequenceArg&& seq, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<ConstBufferSequenceArg>(seq)) { }
		template <class ConstBufferSequenceArg>
		send_message_op(web_socket &parent, ConstBufferSequenceArg&& seq, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent),
		seq(std::forward<ConstBufferSequenceArg>(seq)) { }
		
		void operator () (const boost::system::error_code& ec, std::size_t count = 0)
		{
			if (ec) {
				this->callback(ec);
				return;
			}
			
			switch (state) {
				case op_state_t::begin:
					state = op_state_t::write;
					break;
				case op_state_t::write:
					state = op_state_t::end;
					break;
				case op_state_t::end:
					this->callback(ec);
					return;
			}
			perform();
		}
		
		void perform(const web_socket_message_header& header = web_socket_message_header())
		{
			switch (state) {
				case op_state_t::begin:
					parent.async_begin_write(header, std::move(*this));
					break;
				case op_state_t::write:
					_asio::async_write(parent, seq, std::move(*this));
					break;
				case op_state_t::end:
					parent.async_end_write(std::move(*this));
					break;
			}
		}
	};
	
	template <class NextLayer>
	template <class ConstBufferSequence, class Callback>
	void web_socket<NextLayer>::async_send_message(const web_socket_message_header &header,
												   ConstBufferSequence &&buffers, Callback &&cb)
	{
		send_message_op<
		typename std::remove_reference<ConstBufferSequence>::type,
		typename std::remove_reference<Callback>::type>
		op(*this, std::forward<ConstBufferSequence>(buffers), std::forward<Callback>(cb));
		op.perform(header);
	}
	
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::pong_op: public detail::intermediate_op<Callback>
	{
		enum class op_state_t {
			begin,
			write,
			end
		};
		web_socket& parent;
		op_state_t state = op_state_t::begin;
		std::string data;
		
	public:
		pong_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		pong_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		
		void operator () (const boost::system::error_code& ec, std::size_t count = 0)
		{
			if (ec) {
				return;
			}
			
			// This op doesn't call the callback.
			
			switch (state) {
				case op_state_t::begin:
					state = op_state_t::write;
					break;
				case op_state_t::write:
					state = op_state_t::end;
					break;
				case op_state_t::end:
					return;
			}
			
			perform();
		}
		
		void perform()
		{
			switch (state) {
				case op_state_t::begin:
					{
						web_socket_message_header header;
						header.opcode = web_socket_opcode::pong;
						parent.async_begin_write(header, std::move(*this));
					}
					break;
				case op_state_t::write:
					data = *parent.pending_pong;
					parent.pending_pong = boost::none;
					_asio::async_write(parent, _asio::buffer(data), std::move(*this));
					break;
				case op_state_t::end:
					parent.async_end_write(std::move(*this));
					break;
			}
		}
	};
	
	template <class NextLayer>
	auto web_socket<NextLayer>::alloc_awaited_pong() ->
	typename awaited_pong::ptr
	{
		typename awaited_pong::ptr ap;
		if (!awaited_pong_pool.empty()) {
			ap = std::move(awaited_pong_pool.back());
			awaited_pong_pool.pop_back();
		} else {
			ap = std::make_shared<awaited_pong>();
		}
		
		ap->linked = false;
		ap->previous.reset();
		ap->next.reset();
		
		return std::move(ap);
	}
	
	template <class NextLayer>
	void web_socket<NextLayer>::return_awaited_pong(const typename awaited_pong::ptr &p)
	{
		if (awaited_pong_pool.size() < 4) {
			p->previous.reset();
			p->next.reset();
			awaited_pong_pool.emplace_back(p);
		}
	}
	
	template <class NextLayer>
	void web_socket<NextLayer>::enqueue_awaited_pong(const typename awaited_pong::ptr &p)
	{
		assert(p);
		assert(!p->previous);
		assert(!p->linked);
		
		// Link awaited_pong
		p->previous = last_awaited_pong;
		if (last_awaited_pong)
			last_awaited_pong->next = p;
		last_awaited_pong = p;
		p->linked = true;
		
		// Add to awaited_pong_map
		awaited_pong_map[p->data] = p;
	}
	
	// Called when web_socket received the Pong frame.
	// The endpoint may elect to send a Pong frame for only the
	// most recently processed Ping frame [RFC blah 5.5.3. Pong],
	// so receiving a pong indicates that formerly sent pings were
	// processed by the peer too.
	template <class NextLayer>
	void web_socket<NextLayer>::received_pong(const std::string &data)
	{
		auto it = awaited_pong_map.find(data);
		if (it == awaited_pong_map.end())
			return;
		
		// Unlink the awaited_pong from the list
		auto item = std::move(it->second);
		
		auto nextItem = item->next.lock();
		if (nextItem) {
			assert(nextItem->previous == item);
			nextItem->previous.reset();
		}
		
		// Remove unlinked items from awaited_pong_map
		for (auto i = item; i; i = i->previous) {
			i->linked = false;
			
			auto it = awaited_pong_map.find(i->data);
			if (it != awaited_pong_map.end() &&
				it->second == i)
				awaited_pong_map.erase(it);
		}
		
		// Call callbacks and return awaited_pongs to the pool
		// (this done after the previous step is all done because
		//  calling callback might cause received_pong to be called
		//  recursively)
		for (auto i = std::move(item); i; i = i->previous) {
			auto handler = std::move(i->handler);
			handler(boost::system::error_code());
			
			return_awaited_pong(i);
		}
	}
	
	
	
	template <class NextLayer>
	void web_socket<NextLayer>::fail_awaited_pong
	(const typename awaited_pong::ptr &ap,
	 const boost::system::error_code &ec)
	{
		assert(ap);
		
		if (!ap->linked)
			return;
		
		// Remove from awaited_pong_map
		auto it = awaited_pong_map.find(ap->data);
		if (it != awaited_pong_map.end() &&
			it->second == ap)
			awaited_pong_map.erase(it);
		
		// Unlink
		if (ap == last_awaited_pong) {
			if (ap->previous)
				ap->previous->next.reset();
			last_awaited_pong = ap->previous;
		} else {
			auto nextItem = ap->next.lock();
			if (nextItem)
				nextItem->previous = ap->previous;
			if (ap->previous)
				ap->previous->next = nextItem;
		}
		
		ap->linked = false;
		
		// Call callback
		auto handler = std::move(ap->handler);
		handler(ec);
		
		return_awaited_pong(ap);
	}

	
	template <class NextLayer>
	void web_socket<NextLayer>::fail_all_awaited_pong
	(const boost::system::error_code &ec)
	{
		typename awaited_pong::ptr item;
		item.swap(last_awaited_pong);
		
		awaited_pong_map.clear();
		
		for (; item; item = item->previous) {
			auto handler = std::move(item->handler);
			handler(ec);
		}
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::ping_op :
	public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		std::shared_ptr<const std::string> const data;
		typename awaited_pong::ptr ap;
	public:
		ping_op(web_socket& parent, const std::string &data, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		data(std::make_shared<std::string>(data)),
		parent(parent) { }
		ping_op(web_socket& parent, const std::string &data, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		data(std::make_shared<std::string>(data)),
		parent(parent) { }
		
		void operator () (const boost::system::error_code& code)
		{
			if (code) {
				parent.fail_awaited_pong(ap, code);
			}
		}
		
		void perform()
		{
			ap = parent.alloc_awaited_pong();
			ap->handler = this->callback;
			ap->data = *data;
			ap->linked = false;
			parent.enqueue_awaited_pong(ap);
			
			web_socket_message_header header;
			header.opcode = web_socket_opcode::ping;
			parent.async_send_message(header,
									  _asio::buffer(data->data(), data->size()),
									  std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_send_ping(const std::string &data,
												Callback &&cb)
	{
		
		ping_op<
		typename std::remove_reference<Callback>::type>
		op(*this, data, std::forward<Callback>(cb));
		op.perform();
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::close_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		std::shared_ptr<std::vector<char>> buffer;
	public:
		close_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		close_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec)
		{
			// Sending "Close" done.
			switch (parent.state_) {
				case state_t::closing_active:
					parent.state_ = state_t::closing_active_sent_close;
					break;
				case state_t::closing_sending_close:
					parent.state_ = state_t::closed;
					parent.close_done(ec);
					break;
				case state_t::closing_active_sent_close:
				case state_t::closing_passive:
				case state_t::connected:
				case state_t::closed:
					break;
			}
			
			if (ec) {
				return;
			}
			
			// This op doesn't call the callback.
		}
		
		void perform(std::uint16_t status_code, const std::string &reason)
		{
			buffer = std::make_shared<std::vector<char>>(reason.size() + 2);
			
			// FIXME: possible unaligned access
			*reinterpret_cast<std::uint16_t *>(buffer->data()) = detail::host_to_network(status_code);
			std::memcpy(buffer->data() + 2, reason.data(), reason.size());
			
			web_socket_message_header hdr;
			hdr.opcode = web_socket_opcode::connection_close;
			
			parent.async_send_message(hdr, _asio::buffer(*buffer), std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	class web_socket<NextLayer>::discard_receive_message_op: public detail::intermediate_op<Callback>
	{
		web_socket& parent;
		
	public:
		discard_receive_message_op(web_socket &parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		discard_receive_message_op(web_socket &parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent)  { }
		
		void operator () (const boost::system::error_code& ec)
		{
			if (ec) {
				return;
			}
			perform();
		}
		
		void perform()
		{
			parent.async_receive_message(std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket<NextLayer>::async_shutdown(std::uint16_t status_code, const std::string &reason, bool fail, Callback&& cb)
	{
		// Payload of the control frame cannot be longer than 125 bytes.
		assert(reason.size() <= 123);
		
		add_shutdown_handler(std::forward<Callback>(cb));
		
		switch (state_) {
			case state_t::closed:
				// Already closed.
				close_done(boost::system::error_code());
				break;
			case state_t::closing_active:
				if (fail) {
					// Switch to "Fail" closure.
					state_ = state_t::closing_sending_close;
				} else {
					// Shutdown is already initiated.
				}
				break;
			case state_t::closing_active_sent_close:
				if (fail) {
					// Switch to "Fail" closure.
					state_ = state_t::closed;
					close_done(boost::system::error_code());
				} else {
					// Shutdown is already initiated.
				}
				break;
			case state_t::closing_sending_close:
				// Shutdown is already initiated.
				break;
			case state_t::connected:
			case state_t::closing_passive:
				// Perform the WebSocket Close Handshake.
				
				if (fail)
					state_ = state_t::closing_sending_close;
				else
					state_ = state_t::closing_active;
				
				// Wait for the Close reponse.
				if (read_state_ == read_state_t::not_reading) {
					discard_receive_message_op<typename std::remove_reference<Callback>::type>
					(*this, std::forward<Callback>(cb)).perform();
				}
				
				// Send close.
				close_op<typename std::remove_reference<Callback>::type> op
				(*this, std::forward<Callback>(cb));
				op.perform(status_code, reason);
				break;
		}
	}
	
	class http_absolue_path
	{
	public:
		
		http_absolue_path() = default;
		http_absolue_path(const std::string &abspath)
		{
			auto index = abspath.find('?');
			
			auto url_decode = [] (const std::string& str) -> std::string
			{
				auto hex_decode = [] (char c) -> int
				{
					if (c >= '0' && c <= '9')
						return c - '0';
					else if (c >= 'a' && c <= 'f')
						return c - 'a' + 10;
					else if (c >= 'A' && c <= 'F')
						return c - 'A' + 10;
					else
						return -1;
				};
				
				std::string output;
				output.reserve(str.size());
				
				for (std::size_t i = 0; i < str.size(); ) {
					if (str[i] == '%' && i + 2 < str.size()) {
						int digit1 = hex_decode(str[i + 1]);
						int digit2 = hex_decode(str[i + 2]);
						if (digit1 != -1 && digit2 != -1) {
							char ch = static_cast<char>((digit1 << 4) | digit2);
							output.append(1, ch);
							i += 3;
							continue;
						}
					} else if (str[i] == '+') {
						output.append(1, ' ');
						++i;
					} else {
						output.append(1, str[i]);
						++i;
					}
				}
				
				return output;
			};
			
			if (index == std::string::npos) {
				encoded_path = abspath;
			} else {
				encoded_path = abspath.substr(0, index);
				encoded_query = abspath.substr(index + 1);
			}
			
			path = url_decode(encoded_path);
			
			if (encoded_query.size() > 0) {
				std::list<std::string> parts;
				boost::split(parts, encoded_query,
							 [] (char c) { return c == '&'; },
							 boost::token_compress_on);
				for (const auto& part: parts) {
					std::string key, value;
					index = part.find('=');
					if (index == std::string::npos) {
						key = part;
					} else {
						key = part.substr(0, index);
						value = part.substr(index + 1);
					}
					
					query.emplace_back(url_decode(key), url_decode(value));
				}
			}
			
		}
		
		std::string encoded_path;
		std::string path;
		std::string encoded_query;
		std::list<std::pair<std::string, std::string>> query;
		
	};
	
	namespace http_status_codes
	{
		enum http_status_code
		{
			bad_request = 400,
			unauthorized = 401,
			payment_required = 402,
			forbidden = 403,
			not_found = 404,
			method_not_allowed = 405,
			not_acceptable = 406,
			proxy_authentication_required = 407,
			request_timeout = 408,
			conflict = 409,
			gone = 410,
			length_required = 411,
			precondition_failed = 412,
			request_entity_too_large = 413,
			request_uri_too_long = 414,
			unsupported_media_type = 415,
			request_range_not_satisfiable = 416,
			expectation_failed = 417,
			internal_server_error = 500,
			not_implemented = 501,
			bad_gateway = 502,
			service_unavailable = 503,
			gateway_timeout = 504,
			http_version_not_supported = 505
		};
	}

	const char *http_status_text(int status);
	
	enum class http_version
	{
		http_1_0, // Prohibited by WebSocket. Never used. [RFC blah 4.2.1]
		http_1_1
	};
	
	// Simple HTTP server that only accepts WebSocket.
	template <class NextLayer>
	class web_socket_server
	{
		enum class handshake_state_t
		{
			initial,
			reading_header,
			read_header,
			sending_header,
			done,
			fail
		};
		
	public:
		using next_layer_type = typename std::remove_reference<NextLayer>::type;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;
		
		next_layer_type& next_layer() { return next_; }
		lowest_layer_type& lowest_layer() { return next_layer().lowest_layer(); }
		
		struct content_stream
		{
			using next_layer_type = web_socket_server::next_layer_type;
			using lowest_layer_type = web_socket_server::lowest_layer_type;
			next_layer_type& next_layer() { return server.next_layer(); }
			lowest_layer_type& lowest_layer() { return server.lowest_layer(); }
			
			web_socket_server &server;
			content_stream(web_socket_server &server) : server(server)
			{ }
			
			template <class MutableBufferSequence, class Callback>
			void async_read_some(const MutableBufferSequence& buffers, Callback&& cb);
			template <class ConstBufferSequence, class Callback>
			void async_write_some(const ConstBufferSequence& buffers, Callback&& cb);
		};
		
		using socket_type = web_socket<content_stream&>;
		
		template <class ...Args>
		web_socket_server(Args &&...args) :
		next_(std::forward<Args>(args)...),
		content_stream_(*this),
		socket_(content_stream_),
		streambuf_(1024) { }
		
		socket_type& socket() { return socket_; }
		
		http_version http_version() { return http_version_; }
		const http_absolue_path& http_absolute_path() const { return http_absolute_path_; }
		const std::string& host() const { return http_host_; }
		const std::string& origin() const { return http_origin_; }
		const std::string& encoded_protocols() const { return http_sec_websocket_protocol_; }
		
		bool handshake_done() const { return handshake_state_ == handshake_state_t::done; }
		
		template <class Callback>
		void async_start_handshake(Callback&&);
		
		template <class Callback>
		void async_accept(const std::string &protocol, Callback&&);
		
		template <class Callback>
		void async_reject(int statusCode, Callback&&);
		
	private:
		NextLayer next_;
		content_stream content_stream_;
		socket_type socket_;
		handshake_state_t handshake_state_ = handshake_state_t::initial;
		boost::asio::streambuf streambuf_;
		std::vector<char> buffer_;
		std::string const separator_ { "\r\n" };
		
		bool reading_request_line_ = true;
		
		enum http_version http_version_;
		http_absolue_path http_absolute_path_;
		std::string http_host_;
		std::string http_origin_;
		bool http_upgrade_valid_ = false;
		bool http_connection_valid_ = false;
		std::string http_sec_websocket_key_;
		std::string http_sec_websocket_protocol_;
		std::string http_sec_websocket_version_;
		std::size_t http_header_total = 0;
	
		template <class Callback>
		class start_handshake_op;
		
		template <class Callback>
		class accept_op;
		
		template <class Callback>
		class reject_op;
	};
	
	template <class NextLayer>
	template <class MutableBufferSequence, class Callback>
	void web_socket_server<NextLayer>::content_stream::
	async_read_some(const MutableBufferSequence& buffers, Callback&& cb)
	{
		if (server.handshake_state_ != handshake_state_t::done) {
			cb(make_error_code(boost::system::errc::not_connected), 0);
			return;
		}
		
		server.next_layer().async_read_some(buffers, std::forward<Callback>(cb));
	}
	
	template <class NextLayer>
	template <class ConstBufferSequence, class Callback>
	void web_socket_server<NextLayer>::content_stream::
	async_write_some(const ConstBufferSequence& buffers, Callback&& cb)
	{
		if (server.handshake_state_ != handshake_state_t::done) {
			cb(make_error_code(boost::system::errc::not_connected), 0);
			return;
		}
		
		server.next_layer().async_write_some(buffers, std::forward<Callback>(cb));
	}
	
	namespace detail
	{
		struct web_socket_server_regex_t
		{
			std::string const method_ { "(OPTIONS|GET|HEAD|POST|PUT|DELETE|TRACE|CONNECT)" };
			std::string const request_uri_ { "(\\*|(?:[a-z0-9]+\\:\\/*[^/]+)?\\/[^ ]*)" }; // "authority" format is not included
			std::string const http_version_ { "HTTP/([0-9]{1,4})\\.([0-9]{1,4})" };
			std::regex const requestLineRegex
			{ method_ + " " + request_uri_ + " " + http_version_,
				std::regex_constants::optimize | std::regex_constants::icase };
			std::regex const absoluteUriRegex
			{ "[a-z0-9]+\\:\\/*[^/]+(\\/[^ ]*)",
				std::regex_constants::optimize | std::regex_constants::icase };
		};
		
		extern const web_socket_server_regex_t& web_socket_server_regex();
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket_server<NextLayer>::start_handshake_op :
	public detail::intermediate_op<Callback>
	{
		web_socket_server& parent;
		boost::system::error_code last_error;
	public:
		start_handshake_op(web_socket_server& parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		start_handshake_op(web_socket_server& parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		
		void operator () (const boost::system::error_code &error, std::size_t count = 0)
		{
			if (last_error) {
				// Done sending failing response.
				this->callback(last_error);
				return;
			}
			
			if (error) {
				// TODO: raise "Request URI Too Long" when URI is too long?
				if (false) {
					fail(make_error_code(boost::system::errc::protocol_error),
						 http_status_codes::request_uri_too_long);
				} else {
					fail(error, http_status_codes::bad_request);
				}
				return;
			} else if (count < 2) {
				fail(error, http_status_codes::bad_request);
				return;
			}
			
			auto& buf = parent.buffer_;
			buf.resize(count);
			std::istream stream(&parent.streambuf_);
			stream.read(buf.data(), count);
			std::string s(buf.data(), buf.size() - 2);
			
			parent.http_header_total += count;
			if (parent.http_header_total > 65536 * 2) {
				// Limit HTTP header size to prevent DoS.
				fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::request_entity_too_large);
				return;
			}
			
			if (parent.reading_request_line_) {
				// HTTP Requset Line
				if (s.size() == 0) {
					// Empty request line.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				std::smatch matches;
				if (!std::regex_match(s, matches, detail::web_socket_server_regex().requestLineRegex)) {
					// Unrecognizable request line.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				assert(matches.size() == 5);
				
				// Check HTTP-Version.
				if (matches[3].str() == "1") {
					if (matches[4].str() == "0") {
						parent.http_version_ = http_version::http_1_0;
					} else if (matches[4].str() == "1") {
						parent.http_version_ = http_version::http_1_1;
					} else {
						goto unsupportedHttpVersion;
					}
				} else {
					// Unsupported HTTP version.
				unsupportedHttpVersion:
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::http_version_not_supported);
					return;
				}
				
				// Check Method.
				const auto& httpMethod = matches[1].str();
				if (!boost::iequals(httpMethod, "GET")) {
					// Unsupported method.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::method_not_allowed);
					return;
				}
				
				// Check Request-URI.
				auto requestUri = matches[2].str();
				
				if (requestUri == "*" || requestUri.size() == 0) {
					// Invalid Request-URI.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::not_implemented);
					return;
				}
				
				if (requestUri[0] != '/') {
					// absoluteURI. Convert to abs_path.
					if (!std::regex_match(requestUri, matches, detail::web_socket_server_regex().absoluteUriRegex)) {
						// Failed (this is unlikely to happen)
						fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::internal_server_error);
						return;
					}
					
					requestUri = matches[1].str();
				}
				
				parent.http_absolute_path_ = http_absolue_path(requestUri);
				
				parent.reading_request_line_ = false;
			} else if (s.empty()) {
				// End of the HTTP Request.
				// Validate the header.
				
				if (parent.http_version_ == http_version::http_1_0) {
					// HTTP 1.0 is prohibited by [RFC 4.2.1].
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				if (parent.http_version_ == http_version::http_1_1 &&
					parent.http_host_.empty()) {
					// HTTP 1.1 mandates Host header.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				if (!parent.http_upgrade_valid_ ||
					!parent.http_connection_valid_) {
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				if (parent.http_sec_websocket_key_.size() != 24) {
					// Sec-WebSocket-Key must be 24 bytes long.
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				if (parent.http_sec_websocket_version_ != "13") {
					fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::bad_request);
					return;
				}
				
				// Validated.
				parent.handshake_state_ = handshake_state_t::read_header;
				this->callback(boost::system::error_code());
				
				return;
				
			} else {
				// HTTP Header
				
				// Make sure there's no line continuation.
				// (If header contains a quoted-string, it might span
				//  across the lines. Might be HTTP injection attack.)
				{
					bool quot = false;
					for (std::size_t i = 0; i < s.size(); ) {
						if (quot) {
							if (s[i] == '"') {
								quot = false;
								++i;
							} else if (s[i] == '\\') {
								i += 2;
							} else {
								++i;
							}
						} else {
							if (s[i] == '"') {
								quot = true;
							}
							++i;
						}
					}
					
					if (quot) {
						// quoted-string spanning across the lines.
						fail(make_error_code(boost::system::errc::protocol_error), http_status_codes::not_implemented);
						return;
					}
				}
			
				auto index = s.find(':');
				auto name = s.substr(0, index);
				auto value = s.substr(index + 1);
				boost::trim(name);
				boost::trim(value);
				
				if (boost::iequals(name, "Host")) {
					parent.http_host_ = value;
				} else if (boost::iequals(name, "Upgrade")) {
					parent.http_upgrade_valid_ = boost::iequals(value, "websocket");
				} else if (boost::iequals(name, "Connection")) {
					parent.http_connection_valid_ = boost::iequals(value, "Upgrade");
				} else if (boost::iequals(name, "Origin")) {
					parent.http_origin_ = value;
				} else if (boost::iequals(name, "Sec-WebSocket-Version")) {
					parent.http_sec_websocket_version_ = value;
				} else if (boost::iequals(name, "Sec-WebSocket-Key")) {
					parent.http_sec_websocket_key_ = value;
				} else if (boost::iequals(name, "Sec-WebSocket-Protocol")) {
					parent.http_sec_websocket_protocol_ = value;
				} else {
					// Ignore for now...
				}
			}
			
			perform();
			
		}
		
		void perform()
		{
			_asio::async_read_until(parent.next_layer(), parent.streambuf_,
									parent.separator_, std::move(*this));
		}
		
	private:
		void fail(const boost::system::error_code &error,
				  int status)
		{
			last_error = error;
			parent.handshake_state_ = handshake_state_t::read_header;
			parent.async_reject(status, std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket_server<NextLayer>::async_start_handshake(Callback &&cb)
	{
		start_handshake_op<typename std::remove_reference<Callback>::type>
		op(*this, std::forward<Callback>(cb));
		op.perform();
	}
	
	namespace detail
	{
		std::size_t base64_encode(char *dest, const char *src, std::size_t len);
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket_server<NextLayer>::accept_op :
	public detail::intermediate_op<Callback>
	{
		web_socket_server& parent;
		std::shared_ptr<std::string> buffer;
	public:
		accept_op(web_socket_server& parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		accept_op(web_socket_server& parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		
		void operator () (const boost::system::error_code &error, std::size_t count)
		{
			if (error) {
				parent.handshake_state_ = handshake_state_t::fail;
				this->callback(error);
				return;
			} else if (count < buffer->size()) {
				parent.handshake_state_ = handshake_state_t::fail;
				this->callback(make_error_code(boost::system::errc::protocol_error));
				return;
			}
			
			parent.handshake_state_ = handshake_state_t::done;
			this->callback(boost::system::error_code());
		}
		
		void perform(const std::string& protocol)
		{
			// Build Sec-WebSocket-Accept
			boost::uuids::detail::sha1 sha;
			sha.process_bytes(parent.http_sec_websocket_key_.data(),
							  parent.http_sec_websocket_key_.size());
			sha.process_bytes("258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
							  36);
			union {
				unsigned int digest[5];
			};
			char digestBytes[20];
			sha.get_digest(digest);
			
			for (int i = 0; i < 5; ++i) {
				auto dig = digest[i];
				digestBytes[i * 4] = static_cast<char>(dig >> 24);
				digestBytes[i * 4 + 1] = static_cast<char>(dig >> 16);
				digestBytes[i * 4 + 2] = static_cast<char>(dig >> 8);
				digestBytes[i * 4 + 3] = static_cast<char>(dig);
			}
			
			char digestBase64[29];
			digestBase64[28] = 0;
			
			detail::base64_encode(digestBase64, digestBytes, 20);
			
			std::ostringstream s;
			s << "HTTP/1.1 101 Switching Protocols\r\n";
			s << "Upgrade: websocket\r\n";
			s << "Connection: Upgrade\r\n";
			s << "Sec-WebSocket-Protocol: " << protocol << "\r\n";
			s << "Sec-WebSocket-Accept: " << digestBase64 << "\r\n";
			s << "\r\n";
			buffer = std::make_shared<std::string>(s.str());
			
			_asio::async_write(parent.next_,
							   _asio::buffer(buffer->data(), buffer->size()),
							   std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket_server<NextLayer>::async_accept
	(const std::string &protocol, Callback &&cb)
	{
		if (handshake_state_ != handshake_state_t::read_header) {
			cb(make_error_code(boost::system::errc::invalid_argument));
			return;
		}
		
		handshake_state_ = handshake_state_t::sending_header;
		
		accept_op<typename std::remove_reference<Callback>::type>
		op(*this, std::forward<Callback>(cb));
		op.perform(protocol);
	}
	
	template <class NextLayer>
	template <class Callback>
	class web_socket_server<NextLayer>::reject_op :
	public detail::intermediate_op<Callback>
	{
		web_socket_server& parent;
		std::shared_ptr<std::string> buffer;
	public:
		reject_op(web_socket_server& parent, const Callback& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		reject_op(web_socket_server& parent, Callback&& cb):
		detail::intermediate_op<Callback>(cb),
		parent(parent) { }
		
		void operator () (const boost::system::error_code &error, std::size_t count)
		{
			if (error) {
				parent.handshake_state_ = handshake_state_t::fail;
				this->callback(error);
				return;
			} else if (count < buffer->size()) {
				parent.handshake_state_ = handshake_state_t::fail;
				this->callback(make_error_code(boost::system::errc::protocol_error));
				return;
			}
			
			parent.handshake_state_ = handshake_state_t::fail;
			this->callback(boost::system::error_code());
		}
		
		void perform(int status)
		{
			std::ostringstream s;
			switch (parent.http_version_) {
				case http_version::http_1_0:
					s << "HTTP/1.0";
					break;
				case http_version::http_1_1:
					s << "HTTP/1.1";
					break;
			}
			s << " " << status << " " <<
			http_status_text(status) << "\r\n";
			s << "\r\n";
			buffer = std::make_shared<std::string>(s.str());
			
			_asio::async_write(parent.next_,
							   _asio::buffer(buffer->data(), buffer->size()),
							   std::move(*this));
		}
	};
	
	template <class NextLayer>
	template <class Callback>
	void web_socket_server<NextLayer>::async_reject
	(int statusCode, Callback &&cb)
	{
		if (handshake_state_ != handshake_state_t::read_header) {
			cb(make_error_code(boost::system::errc::invalid_argument));
			return;
		}
		
		handshake_state_ = handshake_state_t::sending_header;
		
		reject_op<typename std::remove_reference<Callback>::type>
		op(*this, std::forward<Callback>(cb));
		op.perform(statusCode);
	}
	
}

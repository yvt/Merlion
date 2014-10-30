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
#include <list>
#include <unordered_map>

namespace asiows
{
	namespace _asio = boost::asio;
	
	namespace detail
	{
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
			typename BaseBufferSequence::const_iterator baseEnd;
			typename BaseBufferSequence::const_iterator baseLast;
			std::size_t finalLength = 0;
			void init()
			{
				std::uintmax_t remaining = length;
				baseEnd = baseSequence.begin();
				baseLast = baseSequence.begin();
				while (baseEnd != baseSequence.end()) {
					auto sz = boost::asio::buffer_size(*baseEnd);
					if (baseEnd != baseSequence.begin())
						++baseLast;
					++baseEnd;
					if (sz >= remaining) {
						finalLength = static_cast<std::size_t>(remaining);
						break;
					} else {
						remaining -= sz;
						if (baseEnd == baseSequence.end()) {
							baseLast = baseEnd;
						}
					}
				}
			}
		public:
			class const_iterator :
			public std::iterator<std::bidirectional_iterator_tag, typename BaseBufferSequence::value_type>
			{
				const buffer_clipper *clipper;
				typename BaseBufferSequence::const_iterator baseIt;
				using difference_type = std::ptrdiff_t;
				using value_type = typename BaseBufferSequence::value_type;
				const_iterator() = default;
				const_iterator(const const_iterator&) = default;
				const_iterator(const_iterator&&) = default;
				explicit const_iterator(const buffer_clipper& clipper,
										const typename BaseBufferSequence::const_iterator& baseIt):
				baseIt(baseIt), clipper(&clipper) { }
				const_iterator & operator -- () { --baseIt; return *this; }
				const_iterator & operator ++ () { ++baseIt; return *this; }
				value_type operator * () const {
					if (baseIt == clipper->baseLast) {
						// FIXME: support const buffer
						void *ptr = boost::asio::buffer_cast<void *>(*baseIt);
						return _asio::mutable_buffer(ptr, clipper->finalLength);
					} else {
						return *baseIt;
					}
				}
				bool operator == (const const_iterator& other) const { return baseIt == other.baseIt; }
				bool operator != (const const_iterator& other) const { return baseIt != other.baseIt; }
				const_iterator operator -- (int) { auto old = baseIt--; return const_iterator(clipper, old); }
				const_iterator operator ++ (int) { auto old = baseIt++; return const_iterator(clipper, old); }
				std::ptrdiff_t operator - (const const_iterator& other) const
				{ return baseIt - other.baseIt; }
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
			
			const_iterator begin() const { return const_iterator(*this, baseSequence.begin()); }
			const_iterator end() const { return const_iterator(*this, baseEnd); }
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
					--len;
				}
				while (len >= 4) {
					*reinterpret_cast<std::uint32_t*>(buffer) =
					*reinterpret_cast<std::uint32_t*>(buffer) ^ mask_.u32;
					len -= 4;
				}
				while (len > 0) {
					*buffer = *buffer ^ mask_.u8[pos];
					pos = (pos + 1) & 3;
					--len;
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
	
	enum class web_socket_opcode
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
		return (static_cast<int>(opcode) & 0x8) != 0;
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
				lenSize += 2;
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
							// FIXME: big endian support
							std::uint16_t headerVal = *reinterpret_cast<std::uint16_t *>(bufferVec.data());
							auto& hdr = parent.header_;
							hdr.fin = (headerVal & 0x1) != 0;
							hdr.reserved1 = (headerVal & 0x2) != 0;
							hdr.reserved2 = (headerVal & 0x4) != 0;
							hdr.reserved3 = (headerVal & 0x8) != 0;
							hdr.opcode = static_cast<web_socket_opcode>((headerVal >> 4) & 15);
							if ((headerVal & 0x100) != 0)
								hdr.masking_key = 0;
							else
								hdr.masking_key = boost::none;
							hdr.payload_length = headerVal >> 9;
							
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
							
							// FIXME: big endian support
							// FIXME: unaligned memory access possible
							if (hdr.payload_length == 126) {
								hdr.payload_length = *reinterpret_cast<std::uint16_t const *>(buffer);
								buffer += 2;
								
								if (hdr.payload_length < 126) {
									// Not compact encoding
									parent.state_ = state_t::fail;
									this->callback(make_error_code(boost::system::errc::protocol_error));
									break;
								}
							} else if (hdr.payload_length == 127) {
								hdr.payload_length = *reinterpret_cast<std::uint64_t const *>(buffer);
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
						parent.masker->apply(bufferSequence);
					
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
		std::function<boost::optional<std::uint32_t>()> masking_key_provider;
		
		// Largest possible frame header size.
		const std::size_t write_buffer_start_pos = 14;
		
		enum class write_state_t {
			// Not writing.
			not_writing,
			
			writing_message
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
				boost_asio_handler_invoke_helpers::invoke([this, cbbox, hdr] {
					assert(write_state_ == write_state_t::not_writing);
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
			}
			
			// Control frame.
			parent.handle_control_frame(std::move(*this));
		}
		
		void perform()
		{
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
							  (parent.read_buffer_),
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
			receive_message_op<typename std::remove_reference<Callback>::type>
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
				parent.last_message_header_.payload_length = hdr.payload_length;
				
				// Continue reading.
				parent.async_read_some(seq, this->callback);
				return;
			}
			
			// Control frame.
			parent.handle_control_frame(std::move(*this));
		}
		
		void perform()
		{
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
			if (count != parent.reader.header().payload_length != count) {
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
						parent.close_status_code_ = *reinterpret_cast<const std::uint16_t *>(data);
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
							  detail::infinitely_repeated_buffer_sequence<_asio::mutable_buffer>
							  (_asio::buffer(parent.read_buffer_)),
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
		
		// FIXME: big endian support
		// FIXME: unaligned memory access possible
		std::uint16_t prologue = 0;
		if (hdr.fin) prologue |= 0x1;
		if (hdr.reserved1) prologue |= 0x2;
		if (hdr.reserved2) prologue |= 0x4;
		if (hdr.reserved3) prologue |= 0x8;
		prologue |= static_cast<std::uint16_t>(hdr.opcode) << 4;
		if (hdr.masking_key) prologue |= 0x100;
		if (hdr.payload_length > 65535) {
			prologue |= 127 << 9;
		} else if (hdr.payload_length >= 126) {
			prologue |= 126 << 9;
		} else {
			prologue = static_cast<std::uint16_t>(hdr.payload_length << 9);
		}
		*reinterpret_cast<std::uint16_t *>(data) = prologue;
		data += 2;
		
		if (hdr.payload_length > 65535) {
			*reinterpret_cast<std::uint64_t *>(data) = hdr.payload_length;
			data += 8;
		} else if (hdr.payload_length >= 126) {
			*reinterpret_cast<std::uint16_t *>(data) = static_cast<std::uint16_t>(hdr.payload_length);
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
			// FIXME: support big endian
			*reinterpret_cast<std::uint16_t *>(buffer->data()) = status_code;
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
	
}

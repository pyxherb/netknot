#ifndef _NETKNOT_SOCKET_H_
#define _NETKNOT_SOCKET_H_

#include "rc_buffer.h"

namespace netknot {
	constexpr static peff::UUID
		ADDRFAM_IPV4 = PEFF_UUID(82eaca08, cb19, 420c, 8829, b00086bae90c),
		ADDRFAM_IPV6 = PEFF_UUID(cfd0b918, e20e, 403b, a63a, e8be9974be3a);

	constexpr static peff::UUID
		SOCKET_TCP = PEFF_UUID(aaf1e2e8, f731, 4b2b, b261, 2db641edc248),
		SOCKET_UDP = PEFF_UUID(1b18418c, d36a, 4ed0, a135, 36dc249824db);

	struct Address {
		peff::UUID address_family;

		NETKNOT_FORCEINLINE Address(const peff::UUID &address_family) : address_family(address_family) {}
	};

	struct IPv4Address : public Address {
		uint8_t a = 0, b = 0, c = 0, d = 0;
		uint16_t port = 0;

		NETKNOT_FORCEINLINE IPv4Address() : Address(ADDRFAM_IPV4) {}
		NETKNOT_FORCEINLINE IPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : Address(ADDRFAM_IPV4), a(a), b(b), c(c), d(d) {}
		NETKNOT_FORCEINLINE IPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) : Address(ADDRFAM_IPV4), a(a), b(b), c(c), d(d), port(port) {}
	};

	class TranslatedAddress {
	public:
		NETKNOT_API TranslatedAddress();
		NETKNOT_API virtual ~TranslatedAddress();

		virtual void dealloc() = 0;
	};

	enum class AsyncTaskStatus {
		Ready = 0,
		Running,
		Done,
		Interrupted,
	};

	enum class AsyncTaskType : uint8_t {
		Read = 0,
		Write,
		Accept
	};

	class AsyncTask {
	private:
		std::atomic_size_t _ref_count = 0;
		AsyncTaskType _task_type;

	public:
		NETKNOT_API AsyncTask(AsyncTaskType taskType);
		NETKNOT_API ~AsyncTask();

		virtual void on_ref_zero() noexcept = 0;

		NETKNOT_FORCEINLINE size_t inc_ref(size_t globalRc) noexcept {
			return ++_ref_count;
		}

		NETKNOT_FORCEINLINE size_t dec_ref(size_t globalRc) noexcept {
			if (!--_ref_count) {
				on_ref_zero();
				return 0;
			}

			return _ref_count;
		}

		virtual AsyncTaskStatus get_status() = 0;
		virtual ExceptionPointer &get_except() = 0;

		NETKNOT_FORCEINLINE AsyncTaskType get_task_type() const noexcept {
			return _task_type;
		}
	};

	struct AsyncTaskDeleter {
		NETKNOT_FORCEINLINE void operator()(AsyncTask *task) const noexcept {
			task->on_ref_zero();
		}
	};

	class ReadAsyncTask : public AsyncTask {
	public:
		NETKNOT_API ReadAsyncTask();
		NETKNOT_API virtual ~ReadAsyncTask();

		virtual size_t get_cur_read_size() = 0;
		virtual size_t get_expected_read_size() = 0;
		virtual char *get_buffer() = 0;
		virtual RcBufferRef get_buffer_ref() = 0;
	};

	class WriteAsyncTask : public AsyncTask {
	public:
		NETKNOT_API WriteAsyncTask();
		NETKNOT_API virtual ~WriteAsyncTask();

		virtual size_t get_cur_written_size() = 0;
		virtual size_t get_expected_written_size() = 0;
	};

	class AcceptAsyncTask : public AsyncTask {
	public:
		NETKNOT_API AcceptAsyncTask();
		NETKNOT_API virtual ~AcceptAsyncTask();
	};

	class ReadAsyncCallback {
	private:
		std::atomic_size_t _ref_count = 0;

	public:
		NETKNOT_API ReadAsyncCallback();
		NETKNOT_API virtual ~ReadAsyncCallback();

		virtual void on_ref_zero() noexcept = 0;

		NETKNOT_FORCEINLINE size_t inc_ref(size_t globalRc) noexcept {
			return ++_ref_count;
		}

		NETKNOT_FORCEINLINE size_t dec_ref(size_t globalRc) noexcept {
			if (!--_ref_count) {
				on_ref_zero();
				return 0;
			}

			return _ref_count;
		}

		virtual ExceptionPointer on_status_changed(ReadAsyncTask *task) = 0;
	};

	class WriteAsyncCallback {
	private:
		std::atomic_size_t _ref_count = 0;

	public:
		NETKNOT_API WriteAsyncCallback();
		NETKNOT_API virtual ~WriteAsyncCallback();

		virtual void on_ref_zero() noexcept = 0;

		NETKNOT_FORCEINLINE size_t inc_ref(size_t globalRc) noexcept {
			return ++_ref_count;
		}

		NETKNOT_FORCEINLINE size_t dec_ref(size_t globalRc) noexcept {
			if (!--_ref_count) {
				on_ref_zero();
				return 0;
			}

			return _ref_count;
		}

		virtual ExceptionPointer on_status_changed(WriteAsyncTask *task) = 0;
	};

	class Socket;

	class AcceptAsyncCallback {
	private:
		std::atomic_size_t _ref_count = 0;

	public:
		NETKNOT_API AcceptAsyncCallback();
		NETKNOT_API virtual ~AcceptAsyncCallback();

		virtual void on_ref_zero() noexcept = 0;

		NETKNOT_FORCEINLINE size_t inc_ref(size_t globalRc) noexcept {
			return ++_ref_count;
		}

		NETKNOT_FORCEINLINE size_t dec_ref(size_t globalRc) noexcept {
			if (!--_ref_count) {
				on_ref_zero();
				return 0;
			}

			return _ref_count;
		}

		virtual ExceptionPointer on_accepted(Socket *socket) = 0;
	};

	template <typename Callback>
	class FnReadAsyncCallback final : public ReadAsyncCallback {
	private:
		peff::Alloc *_allocator;
		Callback _callback;

	public:
		static_assert(std::is_invocable_v<Callback, ReadAsyncTask *>, "The callback is malformed");

		NETKNOT_FORCEINLINE FnReadAsyncCallback(peff::Alloc *allocator, Callback &&callback) : _allocator(allocator), _callback(callback) {}
		virtual inline ~FnReadAsyncCallback() {}

		virtual void on_ref_zero() noexcept {
			peff::destroy_and_release<decltype(*this)>(_allocator, this, alignof(decltype(*this)));
		}

		virtual ExceptionPointer on_status_changed(ReadAsyncTask* task) override {
			return _callback(task);
		}
	};

	template <typename Callback>
	class FnWriteAsyncCallback final : public WriteAsyncCallback {
	private:
		peff::Alloc *_allocator;
		Callback _callback;

	public:
		static_assert(std::is_invocable_v<Callback, WriteAsyncTask *>, "The callback is malformed");

		NETKNOT_FORCEINLINE FnWriteAsyncCallback(peff::Alloc *allocator, Callback &&callback) : _allocator(allocator), _callback(callback) {}
		virtual inline ~FnWriteAsyncCallback() {}

		virtual void on_ref_zero() noexcept {
			peff::destroy_and_release<decltype(*this)>(_allocator, this, alignof(decltype(*this)));
		}

		virtual ExceptionPointer on_status_changed(WriteAsyncTask *task) override {
			return _callback(task);
		}
	};

	template <typename Callback>
	class FnAcceptAsyncCallback final : public AcceptAsyncCallback {
	private:
		peff::Alloc *_allocator;
		Callback _callback;

	public:
		static_assert(std::is_invocable_v<Callback, Socket *>, "The callback is malformed");

		NETKNOT_FORCEINLINE FnAcceptAsyncCallback(peff::Alloc *allocator, Callback &&callback) : _allocator(allocator), _callback(callback) {}
		virtual inline ~FnAcceptAsyncCallback() {}

		virtual void on_ref_zero() noexcept {
			peff::destroy_and_release<decltype(*this)>(_allocator, this, alignof(decltype(*this)));
		}

		virtual ExceptionPointer on_accepted(Socket *socket) override {
			return _callback(socket);
		}
	};

	class Socket {
	public:
		NETKNOT_API Socket();
		NETKNOT_API virtual ~Socket();

		virtual void dealloc() noexcept = 0;

		virtual void close() = 0;

		virtual ExceptionPointer bind(const TranslatedAddress *address) = 0;
		virtual ExceptionPointer listen(size_t backlog) = 0;
		virtual ExceptionPointer connect(const TranslatedAddress *address) = 0;

		virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) = 0;
		virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) = 0;
		virtual ExceptionPointer accept(peff::Alloc *allocator, Socket *&socketOut) = 0;

		virtual ExceptionPointer read_async(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&async_task_out) = 0;
		virtual ExceptionPointer write_async(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&async_task_out) = 0;
		virtual ExceptionPointer accept_async(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&async_task_out) = 0;
	};
}

#endif

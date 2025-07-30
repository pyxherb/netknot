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
		peff::UUID addressFamily;

		NETKNOT_FORCEINLINE Address(const peff::UUID &addressFamily) : addressFamily(addressFamily) {}
	};

	struct IPv4Address : public Address {
		uint8_t a = 0, b = 0, c = 0, d = 0;
		uint16_t port = 0;

		NETKNOT_FORCEINLINE IPv4Address() : Address(ADDRFAM_IPV4) {}
		NETKNOT_FORCEINLINE IPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : Address(ADDRFAM_IPV4), a(a), b(b), c(c), d(d) {}
		NETKNOT_FORCEINLINE IPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) : Address(ADDRFAM_IPV4), a(a), b(b), c(c), d(d), port(port) {}
	};

	class CompiledAddress {
	public:
		NETKNOT_API CompiledAddress();
		NETKNOT_API virtual ~CompiledAddress();

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
		std::atomic_size_t _refCount = 0;
		AsyncTaskType _taskType;

	public:
		NETKNOT_API AsyncTask(AsyncTaskType taskType);
		NETKNOT_API ~AsyncTask();

		virtual void onRefZero() noexcept = 0;

		NETKNOT_FORCEINLINE size_t incRef(size_t globalRc) noexcept {
			return ++_refCount;
		}

		NETKNOT_FORCEINLINE size_t decRef(size_t globalRc) noexcept {
			if (!--_refCount) {
				onRefZero();
				return 0;
			}

			return _refCount;
		}

		virtual AsyncTaskStatus getStatus() = 0;
		virtual ExceptionPointer &getException() = 0;

		NETKNOT_FORCEINLINE AsyncTaskType getTaskType() const noexcept {
			return _taskType;
		}
	};

	struct AsyncTaskDeleter {
		NETKNOT_FORCEINLINE void operator()(AsyncTask* task) const noexcept {
			task->onRefZero();
		}
	};

	class ReadAsyncTask : public AsyncTask {
	public:
		NETKNOT_API ReadAsyncTask();
		NETKNOT_API virtual ~ReadAsyncTask();

		virtual size_t getCurrentReadSize() = 0;
		virtual size_t getExpectedReadSize() = 0;
	};

	class WriteAsyncTask : public AsyncTask {
	public:
		NETKNOT_API WriteAsyncTask();
		NETKNOT_API virtual ~WriteAsyncTask();

		virtual size_t getCurrentWrittenSize() = 0;
		virtual size_t getExpectedWrittenSize() = 0;
	};

	class AcceptAsyncTask : public AsyncTask {
	public:
		NETKNOT_API AcceptAsyncTask();
		NETKNOT_API virtual ~AcceptAsyncTask();

		virtual Address &getAcceptedAddress() = 0;
	};

	class ReadAsyncCallback {
	public:
		NETKNOT_API ReadAsyncCallback();
		NETKNOT_API virtual ~ReadAsyncCallback();

		virtual void dealloc() noexcept = 0;

		virtual ExceptionPointer onStatusChanged(ReadAsyncTask *task) = 0;
	};

	class WriteAsyncCallback {
	public:
		NETKNOT_API WriteAsyncCallback();
		NETKNOT_API virtual ~WriteAsyncCallback();

		virtual void dealloc() noexcept = 0;

		virtual ExceptionPointer onStatusChanged(WriteAsyncTask *task) = 0;
	};

	class AcceptAsyncCallback {
	public:
		NETKNOT_API AcceptAsyncCallback();
		NETKNOT_API virtual ~AcceptAsyncCallback();

		virtual void dealloc() noexcept = 0;

		virtual ExceptionPointer onStatusChanged(WriteAsyncTask *task) = 0;
	};

	class Socket {
	public:
		NETKNOT_API Socket();
		NETKNOT_API virtual ~Socket();

		virtual void dealloc() noexcept = 0;

		virtual void close() = 0;

		virtual ExceptionPointer bind(const CompiledAddress *address) = 0;
		virtual ExceptionPointer listen(size_t backlog) = 0;
		virtual ExceptionPointer connect(const CompiledAddress *address) = 0;

		virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) = 0;
		virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) = 0;
		virtual ExceptionPointer accept(peff::Alloc *allocator, Socket *&socketOut) = 0;

		virtual ReadAsyncTask *readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback) = 0;
		virtual WriteAsyncTask *writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback) = 0;
		virtual AcceptAsyncTask *acceptAsync(peff::Alloc *allocator) = 0;
	};
}

#endif

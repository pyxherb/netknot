#ifndef _NETKNOT_WIN_SOCKET_H_
#define _NETKNOT_WIN_SOCKET_H_

#include "../socket.h"
#include <WinSock2.h>

namespace netknot {
	class Win32Socket;
	class Win32IOService;

	class Win32ReadAsyncTask : public ReadAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		RcBufferRef bufferRef;
		size_t szRead = 0;

		NETKNOT_API Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32ReadAsyncTask();

		NETKNOT_API virtual size_t getCurrentReadSize() override;
		NETKNOT_API virtual size_t getExpectedReadSize() override;
	};

	class Win32WriteAsyncTask : public WriteAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		RcBufferRef bufferRef;
		size_t szWritten = 0;

		NETKNOT_API Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32WriteAsyncTask();

		NETKNOT_API virtual size_t getCurrentWrittenSize() override;
		NETKNOT_API virtual size_t getExpectedWrittenSize() override;
	};

	class Win32AcceptAsyncTask : public AcceptAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		peff::UUID addressFamily;

		NETKNOT_API Win32AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const peff::UUID &addressFamily);
		NETKNOT_API virtual ~Win32AcceptAsyncTask();
	};

	class Win32IPv4AcceptAsyncTask : public Win32AcceptAsyncTask {
	public:
		IPv4Address convertedAddress;

		NETKNOT_API Win32IPv4AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket);
		NETKNOT_API virtual ~Win32IPv4AcceptAsyncTask();

		NETKNOT_API virtual Address &getAcceptedAddress() override;
	};

	class Win32Socket : public Socket {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		SOCKET socket;
		peff::UUID socketTypeId;

		NETKNOT_API Win32Socket(SOCKET socket, const peff::UUID &socketTypeId);
		NETKNOT_API virtual ~Win32Socket();

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual void close() override;

		NETKNOT_API virtual ExceptionPointer bind(const CompiledAddress *address) override;
		NETKNOT_API virtual ExceptionPointer listen(size_t backlog) override;
		NETKNOT_API virtual ExceptionPointer connect(const CompiledAddress *address) override;

		NETKNOT_API virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) override;
		NETKNOT_API virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) override;
		NETKNOT_API virtual ExceptionPointer accept(Address &addressOut) override;

		NETKNOT_API virtual ReadAsyncTask *readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback) override;
		NETKNOT_API virtual WriteAsyncTask *writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback) override;
		NETKNOT_API virtual AcceptAsyncTask *acceptAsync(peff::Alloc *allocator) override;
	};
}

#endif

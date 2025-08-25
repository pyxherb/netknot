#ifndef _NETKNOT_WIN_SOCKET_H_
#define _NETKNOT_WIN_SOCKET_H_

#include "../socket.h"
#include <WinSock2.h>
#include <MSWSock.h>
#include <peff/advutils/unique_ptr.h>

namespace netknot {
	class Win32Socket;
	class Win32IOService;
	struct Win32IOCPOverlapped;

	class Win32ReadAsyncTask : public ReadAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		RcBufferRef bufferRef;
		size_t szRead = 0;
		ExceptionPointer exceptPtr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::UniquePtr<ReadAsyncCallback, peff::DeallocableDeleter<ReadAsyncCallback>> callback;

		NETKNOT_API Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32ReadAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;

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
		ExceptionPointer exceptPtr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::UniquePtr<WriteAsyncCallback, peff::DeallocableDeleter<WriteAsyncCallback>> callback;

		NETKNOT_API Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32WriteAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;

		NETKNOT_API virtual size_t getCurrentWrittenSize() override;
		NETKNOT_API virtual size_t getExpectedWrittenSize() override;
	};

	class Win32AcceptAsyncTask : public AcceptAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		peff::UUID addressFamily;
		ExceptionPointer exceptPtr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::UniquePtr<AcceptAsyncCallback, peff::DeallocableDeleter<AcceptAsyncCallback>> callback;

		NETKNOT_API Win32AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const peff::UUID &addressFamily);
		NETKNOT_API virtual ~Win32AcceptAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;
	};

	class Win32Socket : public Socket {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		SOCKET socket = INVALID_SOCKET;
		peff::UUID socketTypeId;
		Win32IOService *ioService;
		peff::UUID addressFamily;

		NETKNOT_API Win32Socket(Win32IOService *ioService, const peff::UUID &addressFamily, const peff::UUID &socketTypeId);
		NETKNOT_API virtual ~Win32Socket();

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual void close() override;

		NETKNOT_API virtual ExceptionPointer bind(const CompiledAddress *address) override;
		NETKNOT_API virtual ExceptionPointer listen(size_t backlog) override;
		NETKNOT_API virtual ExceptionPointer connect(const CompiledAddress *address) override;

		NETKNOT_API virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) override;
		NETKNOT_API virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) override;
		NETKNOT_API virtual ExceptionPointer accept(peff::Alloc *allocator, Socket *&socketOut) override;

		NETKNOT_API virtual ExceptionPointer readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&asyncTaskOut) override;
		NETKNOT_API virtual ExceptionPointer writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&asyncTaskOut) override;
		NETKNOT_API virtual ExceptionPointer acceptAsync(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&asyncTaskOut) override;
	};
}

#endif

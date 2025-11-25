#ifndef _NETKNOT_WIN_SOCKET_H_
#define _NETKNOT_WIN_SOCKET_H_

#include "../socket.h"
#include <arpa/inet.h>
#include <peff/advutils/unique_ptr.h>
#include <peff/base/deallocable.h>
#include <unistd.h>

namespace netknot {
	class UnixSocket;
	class UnixIOService;

	class UnixReadAsyncTask : public ReadAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		RcBufferRef bufferRef;
		size_t szRead = 0;
		ExceptionPointer exceptPtr;
		peff::RcObjectPtr<ReadAsyncCallback> callback;

		NETKNOT_API UnixReadAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~UnixReadAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;

		NETKNOT_API virtual size_t getCurrentReadSize() override;
		NETKNOT_API virtual size_t getExpectedReadSize() override;

		NETKNOT_API virtual char *getBuffer() override;
		NETKNOT_API virtual RcBufferRef getBufferRef() override;
	};

	class UnixWriteAsyncTask : public WriteAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		RcBufferRef bufferRef;
		size_t szWritten = 0;
		ExceptionPointer exceptPtr;
		peff::RcObjectPtr<WriteAsyncCallback> callback;

		NETKNOT_API UnixWriteAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~UnixWriteAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;

		NETKNOT_API virtual size_t getCurrentWrittenSize() override;
		NETKNOT_API virtual size_t getExpectedWrittenSize() override;
	};

	class UnixAcceptAsyncTask : public AcceptAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		peff::UUID addressFamily;
		ExceptionPointer exceptPtr;
		peff::RcObjectPtr<AcceptAsyncCallback> callback;

		NETKNOT_API UnixAcceptAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const peff::UUID &addressFamily);
		NETKNOT_API virtual ~UnixAcceptAsyncTask();

		NETKNOT_API virtual void onRefZero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus getStatus() override;
		NETKNOT_API virtual ExceptionPointer &getException() override;
	};

	class UnixSocket : public Socket {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		int socket = -1;
		peff::UUID socketTypeId;
		UnixIOService *ioService;
		peff::UUID addressFamily;
		size_t backlog = 0;

		NETKNOT_API UnixSocket(UnixIOService *ioService, const peff::UUID &addressFamily, const peff::UUID &socketTypeId);
		NETKNOT_API virtual ~UnixSocket();

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual void close() override;

		NETKNOT_API virtual ExceptionPointer bind(const TranslatedAddress *address) override;
		NETKNOT_API virtual ExceptionPointer listen(size_t backlog) override;
		NETKNOT_API virtual ExceptionPointer connect(const TranslatedAddress *address) override;

		NETKNOT_API virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) override;
		NETKNOT_API virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) override;
		NETKNOT_API virtual ExceptionPointer accept(peff::Alloc *allocator, Socket *&socketOut) override;

		NETKNOT_API virtual ExceptionPointer readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&asyncTaskOut) override;
		NETKNOT_API virtual ExceptionPointer writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&asyncTaskOut) override;
		NETKNOT_API virtual ExceptionPointer acceptAsync(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&asyncTaskOut) override;
	};
}

#endif

#ifndef _NETKNOT_WIN_SOCKET_H_
#define _NETKNOT_WIN_SOCKET_H_

#include "../socket.h"
#include <WinSock2.h>
#include <MSWSock.h>
#include <peff/advutils/unique_ptr.h>
#include <peff/base/deallocable.h>

namespace netknot {
	class Win32Socket;
	class Win32IOService;
	struct Win32IOCPOverlapped;

	class Win32ReadAsyncTask : public ReadAsyncTask {
	public:
		std::mutex access_mutex;
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		RcBufferRef bufferRef;
		size_t sz_read = 0;
		ExceptionPointer except_ptr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::RcObjectPtr<ReadAsyncCallback> callback;

		NETKNOT_API Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32ReadAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;

		NETKNOT_API virtual size_t get_cur_read_size() override;
		NETKNOT_API virtual size_t get_expected_read_size() override;

		NETKNOT_API virtual char *get_buffer() override;
		NETKNOT_API virtual RcBufferRef get_buffer_ref() override;
	};

	class Win32WriteAsyncTask : public WriteAsyncTask {
	public:
		std::mutex access_mutex;
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		Win32Socket *socket;
		RcBufferRef bufferRef;
		size_t szWritten = 0;
		ExceptionPointer except_ptr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::RcObjectPtr<WriteAsyncCallback> callback;

		NETKNOT_API Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~Win32WriteAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;

		NETKNOT_API virtual size_t get_cur_written_size() override;
		NETKNOT_API virtual size_t get_expected_written_size() override;
	};

	class Win32AcceptAsyncTask : public AcceptAsyncTask {
	public:
		std::mutex access_mutex;
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		peff::UniquePtr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> socket;
		peff::UUID address_family;
		ExceptionPointer except_ptr;
		Win32IOCPOverlapped *overlapped = nullptr;
		peff::RcObjectPtr<AcceptAsyncCallback> callback;

		NETKNOT_API Win32AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const peff::UUID &address_family);
		NETKNOT_API virtual ~Win32AcceptAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;
	};

	class Win32Socket : public Socket {
	public:
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		SOCKET socket = INVALID_SOCKET;
		peff::UUID socketTypeId;
		Win32IOService *io_service;
		peff::UUID address_family;

		NETKNOT_API Win32Socket(Win32IOService *io_service, peff::Alloc *self_allocator, const peff::UUID &address_family, const peff::UUID &socketTypeId);
		NETKNOT_API virtual ~Win32Socket();

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual void close() override;

		NETKNOT_API virtual ExceptionPointer bind(const TranslatedAddress *address) override;
		NETKNOT_API virtual ExceptionPointer listen(size_t backlog) override;
		NETKNOT_API virtual ExceptionPointer connect(const TranslatedAddress *address) override;

		NETKNOT_API virtual ExceptionPointer read(char *buffer, size_t size, size_t &szReadOut) override;
		NETKNOT_API virtual ExceptionPointer write(const char *buffer, size_t size, size_t &szWrittenOut) override;
		NETKNOT_API virtual ExceptionPointer accept(peff::Alloc *allocator, Socket *&socketOut) override;

		NETKNOT_API virtual ExceptionPointer read_async(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&async_task_out) override;
		NETKNOT_API virtual ExceptionPointer write_async(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&async_task_out) override;
		NETKNOT_API virtual ExceptionPointer accept_async(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&async_task_out) override;
	};

	NETKNOT_API Win32IOCPOverlapped *alloc_overlapped(peff::Alloc *allocator, size_t addrSize, const RcBufferRef &buffer, AsyncTask *async_task);
	NETKNOT_API void release_overlapped(peff::Alloc *allocator, Win32IOCPOverlapped *overlapped);
}

#endif

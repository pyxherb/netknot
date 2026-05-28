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
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		RcBufferRef bufferRef;
		size_t sz_read = 0;
		ExceptionPointer except_ptr;
		peff::RcObjectPtr<ReadAsyncCallback> callback;

		NETKNOT_API UnixReadAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~UnixReadAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;

		NETKNOT_API virtual size_t get_cur_read_size() override;
		NETKNOT_API virtual size_t get_expected_read_size() override;

		NETKNOT_API virtual char *get_buffer() override;
		NETKNOT_API virtual RcBufferRef get_buffer_ref() override;
	};

	class UnixWriteAsyncTask : public WriteAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		RcBufferRef bufferRef;
		size_t szWritten = 0;
		ExceptionPointer except_ptr;
		peff::RcObjectPtr<WriteAsyncCallback> callback;

		NETKNOT_API UnixWriteAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef);
		NETKNOT_API virtual ~UnixWriteAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;

		NETKNOT_API virtual size_t get_cur_written_size() override;
		NETKNOT_API virtual size_t get_expected_written_size() override;
	};

	class UnixAcceptAsyncTask : public AcceptAsyncTask {
	public:
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		AsyncTaskStatus status = AsyncTaskStatus::Ready;
		UnixSocket *socket;
		peff::UUID address_family;
		ExceptionPointer except_ptr;
		peff::RcObjectPtr<AcceptAsyncCallback> callback;

		NETKNOT_API UnixAcceptAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const peff::UUID &address_family);
		NETKNOT_API virtual ~UnixAcceptAsyncTask();

		NETKNOT_API virtual void on_ref_zero() noexcept override;

		NETKNOT_API virtual AsyncTaskStatus get_status() override;
		NETKNOT_API virtual ExceptionPointer &get_except() override;
	};

	class UnixSocket : public Socket {
	public:
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		int socket = -1;
		peff::UUID socketTypeId;
		UnixIOService *io_service;
		peff::UUID address_family;
		size_t backlog = 0;

		NETKNOT_API UnixSocket(UnixIOService *io_service, const peff::UUID &address_family, const peff::UUID &socketTypeId);
		NETKNOT_API virtual ~UnixSocket();

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
}

#endif

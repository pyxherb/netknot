#ifndef _NETKNOT_WIN_IO_SERVICE_H_
#define _NETKNOT_WIN_IO_SERVICE_H_

#include "socket.h"
#include "../io_service.h"
#include <peff/base/deallocable.h>
#include <peff/containers/dynarray.h>
#include <peff/containers/map.h>
#include <peff/advutils/buffer_alloc.h>
#include <Windows.h>

namespace netknot {
	class Win32TranslatedAddress : public TranslatedAddress {
	public:
		peff::RcObjectPtr<peff::Alloc> self_allocator;
		char *data = nullptr;
		size_t size = 0;

		NETKNOT_API Win32TranslatedAddress(peff::Alloc *self_allocator);
		NETKNOT_API virtual ~Win32TranslatedAddress();

		NETKNOT_API virtual void dealloc() noexcept override;
	};

	struct Win32IOCPOverlapped : public OVERLAPPED {
		WSABUF buf;
		size_t addrSize;
		AsyncTask *async_task;
		RcBuffer *rcBuffer;
		DWORD szOperated;
		DWORD flags;
	};

	class Win32IOService : public IOService {
	private:
		bool _isRunning = false;

	public:
		NETKNOT_API static DWORD WINAPI _worker_thread_proc(LPVOID lpThreadParameter);

		struct ThreadLocalData {
			Win32IOService *io_service;
			HANDLE hThread = INVALID_HANDLE_VALUE;
			size_t threadId;
			bool terminate = false;
			ExceptionPointer exceptionStorage;

			NETKNOT_FORCEINLINE ThreadLocalData(ThreadLocalData &&) = default;
			NETKNOT_FORCEINLINE ThreadLocalData(Win32IOService *io_service, size_t threadId, peff::Alloc *allocator) : io_service(io_service), threadId(threadId) {
			}
			NETKNOT_API ~ThreadLocalData();
		};

		CRITICAL_SECTION terminateNotifyCriticalSection;
		CONDITION_VARIABLE terminateNotifyConditionVar;

		std::mutex cur_tasks_mutex;
		peff::Set<peff::RcObjectPtr<AsyncTask>> cur_tasks;

		peff::RcObjectPtr<peff::Alloc> self_allocator;
		HANDLE iocpCompletionPort = INVALID_HANDLE_VALUE;

		peff::DynArray<ThreadLocalData> threadLocalData;

		NETKNOT_API Win32IOService(peff::Alloc *self_allocator);
		NETKNOT_API ~Win32IOService();

		NETKNOT_API static Win32IOService *alloc(peff::Alloc *self_allocator);

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual ExceptionPointer run() override;
		NETKNOT_API virtual ExceptionPointer stop() override;

		NETKNOT_API virtual ExceptionPointer post_async_task(AsyncTask *task) noexcept override;

		NETKNOT_API virtual ExceptionPointer create_socket(peff::Alloc *allocator, const peff::UUID &address_family, const peff::UUID &socketType, Socket *&socketOut) noexcept override;

		NETKNOT_API virtual ExceptionPointer translate_addr(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut = nullptr) noexcept override;
		NETKNOT_API virtual ExceptionPointer detranslate_addr(peff::Alloc *allocator, const peff::UUID &address_family, const TranslatedAddress *address, Address &addressOut) noexcept override;
	};

	NETKNOT_API ExceptionPointer lastErrorToExcept(peff::Alloc *allocator, DWORD errorCode) noexcept;
	NETKNOT_API ExceptionPointer wsaLastErrorToExcept(peff::Alloc *allocator, DWORD errorCode) noexcept;
	NETKNOT_API ExceptionPointer createIOCPIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept;
}

#endif

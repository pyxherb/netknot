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
	class Win32CompiledAddress : public CompiledAddress {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		char *data = nullptr;
		size_t size = 0;

		NETKNOT_API Win32CompiledAddress(peff::Alloc *selfAllocator);
		NETKNOT_API virtual ~Win32CompiledAddress();

		NETKNOT_API virtual void dealloc() noexcept override;
	};

	struct Win32IOCPOverlapped : public OVERLAPPED {
		WSABUF buf;
		size_t addrSize;
		AsyncTask *asyncTask;
		DWORD szOperated;
		DWORD flags;
	};

	class Win32IOService : public IOService {
	private:
		bool _isRunning = false;

	public:
		NETKNOT_API static DWORD WINAPI _workerThreadProc(LPVOID lpThreadParameter);

		struct ThreadLocalData {
			Win32IOService *ioService;
			HANDLE hThread = INVALID_HANDLE_VALUE;
			size_t threadId;
			peff::Set<peff::RcObjectPtr<AsyncTask>> currentTasks;
			bool terminate = false;
			ExceptionPointer exceptionStorage;

			NETKNOT_FORCEINLINE ThreadLocalData(ThreadLocalData &&) = default;
			NETKNOT_FORCEINLINE ThreadLocalData(Win32IOService *ioService, size_t threadId, peff::Alloc *allocator) : ioService(ioService), threadId(threadId), currentTasks(allocator) {
			}
			NETKNOT_API ~ThreadLocalData();
		};

		CRITICAL_SECTION terminateNotifyCriticalSection;
		CONDITION_VARIABLE terminateNotifyConditionVar;

		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		HANDLE iocpCompletionPort = INVALID_HANDLE_VALUE;

		peff::DynArray<ThreadLocalData> threadLocalData;
		size_t nCurrentTasksOfMostFreeThread;

		size_t szSortedThreadIndicesBuffer = 0, alignSortedThreadIndicesBuffer = 0;
		char *sortedThreadIndicesBuffer = nullptr;
		peff::BufferAlloc sortedThreadIndicesAlloc;

		size_t szSortedThreadSetBuffer = 0, alignSortedThreadSetBuffer = 0;
		char *sortedThreadSetBuffer = nullptr;
		peff::BufferAlloc sortedThreadSetAlloc;

		peff::Map<size_t, peff::Set<size_t>> sortedThreadIndices;
		CRITICAL_SECTION threadResortCriticalSection;

		NETKNOT_API Win32IOService(peff::Alloc *selfAllocator);
		NETKNOT_API ~Win32IOService();

		NETKNOT_API static Win32IOService *alloc(peff::Alloc *selfAllocator);

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual ExceptionPointer run() override;
		NETKNOT_API virtual ExceptionPointer stop() override;

		NETKNOT_API virtual ExceptionPointer postAsyncTask(AsyncTask *task) noexcept override;

		NETKNOT_API virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept override;

		NETKNOT_API virtual ExceptionPointer compileAddress(peff::Alloc *allocator, const Address *address, CompiledAddress **compiledAddressOut, size_t *compiledAddressSizeOut = nullptr) noexcept override;
		NETKNOT_API virtual ExceptionPointer decompileAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const CompiledAddress *address, Address &addressOut) noexcept override;

		NETKNOT_API void addIntoOrInsertNewSortedThreadGroup(size_t load, size_t index);
		NETKNOT_API void sortThreadsByLoad();
	};
}

#endif

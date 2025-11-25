#ifndef _NETKNOT_UNIX_IO_SERVICE_H_
#define _NETKNOT_UNIX_IO_SERVICE_H_

#include "socket.h"
#include "../io_service.h"
#include <peff/base/deallocable.h>
#include <peff/containers/dynarray.h>
#include <peff/containers/map.h>
#include <peff/advutils/buffer_alloc.h>
#include <pthread.h>
#include <poll.h>

namespace netknot {
	class UnixTranslatedAddress : public TranslatedAddress {
	public:
		peff::RcObjectPtr<peff::Alloc> selfAllocator;
		char *data = nullptr;
		size_t size = 0;

		NETKNOT_API UnixTranslatedAddress(peff::Alloc *selfAllocator);
		NETKNOT_API virtual ~UnixTranslatedAddress();

		NETKNOT_API virtual void dealloc() noexcept override;
	};

	class UnixIOService : public IOService {
	private:
		bool _isRunning = false;

	public:
		NETKNOT_API static void *_workerThreadProc(void *lpThreadParameter);

		struct ThreadLocalData {
			UnixIOService *ioService;
			peff::Option<pthread_t> hThread;
			pthread_cond_t startCond = PTHREAD_COND_INITIALIZER;
			pthread_mutex_t startMutex = PTHREAD_MUTEX_INITIALIZER;
			size_t threadId;
			bool terminate = false;
			ExceptionPointer exceptionStorage;

			NETKNOT_FORCEINLINE ThreadLocalData(ThreadLocalData &&) = default;
			NETKNOT_FORCEINLINE ThreadLocalData(UnixIOService *ioService, size_t threadId, peff::Alloc *allocator) : ioService(ioService), threadId(threadId) {
			}
			NETKNOT_API ~ThreadLocalData();
		};

		pthread_mutex_t terminateNotifyMutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_cond_t terminateNotifyConditionVar = PTHREAD_COND_INITIALIZER;

		pthread_mutex_t currentTasksMutex = PTHREAD_MUTEX_INITIALIZER;
		peff::Set<peff::RcObjectPtr<AsyncTask>> currentTasks;

		peff::RcObjectPtr<peff::Alloc> selfAllocator;

		peff::DynArray<ThreadLocalData> threadLocalData;

		NETKNOT_API UnixIOService(peff::Alloc *selfAllocator);
		NETKNOT_API ~UnixIOService();

		NETKNOT_API static UnixIOService *alloc(peff::Alloc *selfAllocator);

		NETKNOT_API virtual void dealloc() noexcept override;

		NETKNOT_API virtual ExceptionPointer run() override;
		NETKNOT_API virtual ExceptionPointer stop() override;

		NETKNOT_API virtual ExceptionPointer postAsyncTask(AsyncTask *task) noexcept override;

		NETKNOT_API virtual ExceptionPointer createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept override;

		NETKNOT_API virtual ExceptionPointer translateAddress(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut = nullptr) noexcept override;
		NETKNOT_API virtual ExceptionPointer detranslateAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const TranslatedAddress *address, Address &addressOut) noexcept override;
	};
}

#endif

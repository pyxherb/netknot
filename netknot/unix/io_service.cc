#include "io_service.h"

using namespace netknot;

NETKNOT_API UnixTranslatedAddress::UnixTranslatedAddress(peff::Alloc *selfAllocator) : selfAllocator(selfAllocator) {
}

NETKNOT_API UnixTranslatedAddress::~UnixTranslatedAddress() {
	if (data) {
		selfAllocator->release(data, size, 1);
	}
}

NETKNOT_API void UnixTranslatedAddress::dealloc() noexcept {
	peff::destroyAndRelease<UnixTranslatedAddress>(selfAllocator.get(), this, alignof(UnixTranslatedAddress));
}

NETKNOT_API void *UnixIOService::_workerThreadProc(void *lpThreadParameter) {
	ThreadLocalData *tld = (ThreadLocalData *)lpThreadParameter;

	pthread_cond_wait(&tld->startCond, &tld->startMutex);

	while (true) {
		// TODO: Implement it.
	}

	pthread_cond_broadcast(&tld->ioService->terminateNotifyConditionVar);
	return 0;
}

NETKNOT_API UnixIOService::ThreadLocalData::~ThreadLocalData() {
	if (hThread.hasValue()) {
		terminate = true;
		pthread_join(hThread, nullptr);
	}
}

NETKNOT_API UnixIOService::UnixIOService(peff::Alloc *selfAllocator)
	: selfAllocator(selfAllocator),
	  threadLocalData(selfAllocator),
	  currentTasks(selfAllocator) {
}

NETKNOT_API UnixIOService::~UnixIOService() {
}

NETKNOT_API UnixIOService *UnixIOService::alloc(peff::Alloc *selfAllocator) {
	std::unique_ptr<UnixIOService, peff::DeallocableDeleter<UnixIOService>> p(
		peff::allocAndConstruct<UnixIOService>(selfAllocator, alignof(UnixIOService), selfAllocator));

	if (!p)
		return nullptr;

	return p.release();
}

NETKNOT_API void UnixIOService::dealloc() noexcept {
}

NETKNOT_API ExceptionPointer UnixIOService::run() {
	if (_isRunning)
		std::terminate();

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	for (auto &i : threadLocalData) {
		pthread_cond_broadcast(&i.startCond);
	}

	pthread_cond_wait(&terminateNotifyConditionVar, &terminateNotifyMutex);

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	_isRunning = true;

	return {};
}

NETKNOT_API ExceptionPointer UnixIOService::stop() {
	if (!_isRunning)
		std::terminate();

	for (auto &i : threadLocalData) {
		i.terminate = true;
	}

	for (auto &i : threadLocalData) {
		pthread_join(i.hThread, nullptr);
	}

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	_isRunning = false;

	return {};
}

NETKNOT_API ExceptionPointer UnixIOService::postAsyncTask(AsyncTask *task) noexcept {
	peff::ScopeGuard currentTasksMutexGuard([this]() noexcept {
		pthread_mutex_unlock(&currentTasksMutex);
	});

	pthread_mutex_lock(&currentTasksMutex);

	if (!currentTasks.insert(task))
		return OutOfMemoryError::alloc();

	return {};
}

NETKNOT_API ExceptionPointer UnixIOService::createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept {
	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> p(
		peff::allocAndConstruct<UnixSocket>(allocator, alignof(UnixSocket), this, addressFamily, socketType));

	if (!p)
		return OutOfMemoryError::alloc();

	int &s = p->socket;

	int af;
	if (addressFamily == ADDRFAM_IPV4) {
		af = AF_INET;
	} else if (addressFamily == ADDRFAM_IPV6) {
		af = AF_INET6;
	} else {
		std::terminate();
	}

	if (socketType == SOCKET_TCP) {
		s = socket(af, SOCK_STREAM, IPPROTO_TCP);
	} else if (socketType == SOCKET_UDP) {
		s = socket(af, SOCK_DGRAM, IPPROTO_UDP);
	} else {
		std::terminate();
	}

	if (s < 0) {
		std::terminate();
	}

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer UnixIOService::translateAddress(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut) noexcept {
	if (address->addressFamily == ADDRFAM_IPV4) {
		sockaddr_in sa = { 0 };

		if (!compiledAddressOut) {
			*compiledAddressSizeOut = sizeof(sa);
			return {};
		} else {
			if (address) {
				const IPv4Address *addr = (const IPv4Address *)address;

				sa.sin_family = AF_INET;
				sa.sin_addr.s_addr = ((addr->d << 24) | (addr->c << 16) | (addr->b << 8) | (addr->a));
				sa.sin_port = htons(addr->port);
			}

			std::unique_ptr<UnixTranslatedAddress, peff::DeallocableDeleter<UnixTranslatedAddress>>
				compiledAddress(peff::allocAndConstruct<UnixTranslatedAddress>(allocator, alignof(UnixTranslatedAddress), allocator));

			if (!compiledAddress)
				return OutOfMemoryError::alloc();

			if (!(compiledAddress->data = (char *)allocator->alloc(sizeof(sa), 1))) {
				return OutOfMemoryError::alloc();
			}

			compiledAddress->size = sizeof(sa);
			memcpy(compiledAddress->data, &sa, sizeof(sa));

			*compiledAddressOut = compiledAddress.release();
		}

		return {};
	} else if (address->addressFamily == ADDRFAM_IPV6) {
	}

	std::terminate();
}

ExceptionPointer UnixIOService::detranslateAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const TranslatedAddress *address, Address &addressOut) noexcept {
	if (addressFamily == ADDRFAM_IPV4) {
		sockaddr_in *sa = (sockaddr_in *)((UnixTranslatedAddress *)address)->data;

		IPv4Address &ipv4Address = (IPv4Address &)addressOut;

		ipv4Address.a = sa->sin_addr.s_addr >> 24;
		ipv4Address.b = (sa->sin_addr.s_addr >> 16) & 0xff;
		ipv4Address.c = (sa->sin_addr.s_addr >> 8) & 0xff;
		ipv4Address.d = sa->sin_addr.s_addr & 0xff;

		ipv4Address.port = sa->sin_port;

		return {};
	} else if (addressFamily == ADDRFAM_IPV6) {
	}

	std::terminate();
}

NETKNOT_API ExceptionPointer netknot::createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	std::unique_ptr<UnixIOService, peff::DeallocableDeleter<UnixIOService>> ioService(UnixIOService::alloc(params.allocator.get()));

	if (!ioService)
		return OutOfMemoryError::alloc();

	if (!ioService->threadLocalData.resizeUninitialized(params.nWorkerThreads)) {
		return OutOfMemoryError::alloc();
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		peff::constructAt(&ioService->threadLocalData.at(i), ioService.get(), i, params.allocator.get());
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		UnixIOService::ThreadLocalData &tld = ioService->threadLocalData.at(i);

		peff::ScopeGuard removeThreadHandleGuard([&tld]() noexcept {
			tld.hThread.reset();
		});

		tld.hThread = 0;
		if (pthread_create(&tld.hThread.value(), nullptr, UnixIOService::_workerThreadProc, &tld) < 0) {
			switch (errno) {
				case EAGAIN:
					std::terminate();
					break;
				case EINVAL:
					std::terminate();
					break;
				case ENOMEM:
					std::terminate();
					break;
				default:
					std::terminate();
					break;
			}
		}

		removeThreadHandleGuard.release();
	}

	ioServiceOut = ioService.release();

	return {};
}

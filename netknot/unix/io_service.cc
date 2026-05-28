#include "io_service.h"

using namespace netknot;

NETKNOT_API UnixTranslatedAddress::UnixTranslatedAddress(peff::Alloc *self_allocator) : self_allocator(self_allocator) {
}

NETKNOT_API UnixTranslatedAddress::~UnixTranslatedAddress() {
	if (data) {
		self_allocator->release(data, size, 1);
	}
}

NETKNOT_API void UnixTranslatedAddress::dealloc() noexcept {
	peff::destroy_and_release<UnixTranslatedAddress>(self_allocator.get(), this, alignof(UnixTranslatedAddress));
}

NETKNOT_API void *UnixIOService::_workerThreadProc(void *lpThreadParameter) {
	ThreadLocalData *tld = (ThreadLocalData *)lpThreadParameter;

	pthread_cond_wait(&tld->startCond, &tld->startMutex);

	while (true) {
		// TODO: Implement it.
	}

	pthread_cond_broadcast(&tld->io_service->terminateNotifyConditionVar);
	return 0;
}

NETKNOT_API UnixIOService::ThreadLocalData::~ThreadLocalData() {
	if (hThread.has_value()) {
		terminate = true;
		pthread_join(hThread, nullptr);
	}
}

NETKNOT_API UnixIOService::UnixIOService(peff::Alloc *self_allocator)
	: self_allocator(self_allocator),
	  threadLocalData(self_allocator),
	  cur_tasks(self_allocator) {
}

NETKNOT_API UnixIOService::~UnixIOService() {
}

NETKNOT_API UnixIOService *UnixIOService::alloc(peff::Alloc *self_allocator) {
	std::unique_ptr<UnixIOService, peff::DeallocableDeleter<UnixIOService>> p(
		peff::alloc_and_construct<UnixIOService>(self_allocator, alignof(UnixIOService), self_allocator));

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
	peff::ScopeGuard cur_tasks_mutex_guard([this]() noexcept {
		pthread_mutex_unlock(&cur_tasks_mutex);
	});

	pthread_mutex_lock(&cur_tasks_mutex);

	if (!cur_tasks.insert(task))
		return OutOfMemoryError::alloc();

	return {};
}

NETKNOT_API ExceptionPointer UnixIOService::createSocket(peff::Alloc *allocator, const peff::UUID &address_family, const peff::UUID &socketType, Socket *&socketOut) noexcept {
	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> p(
		peff::alloc_and_construct<UnixSocket>(allocator, alignof(UnixSocket), this, address_family, socketType));

	if (!p)
		return OutOfMemoryError::alloc();

	int &s = p->socket;

	int af;
	if (address_family == ADDRFAM_IPV4) {
		af = AF_INET;
	} else if (address_family == ADDRFAM_IPV6) {
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
	if (address->address_family == ADDRFAM_IPV4) {
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
				compiledAddress(peff::alloc_and_construct<UnixTranslatedAddress>(allocator, alignof(UnixTranslatedAddress), allocator));

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
	} else if (address->address_family == ADDRFAM_IPV6) {
	}

	std::terminate();
}

ExceptionPointer UnixIOService::detranslateAddress(peff::Alloc *allocator, const peff::UUID &address_family, const TranslatedAddress *address, Address &addressOut) noexcept {
	if (address_family == ADDRFAM_IPV4) {
		sockaddr_in *sa = (sockaddr_in *)((UnixTranslatedAddress *)address)->data;

		IPv4Address &ipv4Address = (IPv4Address &)addressOut;

		ipv4Address.a = sa->sin_addr.s_addr >> 24;
		ipv4Address.b = (sa->sin_addr.s_addr >> 16) & 0xff;
		ipv4Address.c = (sa->sin_addr.s_addr >> 8) & 0xff;
		ipv4Address.d = sa->sin_addr.s_addr & 0xff;

		ipv4Address.port = sa->sin_port;

		return {};
	} else if (address_family == ADDRFAM_IPV6) {
	}

	std::terminate();
}

NETKNOT_API ExceptionPointer netknot::createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	std::unique_ptr<UnixIOService, peff::DeallocableDeleter<UnixIOService>> io_service(UnixIOService::alloc(params.allocator.get()));

	if (!io_service)
		return OutOfMemoryError::alloc();

	if (!io_service->threadLocalData.resize_uninit(params.nWorkerThreads)) {
		return OutOfMemoryError::alloc();
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		peff::construct_at(&io_service->threadLocalData.at(i), io_service.get(), i, params.allocator.get());
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		UnixIOService::ThreadLocalData &tld = io_service->threadLocalData.at(i);

		peff::ScopeGuard remove_thread_handle_guard([&tld]() noexcept {
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

		remove_thread_handle_guard.release();
	}

	ioServiceOut = io_service.release();

	return {};
}

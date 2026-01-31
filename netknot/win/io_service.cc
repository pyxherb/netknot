#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32TranslatedAddress::Win32TranslatedAddress(peff::Alloc *selfAllocator) : selfAllocator(selfAllocator) {
}

NETKNOT_API Win32TranslatedAddress::~Win32TranslatedAddress() {
	if (data) {
		selfAllocator->release(data, size, 1);
	}
}

NETKNOT_API void Win32TranslatedAddress::dealloc() noexcept {
	peff::destroyAndRelease<Win32TranslatedAddress>(selfAllocator.get(), this, alignof(Win32TranslatedAddress));
}

NETKNOT_API DWORD WINAPI Win32IOService::_workerThreadProc(LPVOID lpThreadParameter) {
	ThreadLocalData *tld = (ThreadLocalData *)lpThreadParameter;

	while (true) {
		DWORD szTransferred;
		ULONG_PTR key;
		LPOVERLAPPED ov;

		if (!GetQueuedCompletionStatus(tld->ioService->iocpCompletionPort, &szTransferred, &key, &ov, INFINITE)) {
			DWORD e = WSAGetLastError();
			tld->exceptionStorage = wsaLastErrorToExcept(tld->ioService->selfAllocator.get(), e);
			return e;
		}

		if (!key)
			break;

		Win32IOCPOverlapped *iocpOverlapped = (Win32IOCPOverlapped *)ov;

		switch (iocpOverlapped->asyncTask->getTaskType()) {
			case AsyncTaskType::Read: {
				Win32ReadAsyncTask *task = (Win32ReadAsyncTask *)iocpOverlapped->asyncTask;

				task->szRead += szTransferred;
				task->status = AsyncTaskStatus::Done;

				if ((tld->exceptionStorage = task->callback->onStatusChanged(task))) {
					WakeAllConditionVariable(&tld->ioService->terminateNotifyConditionVar);
					return -1;
				}

				tld->ioService->currentTasks.remove(task);

				break;
			}
			case AsyncTaskType::Write: {
				Win32WriteAsyncTask *task = (Win32WriteAsyncTask *)iocpOverlapped->asyncTask;

				task->szWritten += szTransferred;
				task->status = AsyncTaskStatus::Done;

				if ((tld->exceptionStorage = task->callback->onStatusChanged(task))) {
					WakeAllConditionVariable(&tld->ioService->terminateNotifyConditionVar);
					return -1;
				}

				tld->ioService->currentTasks.remove(task);
				break;
			}
			case AsyncTaskType::Accept: {
				Win32AcceptAsyncTask *task = (Win32AcceptAsyncTask *)iocpOverlapped->asyncTask;

				task->status = AsyncTaskStatus::Done;

				if ((tld->exceptionStorage = task->callback->onAccepted(task->socket))) {
					WakeAllConditionVariable(&tld->ioService->terminateNotifyConditionVar);
					return -1;
				}

				tld->ioService->currentTasks.remove(task);
				break;
			}
		}
	}

	WakeAllConditionVariable(&tld->ioService->terminateNotifyConditionVar);
	return 0;
}

NETKNOT_API Win32IOService::ThreadLocalData::~ThreadLocalData() {
	if (hThread != INVALID_HANDLE_VALUE) {
		terminate = true;
		WaitForSingleObject(hThread, INFINITE);
	}
}

NETKNOT_API Win32IOService::Win32IOService(peff::Alloc *selfAllocator)
	: selfAllocator(selfAllocator),
	  threadLocalData(selfAllocator),
	  currentTasks(selfAllocator) {
}

NETKNOT_API Win32IOService::~Win32IOService() {
	DeleteCriticalSection(&terminateNotifyCriticalSection);

	WSACleanup();
}

NETKNOT_API Win32IOService *Win32IOService::alloc(peff::Alloc *selfAllocator) {
	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> p(
		peff::allocAndConstruct<Win32IOService>(selfAllocator, alignof(Win32IOService), selfAllocator));

	if (!p)
		return nullptr;

	return p.release();
}

NETKNOT_API void Win32IOService::dealloc() noexcept {
}

NETKNOT_API ExceptionPointer Win32IOService::run() {
	if (_isRunning)
		std::terminate();

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	for (auto &i : threadLocalData) {
		ResumeThread(i.hThread);
	}

	SleepConditionVariableCS(&terminateNotifyConditionVar, &terminateNotifyCriticalSection, INFINITE);

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	_isRunning = true;

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::stop() {
	if (!_isRunning)
		std::terminate();

	for (auto &i : threadLocalData) {
		PostQueuedCompletionStatus(iocpCompletionPort, 0, 0, nullptr);
	}

	for (auto &i : threadLocalData) {
		WaitForSingleObject(i.hThread, INFINITE);
	}

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	_isRunning = false;

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::postAsyncTask(AsyncTask *task) noexcept {
	peff::ScopeGuard currentTasksCriticalSectionGuard([this]() noexcept {
		LeaveCriticalSection(&currentTasksCriticalSection);
	});

	EnterCriticalSection(&currentTasksCriticalSection);

	if (!currentTasks.insert(task))
		return OutOfMemoryError::alloc();

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept {
	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::allocAndConstruct<Win32Socket>(allocator, alignof(Win32Socket), this, addressFamily, socketType));

	if (!p)
		return OutOfMemoryError::alloc();

	SOCKET &s = p->socket;

	int af;
	if (addressFamily == ADDRFAM_IPV4) {
		af = AF_INET;
	} else if (addressFamily == ADDRFAM_IPV6) {
		af = AF_INET6;
	} else {
		// Unhandled address family.
		std::terminate();
	}

	if (socketType == SOCKET_TCP) {
		s = WSASocket(af, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	} else if (socketType == SOCKET_UDP) {
		s = WSASocket(af, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	} else {
		// Unhandled address family.
		std::terminate();
	}

	if (s == INVALID_SOCKET) {
		std::terminate();
	}

	HANDLE hPort = CreateIoCompletionPort(
		(HANDLE)p->socket,
		iocpCompletionPort,
		(ULONG_PTR)this,
		0);
	if (!hPort) {
		std::terminate();
	}

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::translateAddress(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut) noexcept {
	if (address->addressFamily == ADDRFAM_IPV4) {
		SOCKADDR_IN sa = { 0 };

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

			std::unique_ptr<Win32TranslatedAddress, peff::DeallocableDeleter<Win32TranslatedAddress>>
				compiledAddress(peff::allocAndConstruct<Win32TranslatedAddress>(allocator, alignof(Win32TranslatedAddress), allocator));

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

ExceptionPointer Win32IOService::detranslateAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const TranslatedAddress *address, Address &addressOut) noexcept {
	if (addressFamily == ADDRFAM_IPV4) {
		sockaddr_in *sa = (sockaddr_in *)((Win32TranslatedAddress *)address)->data;

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

NETKNOT_API ExceptionPointer netknot::lastErrorToExcept(peff::Alloc* allocator, DWORD errorCode) noexcept {
	switch (errorCode) {
		case ERROR_OUTOFMEMORY:
			return OutOfMemoryError::alloc();
		default:
			break;
	}
	std::terminate();
}

NETKNOT_API ExceptionPointer netknot::wsaLastErrorToExcept(peff::Alloc* allocator, DWORD errorCode) noexcept {
	switch (errorCode) {
		case WSA_NOT_ENOUGH_MEMORY:
			return OutOfMemoryError::alloc();
		case WSAEADDRINUSE:
			return NetworkError::alloc(allocator, NetworkErrorCode::AddressInUse);
		case WSAEACCES:
			return NetworkError::alloc(allocator, NetworkErrorCode::AccessDenied);
		case WSAEMFILE:
			return NetworkError::alloc(allocator, NetworkErrorCode::TooManyOpenedFiles);
		case WSAEMSGSIZE:
			return NetworkError::alloc(allocator, NetworkErrorCode::MessageSizeIsTooBig);
		case WSAEPROTONOSUPPORT:
			return NetworkError::alloc(allocator, NetworkErrorCode::ProtocolNotSupported);
		case WSAESOCKTNOSUPPORT:
			return NetworkError::alloc(allocator, NetworkErrorCode::SocketTypeNotSupported);
		case WSAEADDRNOTAVAIL:
			return NetworkError::alloc(allocator, NetworkErrorCode::AddressNotAvailable);
		case WSAENETDOWN:
			return NetworkError::alloc(allocator, NetworkErrorCode::NetworkIsDown);
		case WSAENETRESET:
			return NetworkError::alloc(allocator, NetworkErrorCode::NetworkReseted);
		case WSAENETUNREACH:
			return NetworkError::alloc(allocator, NetworkErrorCode::NetworkIsUnreachable);
		case WSAECONNRESET:
			return NetworkError::alloc(allocator, NetworkErrorCode::ConnectionReseted);
		case WSAESHUTDOWN:
			return NetworkError::alloc(allocator, NetworkErrorCode::Shutdown);
		case WSAETIMEDOUT:
			return NetworkError::alloc(allocator, NetworkErrorCode::TimedOut);
		case WSAECONNREFUSED:
			return NetworkError::alloc(allocator, NetworkErrorCode::ConnectionRefused);
		case WSAEHOSTDOWN:
			return NetworkError::alloc(allocator, NetworkErrorCode::HostIsDown);
		case WSAEHOSTUNREACH:
			return NetworkError::alloc(allocator, NetworkErrorCode::HostIsUnreachable);
		case WSAEPROCLIM:
		case WSAEUSERS:
		case WSAEDQUOT:
			return NetworkError::alloc(allocator, NetworkErrorCode::ResourceLimitExceeded);
		case WSASYSNOTREADY:
			return NetworkError::alloc(allocator, NetworkErrorCode::SystemIsNotReady);
		case WSAVERNOTSUPPORTED:
			return NetworkError::alloc(allocator, NetworkErrorCode::UnsupportedPlatform);
		case WSANOTINITIALISED:
			return NetworkError::alloc(allocator, NetworkErrorCode::ErrorInit);
		default:
			break;
	}
	std::terminate();
}

NETKNOT_API ExceptionPointer netknot::createIOCPIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(ver, &wsaData))
		std::terminate();

	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> ioService(Win32IOService::alloc(params.allocator.get()));

	if (!ioService)
		return OutOfMemoryError::alloc();

	if (!ioService->threadLocalData.resizeUninitialized(params.nWorkerThreads)) {
		return OutOfMemoryError::alloc();
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		peff::constructAt(&ioService->threadLocalData.at(i), ioService.get(), i, params.allocator.get());
	}

	size_t idxWorkerThread = 0;
	peff::ScopeGuard releaseThreadsGuard([&ioService, &idxWorkerThread]() noexcept {
		for (size_t j = 0; j < idxWorkerThread; ++j) {
			Win32IOService::ThreadLocalData &tld = ioService->threadLocalData.at(idxWorkerThread);
			tld.terminate = true;
			ResumeThread(tld.hThread);
		}
	});
	while (idxWorkerThread < params.nWorkerThreads) {
		Win32IOService::ThreadLocalData &tld = ioService->threadLocalData.at(idxWorkerThread);

		HANDLE hThread = CreateThread(NULL, params.szWorkerThreadStackSize, Win32IOService::_workerThreadProc, &tld, CREATE_SUSPENDED, 0);

		if (hThread == INVALID_HANDLE_VALUE)
			return OutOfMemoryError::alloc();

		tld.hThread = hThread;

		++idxWorkerThread;
	}

	InitializeCriticalSection(&ioService->terminateNotifyCriticalSection);
	InitializeConditionVariable(&ioService->terminateNotifyConditionVar);

	InitializeCriticalSection(&ioService->currentTasksCriticalSection);

	if (!((ioService->iocpCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)))) {
		std::terminate();
	}

	releaseThreadsGuard.release();

	ioServiceOut = ioService.release();

	return {};
}

NETKNOT_API ExceptionPointer netknot::createDefaultIOService(IOService*& ioServiceOut, const IOServiceCreationParams& params) noexcept {
	return createIOCPIOService(ioServiceOut, params);
}

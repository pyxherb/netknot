#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32TranslatedAddress::Win32TranslatedAddress(peff::Alloc *self_allocator) : self_allocator(self_allocator) {
}

NETKNOT_API Win32TranslatedAddress::~Win32TranslatedAddress() {
	if (data) {
		self_allocator->release(data, size, 1);
	}
}

NETKNOT_API void Win32TranslatedAddress::dealloc() noexcept {
	peff::destroy_and_release<Win32TranslatedAddress>(self_allocator.get(), this, alignof(Win32TranslatedAddress));
}

NETKNOT_API DWORD WINAPI Win32IOService::_worker_thread_proc(LPVOID lpThreadParameter) {
	ThreadLocalData *tld = (ThreadLocalData *)lpThreadParameter;

	while (true) {
		DWORD szTransferred;
		ULONG_PTR key;
		LPOVERLAPPED ov;

		if (!GetQueuedCompletionStatus(tld->io_service->iocpCompletionPort, &szTransferred, &key, &ov, INFINITE)) {
			DWORD e = WSAGetLastError();
			tld->exceptionStorage = wsaLastErrorToExcept(tld->io_service->self_allocator.get(), e);
			return e;
		}

		if (!key)
			break;

		Win32IOCPOverlapped *iocpOverlapped = (Win32IOCPOverlapped *)ov;

		peff::RcObjectPtr<AsyncTask> rawTask = iocpOverlapped->async_task;
		tld->io_service->cur_tasks_mutex.lock();
		if (tld->io_service->cur_tasks.contains(iocpOverlapped->async_task)) {
			tld->io_service->cur_tasks.remove(iocpOverlapped->async_task);

			tld->io_service->cur_tasks_mutex.unlock();

			switch (rawTask->get_task_type()) {
				case AsyncTaskType::Read: {
					peff::RcObjectPtr<Win32ReadAsyncTask> task = (Win32ReadAsyncTask *)iocpOverlapped->async_task;

					std::lock_guard access_guard(task->access_mutex);

					task->sz_read += szTransferred;
					task->status = AsyncTaskStatus::Done;

					if ((tld->exceptionStorage = task->callback->on_status_changed(task.get()))) {
						WakeAllConditionVariable(&tld->io_service->terminateNotifyConditionVar);
						return -1;
					}

					break;
				}
				case AsyncTaskType::Write: {
					peff::RcObjectPtr<Win32WriteAsyncTask> task = (Win32WriteAsyncTask *)iocpOverlapped->async_task;

					std::lock_guard access_guard(task->access_mutex);

					task->szWritten += szTransferred;
					task->status = AsyncTaskStatus::Done;

					if ((tld->exceptionStorage = task->callback->on_status_changed(task.get()))) {
						WakeAllConditionVariable(&tld->io_service->terminateNotifyConditionVar);
						return -1;
					}

					break;
				}
				case AsyncTaskType::Accept: {
					peff::RcObjectPtr<Win32AcceptAsyncTask> task = (Win32AcceptAsyncTask *)iocpOverlapped->async_task;

					std::lock_guard access_guard(task->access_mutex);

					task->status = AsyncTaskStatus::Done;

					if ((tld->exceptionStorage = task->callback->on_accepted(task->socket.release()))) {
						WakeAllConditionVariable(&tld->io_service->terminateNotifyConditionVar);
						return -1;
					}
					break;
				}
			}
		} else
			tld->io_service->cur_tasks_mutex.unlock();
	}

	tld->io_service->cur_tasks_mutex.lock();
	tld->io_service->cur_tasks.clear();
	tld->io_service->cur_tasks_mutex.unlock();

	return 0;
}

NETKNOT_API Win32IOService::ThreadLocalData::~ThreadLocalData() {
	if (hThread != INVALID_HANDLE_VALUE) {
		terminate = true;
		WaitForSingleObject(hThread, INFINITE);
	}
}

NETKNOT_API Win32IOService::Win32IOService(peff::Alloc *self_allocator)
	: self_allocator(self_allocator),
	  threadLocalData(self_allocator),
	  cur_tasks(self_allocator) {
	InitializeConditionVariable(&terminateNotifyConditionVar);
	InitializeCriticalSection(&terminateNotifyCriticalSection);
}

NETKNOT_API Win32IOService::~Win32IOService() {
	if (_isRunning)
		peff::panic("Destructing I/O service when the service is still running");
	WSACleanup();
}

NETKNOT_API Win32IOService *Win32IOService::alloc(peff::Alloc *self_allocator) {
	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> p(
		peff::alloc_and_construct<Win32IOService>(self_allocator, alignof(Win32IOService), self_allocator));

	if (!p)
		return nullptr;

	return p.release();
}

NETKNOT_API void Win32IOService::dealloc() noexcept {
	peff::destroy_and_release<Win32IOService>(self_allocator.get(), this, alignof(Win32IOService));
}

NETKNOT_API ExceptionPointer Win32IOService::run() {
	if (_isRunning)
		std::terminate();

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	_isRunning = true;

	for (auto &i : threadLocalData) {
		ResumeThread(i.hThread);
	}

	SleepConditionVariableCS(&terminateNotifyConditionVar, &terminateNotifyCriticalSection, INFINITE);

	for (auto &i : threadLocalData) {
		PostQueuedCompletionStatus(iocpCompletionPort, 0, 0, nullptr);
	}

	for (auto &i : threadLocalData) {
		WaitForSingleObject(i.hThread, INFINITE);
	}

	for (auto &i : threadLocalData) {
		NETKNOT_RETURN_IF_EXCEPT(std::move(i.exceptionStorage));
	}

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::stop() {
	if (!_isRunning)
		std::terminate();

	_isRunning = false;

	WakeAllConditionVariable(&terminateNotifyConditionVar);

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::post_async_task(AsyncTask *task) noexcept {
	std::lock_guard cur_tasksGuard(cur_tasks_mutex);

	if (!cur_tasks.insert(task))
		return OutOfMemoryError::alloc();

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::create_socket(peff::Alloc *allocator, const peff::UUID &address_family, const peff::UUID &socketType, Socket *&socketOut) noexcept {
	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::alloc_and_construct<Win32Socket>(allocator, alignof(Win32Socket), this, allocator, address_family, socketType));

	if (!p)
		return OutOfMemoryError::alloc();

	SOCKET &s = p->socket;

	int af;
	if (address_family == ADDRFAM_IPV4) {
		af = AF_INET;
	} else if (address_family == ADDRFAM_IPV6) {
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

NETKNOT_API ExceptionPointer Win32IOService::translate_addr(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut) noexcept {
	if (address->address_family == ADDRFAM_IPV4) {
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
				compiledAddress(peff::alloc_and_construct<Win32TranslatedAddress>(allocator, alignof(Win32TranslatedAddress), allocator));

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

ExceptionPointer Win32IOService::detranslate_addr(peff::Alloc *allocator, const peff::UUID &address_family, const TranslatedAddress *address, Address &addressOut) noexcept {
	if (address_family == ADDRFAM_IPV4) {
		sockaddr_in *sa = (sockaddr_in *)((Win32TranslatedAddress *)address)->data;

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

NETKNOT_API ExceptionPointer netknot::lastErrorToExcept(peff::Alloc *allocator, DWORD errorCode) noexcept {
	switch (errorCode) {
		case ERROR_OUTOFMEMORY:
			return OutOfMemoryError::alloc();
		default:
			break;
	}
	std::terminate();
}

NETKNOT_API ExceptionPointer netknot::wsaLastErrorToExcept(peff::Alloc *allocator, DWORD errorCode) noexcept {
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

	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> io_service(Win32IOService::alloc(params.allocator.get()));

	if (!io_service)
		return OutOfMemoryError::alloc();

	if (!io_service->threadLocalData.resize_uninit(params.nWorkerThreads)) {
		return OutOfMemoryError::alloc();
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		peff::construct_at(&io_service->threadLocalData.at(i), io_service.get(), i, params.allocator.get());
	}

	size_t idxWorkerThread = 0;
	peff::ScopeGuard release_threads_guard([&io_service, &idxWorkerThread]() noexcept {
		for (size_t j = 0; j < idxWorkerThread; ++j) {
			Win32IOService::ThreadLocalData &tld = io_service->threadLocalData.at(idxWorkerThread);
			tld.terminate = true;
			ResumeThread(tld.hThread);
		}
	});
	while (idxWorkerThread < params.nWorkerThreads) {
		Win32IOService::ThreadLocalData &tld = io_service->threadLocalData.at(idxWorkerThread);

		HANDLE hThread = CreateThread(NULL, params.szWorkerThreadStackSize, Win32IOService::_worker_thread_proc, &tld, CREATE_SUSPENDED, 0);

		if (hThread == INVALID_HANDLE_VALUE)
			return OutOfMemoryError::alloc();

		tld.hThread = hThread;

		++idxWorkerThread;
	}

	if (!((io_service->iocpCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)))) {
		std::terminate();
	}

	release_threads_guard.release();

	ioServiceOut = io_service.release();

	return {};
}

NETKNOT_API ExceptionPointer netknot::create_default_io_service(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	return createIOCPIOService(ioServiceOut, params);
}

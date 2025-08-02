#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32CompiledAddress::Win32CompiledAddress(peff::Alloc *selfAllocator) : selfAllocator(selfAllocator) {
}

NETKNOT_API Win32CompiledAddress::~Win32CompiledAddress() {
	if (data) {
		selfAllocator->release(data, size, 1);
	}
}

NETKNOT_API void Win32CompiledAddress::dealloc() noexcept {
	peff::destroyAndRelease<Win32CompiledAddress>(selfAllocator.get(), this, alignof(Win32CompiledAddress));
}

NETKNOT_API DWORD WINAPI Win32IOService::_workerThreadProc(LPVOID lpThreadParameter) {
	ThreadLocalData *tld = (ThreadLocalData *)lpThreadParameter;

	while (true) {
		DWORD szTransferred;
		ULONG_PTR key;
		LPOVERLAPPED ov;

		GetQueuedCompletionStatus(tld->ioService->iocpCompletionPort, &szTransferred, &key, &ov, INFINITE);

		IOCPOverlapped *iocpOverlapped = (IOCPOverlapped *)ov;
	}
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
	  sortedThreadIndicesAlloc(nullptr, 0),
	  sortedThreadSetAlloc(nullptr, 0),
	  sortedThreadIndices(selfAllocator) {
}

NETKNOT_API Win32IOService::~Win32IOService() {
	if (sortedThreadIndicesBuffer) {
		selfAllocator->release(sortedThreadIndicesBuffer, szSortedThreadIndicesBuffer, alignSortedThreadIndicesBuffer);
	}
	if (sortedThreadSetBuffer) {
		selfAllocator->release(sortedThreadSetBuffer, szSortedThreadSetBuffer, alignSortedThreadSetBuffer);
	}
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

NETKNOT_API void Win32IOService::run() {
	sortThreadsByLoad();

	for (auto &i : threadLocalData) {
		ResumeThread(i.hThread);
	}
}

NETKNOT_API ExceptionPointer Win32IOService::postAsyncTask(AsyncTask *task) noexcept {
	EnterCriticalSection(&threadResortCriticalSection);

	peff::ScopeGuard leaveCriticalSectionGuard([this]() noexcept {
		LeaveCriticalSection(&threadResortCriticalSection);
	});

	auto &set = sortedThreadIndices.begin().value();

	size_t selectedThreadId = *set.begin();

	ThreadLocalData &tld = threadLocalData.at(selectedThreadId);

	size_t load = tld.currentTasks.size();
	size_t newLoad = load + 1;

	if (!tld.currentTasks.insert(task))
		return OutOfMemoryError::alloc();

	set.remove(+selectedThreadId);

	addIntoOrInsertNewSortedThreadGroup(newLoad, selectedThreadId);

	return {};
}

NETKNOT_API ExceptionPointer Win32IOService::createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType, Socket *&socketOut) noexcept {
	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::allocAndConstruct<Win32Socket>(allocator, alignof(Win32Socket), addressFamily, socketType));

	if (!p)
		return OutOfMemoryError::alloc();

	SOCKET &s = p->socket;

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

	if (s == INVALID_SOCKET) {
		std::terminate();
	}
}

NETKNOT_API ExceptionPointer Win32IOService::compileAddress(peff::Alloc *allocator, const Address *address, CompiledAddress *&compiledAddressOut) noexcept {
	if (address->addressFamily == ADDRFAM_IPV4) {
		SOCKADDR_IN sa = { 0 };

		if (address) {
			const IPv4Address *addr = (const IPv4Address *)address;

			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = ((addr->a << 24) | (addr->b << 16) | (addr->c << 8) | (addr->d));
			sa.sin_port = htons(addr->port);
		}

		std::unique_ptr<Win32CompiledAddress, peff::DeallocableDeleter<Win32CompiledAddress>>
			compiledAddress(peff::allocAndConstruct<Win32CompiledAddress>(allocator, alignof(Win32CompiledAddress), allocator));

		if (!compiledAddress)
			return OutOfMemoryError::alloc();

		if (!(compiledAddress->data = (char *)allocator->alloc(sizeof(sa), 1))) {
			return OutOfMemoryError::alloc();
		}

		compiledAddress->size = sizeof(sa);

		compiledAddressOut = compiledAddress.release();

		return {};
	} else if (address->addressFamily == ADDRFAM_IPV6) {
	}

	std::terminate();
}

ExceptionPointer Win32IOService::decompileAddress(peff::Alloc *allocator, const peff::UUID &addressFamily, const CompiledAddress *address, Address &addressOut) noexcept {
	if (addressFamily == ADDRFAM_IPV4) {
		sockaddr_in *sa = (sockaddr_in *)((Win32CompiledAddress *)address)->data;

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

NETKNOT_API void Win32IOService::addIntoOrInsertNewSortedThreadGroup(size_t load, size_t index) {
	if (auto it = sortedThreadIndices.find(load); it != sortedThreadIndices.end()) {
		if (!it.value().insert(+index))
			std::terminate();
	} else {
		peff::Set<size_t> groupSet(&sortedThreadIndicesAlloc);

		if (!groupSet.insert(+index))
			std::terminate();

		if (!sortedThreadIndices.insert(+load, std::move(groupSet)))
			std::terminate();
	}
}

NETKNOT_API void Win32IOService::sortThreadsByLoad() {
	sortedThreadIndices.clear();

	for (size_t i = 0; i < threadLocalData.size(); ++i) {
		auto &tld = threadLocalData.at(i);

		addIntoOrInsertNewSortedThreadGroup(tld.currentTasks.size(), i);
	}
}

NETKNOT_API ExceptionPointer netknot::createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	HANDLE iocpCompletionPort;

	if ((iocpCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)) == INVALID_HANDLE_VALUE) {
		std::terminate();
	}

	peff::ScopeGuard closeIocpCompletionPortGuard([iocpCompletionPort]() noexcept {
		CloseHandle(iocpCompletionPort);
	});

	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> ioService(Win32IOService::alloc(params.allocator.get()));

	if (!ioService)
		return OutOfMemoryError::alloc();

	if (!ioService->threadLocalData.resizeUninitialized(params.nWorkerThreads)) {
		return OutOfMemoryError::alloc();
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		peff::constructAt(&ioService->threadLocalData.at(i), ioService.get(), i, params.allocator.get());
	}

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
		Win32IOService::ThreadLocalData &tld = ioService->threadLocalData.at(i);

		HANDLE hThread = CreateThread(NULL, params.szWorkerThreadStackSize, Win32IOService::_workerThreadProc, &tld, CREATE_SUSPENDED, 0);

		if (hThread == INVALID_HANDLE_VALUE) {
			std::terminate();
		}

		tld.hThread = hThread;
	}

	{
		const size_t szBuffer = (peff::BufferAlloc::calcAllocSize(
									sizeof(peff::Map<size_t, peff::Set<size_t>>::NodeType),
									alignof(std::max_align_t))) *
								params.nWorkerThreads,
					 alignment = alignof(std::max_align_t);

		if (!(ioService->sortedThreadIndicesBuffer = (char *)params.allocator->alloc(
				  szBuffer,
				  alignment))) {
			return OutOfMemoryError::alloc();
		}

		ioService->sortedThreadIndicesAlloc = peff::BufferAlloc(ioService->sortedThreadIndicesBuffer, szBuffer);

		ioService->szSortedThreadIndicesBuffer = szBuffer;
		ioService->alignSortedThreadIndicesBuffer = alignment;
	}

	{
		const size_t szBuffer = (peff::BufferAlloc::calcAllocSize(
									sizeof(peff::Set<size_t>::NodeType),
									alignof(std::max_align_t))) *
								params.nWorkerThreads,
					 alignment = alignof(std::max_align_t);

		if (!(ioService->sortedThreadSetBuffer = (char *)params.allocator->alloc(
				  szBuffer,
				  alignment))) {
			return OutOfMemoryError::alloc();
		}

		ioService->sortedThreadSetAlloc = peff::BufferAlloc(ioService->sortedThreadSetBuffer, szBuffer);

		ioService->szSortedThreadSetBuffer = szBuffer;
		ioService->alignSortedThreadSetBuffer = alignment;
	}

	ioServiceOut = ioService.release();

	return {};
}

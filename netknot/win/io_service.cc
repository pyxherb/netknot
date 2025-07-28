#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32CompiledAddress::Win32CompiledAddress(peff::Alloc *selfAllocator): selfAllocator(selfAllocator) {
}

NETKNOT_API Win32CompiledAddress::~Win32CompiledAddress() {
	if (data) {
		selfAllocator->release(data, size, 1);
	}
}

NETKNOT_API void Win32CompiledAddress::dealloc() noexcept {
	peff::destroyAndRelease<Win32CompiledAddress>(selfAllocator.get(), this, alignof(Win32CompiledAddress));
}

NETKNOT_API Win32IOService::Win32IOService(peff::Alloc *selfAllocator) : selfAllocator(selfAllocator) {
}

NETKNOT_API Win32IOService::~Win32IOService() {
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
}

NETKNOT_API ExceptionPointer Win32IOService::createSocket(peff::Alloc *allocator, const peff::UUID &addressFamily, const peff::UUID &socketType) {
}

NETKNOT_API ExceptionPointer Win32IOService::compileAddress(peff::Alloc *allocator, const Address &address, CompiledAddress *&compiledAddressOut) {
	if (address.addressFamily == ADDRFAM_IPV4) {
		const IPv4Address &addr = (const IPv4Address &)address;
		SOCKADDR_IN sa = { 0 };

		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = ((addr.a << 24) | (addr.b << 16) | (addr.c << 8) | (addr.d));
		sa.sin_port = htons(addr.port);
	} else if (address.addressFamily == ADDRFAM_IPV6) {
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

	for (size_t i = 0; i < params.nWorkerThreads; ++i) {
	}

	ioServiceOut = ioService.release();

	return {};
}

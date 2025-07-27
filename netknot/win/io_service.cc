#include "io_service.h"

using namespace netknot;

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

ExceptionPointer netknot::createDefaultIOService(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept {
	HANDLE iocpCompletionPort;

	if ((iocpCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0)) == INVALID_HANDLE_VALUE) {
		std::terminate();
	}

	peff::ScopeGuard closeIocpCompletionPortGuard([iocpCompletionPort]() noexcept {
		CloseHandle(iocpCompletionPort);
	});

	std::unique_ptr<Win32IOService, peff::DeallocableDeleter<Win32IOService>> ioService(Win32IOService::alloc(params.allocator.get()));

	if(!ioService)
		return OutOfMemoryError::alloc();

	ioServiceOut = ioService.release();

	return {};
}

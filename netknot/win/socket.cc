#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32ReadAsyncTask::Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32ReadAsyncTask::~Win32ReadAsyncTask() {
}

NETKNOT_API size_t Win32ReadAsyncTask::getCurrentReadSize() {
	return szRead;
}

NETKNOT_API size_t Win32ReadAsyncTask::getExpectedReadSize() {
	return bufferRef.size;
}

NETKNOT_API Win32WriteAsyncTask::Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32WriteAsyncTask::~Win32WriteAsyncTask() {
}

NETKNOT_API size_t Win32WriteAsyncTask::getCurrentWrittenSize() {
	return szWritten;
}

NETKNOT_API size_t Win32WriteAsyncTask::getExpectedWrittenSize() {
	return bufferRef.size;
}

NETKNOT_API Win32AcceptAsyncTask::Win32AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const peff::UUID &addressFamily) : selfAllocator(allocator), socket(socket), addressFamily(addressFamily) {
}

NETKNOT_API Win32AcceptAsyncTask::~Win32AcceptAsyncTask() {
}

NETKNOT_API Win32IPv4AcceptAsyncTask::Win32IPv4AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket) : Win32AcceptAsyncTask(allocator, socket, ADDRFAM_IPV4) {
}

NETKNOT_API Win32IPv4AcceptAsyncTask::~Win32IPv4AcceptAsyncTask() {
}

NETKNOT_API Address &Win32IPv4AcceptAsyncTask::getAcceptedAddress() {
	return convertedAddress;
}

NETKNOT_API Win32Socket::Win32Socket(SOCKET socket, const peff::UUID &socketTypeId) : socket(socket) {
}

NETKNOT_API Win32Socket::~Win32Socket() {
	close();
}

NETKNOT_API void Win32Socket::dealloc() noexcept {
	peff::destroyAndRelease<Win32Socket>(selfAllocator.get(), this, alignof(Win32Socket));
}

NETKNOT_API void Win32Socket::close() {
	// TODO: Do we actually need to set socket to INVALID_SOCKET to represent if the socket is closed?
	if (socket == INVALID_SOCKET) {
		closesocket(socket);
	}
}

NETKNOT_API ExceptionPointer Win32Socket::bind(const CompiledAddress *address) {
	const Win32CompiledAddress *addr = (const Win32CompiledAddress *)address;

	int result = ::bind(socket, (const sockaddr *)addr->data, addr->size);

	if (result == SOCKET_ERROR) {
		std::terminate();
	}

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::listen(size_t backlog) {
	int result = ::listen(socket, (int)backlog);

	if (result == SOCKET_ERROR) {
		std::terminate();
	}

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::connect(const CompiledAddress *address) {
	const Win32CompiledAddress *addr = (const Win32CompiledAddress *)address;

	int result = ::connect(socket, (const sockaddr *)addr->data, addr->size);

	if (result == SOCKET_ERROR) {
		std::terminate();
	}

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::read(char *buffer, size_t size, size_t &szReadOut) {
	int result = ::recv(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR) {
		// TODO: Handle the errors...
		std::terminate();
	}

	szReadOut = (size_t)result;

	return {};
}
NETKNOT_API ExceptionPointer Win32Socket::write(const char *buffer, size_t size, size_t &szWrittenOut) {
	int result = ::send(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR) {
		// TODO: Handle the errors...
		std::terminate();
	}

	szWrittenOut = (size_t)result;

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::accept(peff::Alloc *allocator, Socket *&socketOut) {
	int addrLen = 0;
	SOCKET newSocket = ::accept(socket, NULL, &addrLen);

	if (newSocket == INVALID_SOCKET) {
		// TODO: Handle the errors...
		std::terminate();
	}

	return {};
}

NETKNOT_API ReadAsyncTask *Win32Socket::readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback) {
	std::unique_ptr<Win32ReadAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32ReadAsyncTask>(allocator, alignof(Win32ReadAsyncTask), allocator, this, buffer));

	if (!task)
		return nullptr;
}

NETKNOT_API WriteAsyncTask *Win32Socket::writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback) {
}

NETKNOT_API AcceptAsyncTask *Win32Socket::acceptAsync(peff::Alloc *allocator) {
}

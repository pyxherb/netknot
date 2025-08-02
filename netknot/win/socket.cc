#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32ReadAsyncTask::Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32ReadAsyncTask::~Win32ReadAsyncTask() {
}

NETKNOT_API void Win32ReadAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<Win32ReadAsyncTask>(selfAllocator.get(), this, alignof(Win32ReadAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32ReadAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32ReadAsyncTask::getException() {
	return exceptPtr;
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

NETKNOT_API void Win32WriteAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<Win32WriteAsyncTask>(selfAllocator.get(), this, alignof(Win32WriteAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32WriteAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32WriteAsyncTask::getException() {
	return exceptPtr;
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

NETKNOT_API void Win32AcceptAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<Win32AcceptAsyncTask>(selfAllocator.get(), this, alignof(Win32AcceptAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32AcceptAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32AcceptAsyncTask::getException() {
	return exceptPtr;
}

NETKNOT_API Win32Socket::Win32Socket(const peff::UUID &addressFamily, const peff::UUID &socketTypeId) : socket(socket), addressFamily(addressFamily), socketTypeId(socketTypeId) {
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

	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::allocAndConstruct<Win32Socket>(allocator, alignof(Win32Socket), newSocket, addressFamily, socketTypeId));

	if (!p)
		return OutOfMemoryError::alloc();

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&asyncTaskOut) {
	std::unique_ptr<Win32ReadAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32ReadAsyncTask>(allocator, alignof(Win32ReadAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	asyncTaskOut = task.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&asyncTaskOut) {
	std::unique_ptr<Win32WriteAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32WriteAsyncTask>(allocator, alignof(Win32WriteAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	asyncTaskOut = task.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::acceptAsync(peff::Alloc *allocator, AcceptAsyncTask *&asyncTaskOut) {
	std::unique_ptr<Win32AcceptAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32AcceptAsyncTask>(allocator, alignof(Win32AcceptAsyncTask), allocator, this, addressFamily));

	if (!task)
		return OutOfMemoryError::alloc();

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	asyncTaskOut = task.release();

	return {};
}

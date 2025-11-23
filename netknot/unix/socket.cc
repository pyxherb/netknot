#include "io_service.h"

using namespace netknot;

NETKNOT_API UnixReadAsyncTask::UnixReadAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API UnixReadAsyncTask::~UnixReadAsyncTask() {
}

NETKNOT_API void UnixReadAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<UnixReadAsyncTask>(selfAllocator.get(), this, alignof(UnixReadAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixReadAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixReadAsyncTask::getException() {
	return exceptPtr;
}

NETKNOT_API size_t UnixReadAsyncTask::getCurrentReadSize() {
	return szRead;
}

NETKNOT_API size_t UnixReadAsyncTask::getExpectedReadSize() {
	return bufferRef.size;
}

NETKNOT_API char *UnixReadAsyncTask::getBuffer() {
	return bufferRef.buffer->data + bufferRef.offset;
}

NETKNOT_API RcBufferRef UnixReadAsyncTask::getBufferRef() {
	return bufferRef;
}

NETKNOT_API UnixWriteAsyncTask::UnixWriteAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API UnixWriteAsyncTask::~UnixWriteAsyncTask() {
}

NETKNOT_API void UnixWriteAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<UnixWriteAsyncTask>(selfAllocator.get(), this, alignof(UnixWriteAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixWriteAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixWriteAsyncTask::getException() {
	return exceptPtr;
}

NETKNOT_API size_t UnixWriteAsyncTask::getCurrentWrittenSize() {
	return szWritten;
}

NETKNOT_API size_t UnixWriteAsyncTask::getExpectedWrittenSize() {
	return bufferRef.size;
}

NETKNOT_API UnixAcceptAsyncTask::UnixAcceptAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const peff::UUID &addressFamily) : selfAllocator(allocator), socket(socket), addressFamily(addressFamily) {
}

NETKNOT_API UnixAcceptAsyncTask::~UnixAcceptAsyncTask() {
}

NETKNOT_API void UnixAcceptAsyncTask::onRefZero() noexcept {
	peff::destroyAndRelease<UnixAcceptAsyncTask>(selfAllocator.get(), this, alignof(UnixAcceptAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixAcceptAsyncTask::getStatus() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixAcceptAsyncTask::getException() {
	return exceptPtr;
}

NETKNOT_API UnixSocket::UnixSocket(UnixIOService *ioService, const peff::UUID &addressFamily, const peff::UUID &socketTypeId) : ioService(ioService), socket(socket), addressFamily(addressFamily), socketTypeId(socketTypeId) {
}

NETKNOT_API UnixSocket::~UnixSocket() {
	if (socket >= 0)
		std::terminate();
}

NETKNOT_API void UnixSocket::dealloc() noexcept {
	peff::destroyAndRelease<UnixSocket>(selfAllocator.get(), this, alignof(UnixSocket));
}

NETKNOT_API void UnixSocket::close() {
	// TODO: Do we actually need to set socket to INVALID_SOCKET to represent if the socket is closed?
	if (socket >= 0) {
		::close(socket);
	}
}

NETKNOT_API ExceptionPointer UnixSocket::bind(const CompiledAddress *address) {
	const UnixCompiledAddress *addr = (const UnixCompiledAddress *)address;

	int result = ::bind(socket, (const sockaddr *)addr->data, addr->size);

	if (result < 0) {
		std::terminate();
	}

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::listen(size_t backlog) {
	int result = ::listen(socket, (int)backlog);

	if (result < 0) {
		std::terminate();
	}

	this->backlog = backlog;

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::connect(const CompiledAddress *address) {
	const UnixCompiledAddress *addr = (const UnixCompiledAddress *)address;

	int result = ::connect(socket, (const sockaddr *)addr->data, addr->size);

	if (result < 0) {
		std::terminate();
	}

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::read(char *buffer, size_t size, size_t &szReadOut) {
	int result = ::recv(socket, buffer, (int)size, 0);

	if (result < 0) {
		// TODO: Handle the errors...
		std::terminate();
	}

	szReadOut = (size_t)result;

	return {};
}
NETKNOT_API ExceptionPointer UnixSocket::write(const char *buffer, size_t size, size_t &szWrittenOut) {
	int result = ::send(socket, buffer, (int)size, 0);

	if (result < 0) {
		// TODO: Handle the errors...
		std::terminate();
	}

	szWrittenOut = (size_t)result;

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::accept(peff::Alloc *allocator, Socket *&socketOut) {
	socklen_t addrLen = 0;
	int newSocket = ::accept(socket, NULL, &addrLen);

	if (newSocket < 0) {
		// TODO: Handle the errors...
		std::terminate();
	}

	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> p(
		peff::allocAndConstruct<UnixSocket>(allocator, alignof(UnixSocket), ioService, addressFamily, socketTypeId));

	if (!p)
		return OutOfMemoryError::alloc();

	p->socket = newSocket;

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&asyncTaskOut) {
	peff::RcObjectPtr<UnixReadAsyncTask> task(
		peff::allocAndConstruct<UnixReadAsyncTask>(allocator, alignof(UnixReadAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	// TODO: Implement it.

	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	task->incRef(peff::acquireGlobalRcObjectPtrCounter());
	asyncTaskOut = task.get();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&asyncTaskOut) {
	std::unique_ptr<UnixWriteAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<UnixWriteAsyncTask>(allocator, alignof(UnixWriteAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	// TODO: Implement it.

	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	task->incRef(peff::acquireGlobalRcObjectPtrCounter());
	asyncTaskOut = task.release();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::acceptAsync(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&asyncTaskOut) {
	std::unique_ptr<UnixAcceptAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<UnixAcceptAsyncTask>(allocator, alignof(UnixAcceptAsyncTask), allocator, this, addressFamily));

	if (!task)
		return OutOfMemoryError::alloc();

	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> newSocket;
	{
		Socket *s;

		NETKNOT_RETURN_IF_EXCEPT(ioService->createSocket(allocator, addressFamily, socketTypeId, s));

		newSocket = std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>>((UnixSocket *)s);
	}

	size_t compiledAddrSize;

	{
		Address addr(addressFamily);

		ioService->compileAddress(nullptr, &addr, nullptr, &compiledAddrSize).unwrap();
	}

	// TODO: Implement it.

	task->socket = newSocket.get();
	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	newSocket.release();

	task->incRef(peff::acquireGlobalRcObjectPtrCounter());
	asyncTaskOut = task.release();

	return {};
}

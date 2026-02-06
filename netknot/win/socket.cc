#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32ReadAsyncTask::Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32ReadAsyncTask::~Win32ReadAsyncTask() {
	releaseOverlapped(selfAllocator.get(), overlapped);
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

NETKNOT_API char *Win32ReadAsyncTask::getBuffer() {
	return bufferRef.buffer->data + bufferRef.offset;
}

NETKNOT_API RcBufferRef Win32ReadAsyncTask::getBufferRef() {
	return bufferRef;
}

NETKNOT_API Win32WriteAsyncTask::Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : selfAllocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32WriteAsyncTask::~Win32WriteAsyncTask() {
	releaseOverlapped(selfAllocator.get(), overlapped);
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
	releaseOverlapped(selfAllocator.get(), overlapped);
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

NETKNOT_API Win32Socket::Win32Socket(Win32IOService *ioService, peff::Alloc *selfAllocator, const peff::UUID &addressFamily, const peff::UUID &socketTypeId) : ioService(ioService), selfAllocator(selfAllocator), socket(INVALID_SOCKET), addressFamily(addressFamily), socketTypeId(socketTypeId) {
}

NETKNOT_API Win32Socket::~Win32Socket() {
	close();
}

NETKNOT_API void Win32Socket::dealloc() noexcept {
	peff::destroyAndRelease<Win32Socket>(selfAllocator.get(), this, alignof(Win32Socket));
}

NETKNOT_API void Win32Socket::close() {
	// TODO: Do we actually need to set socket to INVALID_SOCKET to represent if the socket is closed?
	if (socket != INVALID_SOCKET) {
		closesocket(socket);
	}
}

NETKNOT_API ExceptionPointer Win32Socket::bind(const TranslatedAddress *address) {
	const Win32TranslatedAddress *addr = (const Win32TranslatedAddress *)address;

	int result = ::bind(socket, (const sockaddr *)addr->data, (int)addr->size);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::listen(size_t backlog) {
	int result = ::listen(socket, (int)backlog);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::connect(const TranslatedAddress *address) {
	const Win32TranslatedAddress *addr = (const Win32TranslatedAddress *)address;

	int result = ::connect(socket, (const sockaddr *)addr->data, (int)addr->size);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::read(char *buffer, size_t size, size_t &szReadOut) {
	int result = ::recv(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	szReadOut = (size_t)result;

	return {};
}
NETKNOT_API ExceptionPointer Win32Socket::write(const char *buffer, size_t size, size_t &szWrittenOut) {
	int result = ::send(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	szWrittenOut = (size_t)result;

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::accept(peff::Alloc *allocator, Socket *&socketOut) {
	int addrLen = 0;
	SOCKET newSocket = ::accept(socket, NULL, &addrLen);

	if (newSocket == INVALID_SOCKET)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::allocAndConstruct<Win32Socket>(allocator, alignof(Win32Socket), ioService, allocator, addressFamily, socketTypeId));

	if (!p)
		return OutOfMemoryError::alloc();

	p->socket = newSocket;

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::readAsync(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&asyncTaskOut) {
	if (buffer.buffer->size > ULONG_MAX)
		return BufferIsTooBigError::alloc();
	peff::RcObjectPtr<Win32ReadAsyncTask> task(
		peff::allocAndConstruct<Win32ReadAsyncTask>(allocator, alignof(Win32ReadAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)allocOverlapped(allocator, 0, buffer, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->callback = callback;

	int result = WSARecv(socket, &overlapped->buf, 1, &overlapped->szOperated, &overlapped->flags, overlapped, NULL);

	if (result == SOCKET_ERROR) {
		int errorCode = WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
			return wsaLastErrorToExcept(ioService->selfAllocator.get(), errorCode);
	}

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	task->incRef(0);
	asyncTaskOut = task.get();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::writeAsync(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&asyncTaskOut) {
	if (buffer.buffer->size > ULONG_MAX)
		return BufferIsTooBigError::alloc();
	std::unique_ptr<Win32WriteAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32WriteAsyncTask>(allocator, alignof(Win32WriteAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();
	
	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)allocOverlapped(allocator, 0, buffer, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->callback = callback;

	int result = WSASend(socket, &overlapped->buf, 1, &overlapped->szOperated, 0, overlapped, NULL);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(ioService->selfAllocator.get(), WSAGetLastError());

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));

	task->incRef(0);
	asyncTaskOut = task.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::acceptAsync(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&asyncTaskOut) {
	std::unique_ptr<Win32AcceptAsyncTask, AsyncTaskDeleter> task(
		peff::allocAndConstruct<Win32AcceptAsyncTask>(allocator, alignof(Win32AcceptAsyncTask), allocator, this, addressFamily));

	if (!task)
		return OutOfMemoryError::alloc();

	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> newSocket;
	{
		Socket *s;

		NETKNOT_RETURN_IF_EXCEPT(ioService->createSocket(allocator, addressFamily, socketTypeId, s));

		newSocket = std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>>((Win32Socket *)s);
	}

	size_t compiledAddrSize;

	{
		Address addr(addressFamily);

		ioService->translateAddress(nullptr, &addr, nullptr, &compiledAddrSize).unwrap();

		compiledAddrSize += 16;
	}

	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)allocOverlapped(allocator, compiledAddrSize, RcBufferRef{}, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->socket = newSocket.get();
	task->callback = callback;

	if (!AcceptEx(socket, newSocket->socket, overlapped + 1, 0, (DWORD)overlapped->addrSize, (DWORD)overlapped->addrSize, &overlapped->szOperated, overlapped)) {
		int lastError = WSAGetLastError();
		if (lastError != WSA_IO_PENDING)
			return wsaLastErrorToExcept(ioService->selfAllocator.get(), lastError);
	}

	newSocket.release();

	NETKNOT_RETURN_IF_EXCEPT(ioService->postAsyncTask(task.get()));
	task->incRef(0);
	asyncTaskOut = task.release();

	return {};
}

NETKNOT_API Win32IOCPOverlapped* netknot::allocOverlapped(peff::Alloc* allocator, size_t addrSize, const RcBufferRef& buffer, AsyncTask* asyncTask) {
	Win32IOCPOverlapped *overlapped = nullptr;

	if (!(overlapped = (Win32IOCPOverlapped *)allocator->alloc(sizeof(Win32IOCPOverlapped) + addrSize, alignof(Win32IOCPOverlapped)))) {
		return nullptr;
	}
	memset(overlapped, 0, sizeof(Win32IOCPOverlapped) + addrSize);

	overlapped->addrSize = addrSize;

	if (buffer.buffer) {
		overlapped->buf.len = (ULONG)(buffer.buffer->size - buffer.offset);
		overlapped->buf.buf = buffer.buffer->data + buffer.offset;
		buffer.buffer->incRef(0);
		overlapped->rcBuffer = buffer.buffer.get();
	}

	if (asyncTask) {
		asyncTask->incRef(0);
		overlapped->asyncTask = asyncTask;
	}

	return overlapped;
}

NETKNOT_API void netknot::releaseOverlapped(peff::Alloc *allocator, Win32IOCPOverlapped *overlapped) {
	if (overlapped) {
		if (overlapped->rcBuffer)
			overlapped->rcBuffer->decRef(0);
		overlapped->asyncTask->decRef(0);
		allocator->release(overlapped, sizeof(Win32IOCPOverlapped) + overlapped->addrSize, alignof(Win32IOCPOverlapped));
	}
}

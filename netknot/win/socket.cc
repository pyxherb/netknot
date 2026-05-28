#include "io_service.h"

using namespace netknot;

NETKNOT_API Win32ReadAsyncTask::Win32ReadAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : self_allocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32ReadAsyncTask::~Win32ReadAsyncTask() {
	release_overlapped(self_allocator.get(), overlapped);
}

NETKNOT_API void Win32ReadAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<Win32ReadAsyncTask>(self_allocator.get(), this, alignof(Win32ReadAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32ReadAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32ReadAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API size_t Win32ReadAsyncTask::get_cur_read_size() {
	return sz_read;
}

NETKNOT_API size_t Win32ReadAsyncTask::get_expected_read_size() {
	return bufferRef.size;
}

NETKNOT_API char *Win32ReadAsyncTask::get_buffer() {
	return bufferRef.buffer->data + bufferRef.offset;
}

NETKNOT_API RcBufferRef Win32ReadAsyncTask::get_buffer_ref() {
	return bufferRef;
}

NETKNOT_API Win32WriteAsyncTask::Win32WriteAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const RcBufferRef &bufferRef) : self_allocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API Win32WriteAsyncTask::~Win32WriteAsyncTask() {
	release_overlapped(self_allocator.get(), overlapped);
}

NETKNOT_API void Win32WriteAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<Win32WriteAsyncTask>(self_allocator.get(), this, alignof(Win32WriteAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32WriteAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32WriteAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API size_t Win32WriteAsyncTask::get_cur_written_size() {
	return szWritten;
}

NETKNOT_API size_t Win32WriteAsyncTask::get_expected_written_size() {
	return bufferRef.size;
}

NETKNOT_API Win32AcceptAsyncTask::Win32AcceptAsyncTask(peff::Alloc *allocator, Win32Socket *socket, const peff::UUID &address_family) : self_allocator(allocator), socket(socket), address_family(address_family) {
}

NETKNOT_API Win32AcceptAsyncTask::~Win32AcceptAsyncTask() {
	release_overlapped(self_allocator.get(), overlapped);
}

NETKNOT_API void Win32AcceptAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<Win32AcceptAsyncTask>(self_allocator.get(), this, alignof(Win32AcceptAsyncTask));
}

NETKNOT_API AsyncTaskStatus Win32AcceptAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &Win32AcceptAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API Win32Socket::Win32Socket(Win32IOService *io_service, peff::Alloc *self_allocator, const peff::UUID &address_family, const peff::UUID &socketTypeId) : io_service(io_service), self_allocator(self_allocator), socket(INVALID_SOCKET), address_family(address_family), socketTypeId(socketTypeId) {
}

NETKNOT_API Win32Socket::~Win32Socket() {
	close();
}

NETKNOT_API void Win32Socket::dealloc() noexcept {
	peff::destroy_and_release<Win32Socket>(self_allocator.get(), this, alignof(Win32Socket));
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
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::listen(size_t backlog) {
	int result = ::listen(socket, (int)backlog);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::connect(const TranslatedAddress *address) {
	const Win32TranslatedAddress *addr = (const Win32TranslatedAddress *)address;

	int result = ::connect(socket, (const sockaddr *)addr->data, (int)addr->size);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::read(char *buffer, size_t size, size_t &szReadOut) {
	int result = ::recv(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	szReadOut = (size_t)result;

	return {};
}
NETKNOT_API ExceptionPointer Win32Socket::write(const char *buffer, size_t size, size_t &szWrittenOut) {
	int result = ::send(socket, buffer, (int)size, 0);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	szWrittenOut = (size_t)result;

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::accept(peff::Alloc *allocator, Socket *&socketOut) {
	int addrLen = 0;
	SOCKET new_socket = ::accept(socket, NULL, &addrLen);

	if (new_socket == INVALID_SOCKET)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> p(
		peff::alloc_and_construct<Win32Socket>(allocator, alignof(Win32Socket), io_service, allocator, address_family, socketTypeId));

	if (!p)
		return OutOfMemoryError::alloc();

	p->socket = new_socket;

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::read_async(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&async_task_out) {
	if (buffer.buffer->size > ULONG_MAX)
		return BufferIsTooBigError::alloc();
	peff::RcObjectPtr<Win32ReadAsyncTask> task(
		peff::alloc_and_construct<Win32ReadAsyncTask>(allocator, alignof(Win32ReadAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)alloc_overlapped(allocator, 0, buffer, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->callback = callback;

	int result = WSARecv(socket, &overlapped->buf, 1, &overlapped->szOperated, &overlapped->flags, overlapped, NULL);

	if (result == SOCKET_ERROR) {
		int errorCode = WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
			return wsaLastErrorToExcept(io_service->self_allocator.get(), errorCode);
	}

	NETKNOT_RETURN_IF_EXCEPT(io_service->post_async_task(task.get()));

	task->inc_ref(0);
	async_task_out = task.get();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::write_async(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&async_task_out) {
	if (buffer.buffer->size > ULONG_MAX)
		return BufferIsTooBigError::alloc();
	std::unique_ptr<Win32WriteAsyncTask, AsyncTaskDeleter> task(
		peff::alloc_and_construct<Win32WriteAsyncTask>(allocator, alignof(Win32WriteAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)alloc_overlapped(allocator, 0, buffer, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->callback = callback;

	int result = WSASend(socket, &overlapped->buf, 1, &overlapped->szOperated, 0, overlapped, NULL);

	if (result == SOCKET_ERROR)
		return wsaLastErrorToExcept(io_service->self_allocator.get(), WSAGetLastError());

	NETKNOT_RETURN_IF_EXCEPT(io_service->post_async_task(task.get()));

	task->inc_ref(0);
	async_task_out = task.release();

	return {};
}

NETKNOT_API ExceptionPointer Win32Socket::accept_async(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&async_task_out) {
	std::unique_ptr<Win32AcceptAsyncTask, AsyncTaskDeleter> task(
		peff::alloc_and_construct<Win32AcceptAsyncTask>(allocator, alignof(Win32AcceptAsyncTask), allocator, nullptr, address_family));

	if (!task)
		return OutOfMemoryError::alloc();

	std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>> new_socket;
	{
		Socket *s;

		NETKNOT_RETURN_IF_EXCEPT(io_service->create_socket(allocator, address_family, socketTypeId, s));

		new_socket = std::unique_ptr<Win32Socket, peff::DeallocableDeleter<Win32Socket>>((Win32Socket *)s);
	}

	size_t compiled_addr_size;

	{
		Address addr(address_family);

		io_service->translate_addr(nullptr, &addr, nullptr, &compiled_addr_size).unwrap();

		compiled_addr_size += 16;
	}

	Win32IOCPOverlapped *overlapped;

	if (!(overlapped = (Win32IOCPOverlapped *)alloc_overlapped(allocator, compiled_addr_size, RcBufferRef{}, task.get()))) {
		return OutOfMemoryError::alloc();
	}

	task->overlapped = overlapped;

	task->callback = callback;

	if (!AcceptEx(socket, new_socket->socket, &overlapped[1], 0, (DWORD)overlapped->addrSize, (DWORD)overlapped->addrSize, &overlapped->szOperated, overlapped)) {
		int lastError = WSAGetLastError();
		if (lastError != WSA_IO_PENDING)
			return wsaLastErrorToExcept(io_service->self_allocator.get(), lastError);
	}

	task->socket = new_socket.release();

	NETKNOT_RETURN_IF_EXCEPT(io_service->post_async_task(task.get()));
	task->inc_ref(0);
	async_task_out = task.release();

	return {};
}

NETKNOT_API Win32IOCPOverlapped *netknot::alloc_overlapped(peff::Alloc *allocator, size_t addrSize, const RcBufferRef &buffer, AsyncTask *async_task) {
	Win32IOCPOverlapped *overlapped = nullptr;

	if (!(overlapped = (Win32IOCPOverlapped *)allocator->alloc(sizeof(Win32IOCPOverlapped) + addrSize /* Don't know why it just needs it, 16 more bytes for storage. */ + 16, alignof(Win32IOCPOverlapped)))) {
		return nullptr;
	}
	memset(overlapped, 0, sizeof(Win32IOCPOverlapped) + addrSize);

	overlapped->addrSize = addrSize;

	if (buffer.buffer) {
		overlapped->buf.len = (ULONG)(buffer.buffer->size - buffer.offset);
		overlapped->buf.buf = buffer.buffer->data + buffer.offset;
		buffer.buffer->inc_ref(0);
		overlapped->rcBuffer = buffer.buffer.get();
	}

	if (async_task) {
		overlapped->async_task = async_task;
	}

	return overlapped;
}

NETKNOT_API void netknot::release_overlapped(peff::Alloc *allocator, Win32IOCPOverlapped *overlapped) {
	if (overlapped) {
		if (overlapped->rcBuffer)
			overlapped->rcBuffer->dec_ref(0);
		allocator->release(overlapped, sizeof(Win32IOCPOverlapped) + overlapped->addrSize + 16, alignof(Win32IOCPOverlapped));
	}
}

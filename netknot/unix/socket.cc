#include "io_service.h"

using namespace netknot;

NETKNOT_API UnixReadAsyncTask::UnixReadAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef) : self_allocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API UnixReadAsyncTask::~UnixReadAsyncTask() {
}

NETKNOT_API void UnixReadAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<UnixReadAsyncTask>(self_allocator.get(), this, alignof(UnixReadAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixReadAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixReadAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API size_t UnixReadAsyncTask::get_cur_read_size() {
	return sz_read;
}

NETKNOT_API size_t UnixReadAsyncTask::get_expected_read_size() {
	return bufferRef.size;
}

NETKNOT_API char *UnixReadAsyncTask::get_buffer() {
	return bufferRef.buffer->data + bufferRef.offset;
}

NETKNOT_API RcBufferRef UnixReadAsyncTask::get_buffer_ref() {
	return bufferRef;
}

NETKNOT_API UnixWriteAsyncTask::UnixWriteAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const RcBufferRef &bufferRef) : self_allocator(allocator), socket(socket), bufferRef(bufferRef) {
}

NETKNOT_API UnixWriteAsyncTask::~UnixWriteAsyncTask() {
}

NETKNOT_API void UnixWriteAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<UnixWriteAsyncTask>(self_allocator.get(), this, alignof(UnixWriteAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixWriteAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixWriteAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API size_t UnixWriteAsyncTask::get_cur_written_size() {
	return szWritten;
}

NETKNOT_API size_t UnixWriteAsyncTask::get_expected_written_size() {
	return bufferRef.size;
}

NETKNOT_API UnixAcceptAsyncTask::UnixAcceptAsyncTask(peff::Alloc *allocator, UnixSocket *socket, const peff::UUID &address_family) : self_allocator(allocator), socket(socket), address_family(address_family) {
}

NETKNOT_API UnixAcceptAsyncTask::~UnixAcceptAsyncTask() {
}

NETKNOT_API void UnixAcceptAsyncTask::on_ref_zero() noexcept {
	peff::destroy_and_release<UnixAcceptAsyncTask>(self_allocator.get(), this, alignof(UnixAcceptAsyncTask));
}

NETKNOT_API AsyncTaskStatus UnixAcceptAsyncTask::get_status() {
	return status;
}

NETKNOT_API ExceptionPointer &UnixAcceptAsyncTask::get_except() {
	return except_ptr;
}

NETKNOT_API UnixSocket::UnixSocket(UnixIOService *io_service, const peff::UUID &address_family, const peff::UUID &socketTypeId) : io_service(io_service), socket(socket), address_family(address_family), socketTypeId(socketTypeId) {
}

NETKNOT_API UnixSocket::~UnixSocket() {
	if (socket >= 0)
		std::terminate();
}

NETKNOT_API void UnixSocket::dealloc() noexcept {
	peff::destroy_and_release<UnixSocket>(self_allocator.get(), this, alignof(UnixSocket));
}

NETKNOT_API void UnixSocket::close() {
	// TODO: Do we actually need to set socket to INVALID_SOCKET to represent if the socket is closed?
	if (socket >= 0) {
		::close(socket);
	}
}

NETKNOT_API ExceptionPointer UnixSocket::bind(const TranslatedAddress *address) {
	const UnixTranslatedAddress *addr = (const UnixTranslatedAddress *)address;

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

NETKNOT_API ExceptionPointer UnixSocket::connect(const TranslatedAddress *address) {
	const UnixTranslatedAddress *addr = (const UnixTranslatedAddress *)address;

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
	int new_socket = ::accept(socket, NULL, &addrLen);

	if (new_socket < 0) {
		// TODO: Handle the errors...
		std::terminate();
	}

	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> p(
		peff::alloc_and_construct<UnixSocket>(allocator, alignof(UnixSocket), io_service, address_family, socketTypeId));

	if (!p)
		return OutOfMemoryError::alloc();

	p->socket = new_socket;

	socketOut = p.release();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::read_async(peff::Alloc *allocator, const RcBufferRef &buffer, ReadAsyncCallback *callback, ReadAsyncTask *&async_task_out) {
	peff::RcObjectPtr<UnixReadAsyncTask> task(
		peff::alloc_and_construct<UnixReadAsyncTask>(allocator, alignof(UnixReadAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	// TODO: Implement it.

	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(io_service->postAsyncTask(task.get()));

	task->inc_ref(peff::acquireGlobalRcObjectPtrCounter());
	async_task_out = task.get();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::write_async(peff::Alloc *allocator, const RcBufferRef &buffer, WriteAsyncCallback *callback, WriteAsyncTask *&async_task_out) {
	std::unique_ptr<UnixWriteAsyncTask, AsyncTaskDeleter> task(
		peff::alloc_and_construct<UnixWriteAsyncTask>(allocator, alignof(UnixWriteAsyncTask), allocator, this, buffer));

	if (!task)
		return OutOfMemoryError::alloc();

	// TODO: Implement it.

	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(io_service->postAsyncTask(task.get()));

	task->inc_ref(peff::acquireGlobalRcObjectPtrCounter());
	async_task_out = task.release();

	return {};
}

NETKNOT_API ExceptionPointer UnixSocket::accept_async(peff::Alloc *allocator, AcceptAsyncCallback *callback, AcceptAsyncTask *&async_task_out) {
	std::unique_ptr<UnixAcceptAsyncTask, AsyncTaskDeleter> task(
		peff::alloc_and_construct<UnixAcceptAsyncTask>(allocator, alignof(UnixAcceptAsyncTask), allocator, this, address_family));

	if (!task)
		return OutOfMemoryError::alloc();

	std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>> new_socket;
	{
		Socket *s;

		NETKNOT_RETURN_IF_EXCEPT(io_service->createSocket(allocator, address_family, socketTypeId, s));

		new_socket = std::unique_ptr<UnixSocket, peff::DeallocableDeleter<UnixSocket>>((UnixSocket *)s);
	}

	size_t compiled_addr_size;

	{
		Address addr(address_family);

		io_service->translateAddress(nullptr, &addr, nullptr, &compiled_addr_size).unwrap();
	}

	// TODO: Implement it.

	task->socket = new_socket.get();
	task->callback = callback;

	NETKNOT_RETURN_IF_EXCEPT(io_service->postAsyncTask(task.get()));

	new_socket.release();

	task->inc_ref(peff::acquireGlobalRcObjectPtrCounter());
	async_task_out = task.release();

	return {};
}

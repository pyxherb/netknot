#include "except.h"

using namespace netknot;

OutOfMemoryError netknot::g_outOfMemoryError;

NETKNOT_API OutOfMemoryError::OutOfMemoryError() noexcept : Exception(EXCEPT_OOM) {}
NETKNOT_API OutOfMemoryError::~OutOfMemoryError() {}

NETKNOT_API void OutOfMemoryError::dealloc() {
}

NETKNOT_API OutOfMemoryError *OutOfMemoryError::alloc() noexcept {
	return &g_outOfMemoryError;
}

BufferIsTooBigError netknot::g_bufferIsTooBigError;

NETKNOT_API BufferIsTooBigError::BufferIsTooBigError() noexcept : Exception(EXCEPT_BUFFER_IS_TOO_BIG) {}
NETKNOT_API BufferIsTooBigError::~BufferIsTooBigError() {}

NETKNOT_API void BufferIsTooBigError::dealloc() {
}

NETKNOT_API BufferIsTooBigError *BufferIsTooBigError::alloc() noexcept {
	return &g_bufferIsTooBigError;
}

NETKNOT_API NetworkError::NetworkError(peff::Alloc *allocator, NetworkErrorCode errorCode)
	: Exception(EXCEPT_IO), errorCode(errorCode) {}
NETKNOT_API NetworkError::~NetworkError() {}

NETKNOT_API void NetworkError::dealloc() {
	peff::destroyAndRelease<NetworkError>(allocator.get(), this, sizeof(std::max_align_t));
}

NETKNOT_API NetworkError *NetworkError::alloc(peff::Alloc *allocator, NetworkErrorCode errorCode) noexcept {
	void *buf = allocator->alloc(sizeof(NetworkError), sizeof(std::max_align_t));

	if (!buf)
		return nullptr;

	peff::constructAt<NetworkError>((NetworkError *)buf, allocator, errorCode);

	return (NetworkError *)buf;
}

#include "except.h"

using namespace netknot;

OutOfMemoryError netknot::g_outOfMemoryError;

NETKNOT_API OutOfMemoryError::OutOfMemoryError() : Exception(EXCEPT_OOM) {}
NETKNOT_API OutOfMemoryError::~OutOfMemoryError() {}

NETKNOT_API void OutOfMemoryError::dealloc() {
}

NETKNOT_API OutOfMemoryError *OutOfMemoryError::alloc() {
	return &g_outOfMemoryError;
}

NETKNOT_API IOError::IOError(peff::Alloc *allocator)
	: Exception(EXCEPT_IO) {}
NETKNOT_API IOError::~IOError() {}

NETKNOT_API void IOError::dealloc() {
	peff::destroyAndRelease<IOError>(allocator.get(), this, sizeof(std::max_align_t));
}

NETKNOT_API IOError *IOError::alloc(peff::Alloc *allocator) noexcept {
	void *buf = allocator->alloc(sizeof(IOError), sizeof(std::max_align_t));

	if (!buf)
		return nullptr;

	peff::constructAt<IOError>((IOError *)buf, allocator);

	return (IOError *)buf;
}

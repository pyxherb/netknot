#ifndef _NETKNOT_RC_BUFFER_H_
#define _NETKNOT_RC_BUFFER_H_

#include "except.h"
#include <peff/base/rcobj.h>

namespace netknot {
	class RcBuffer {
	public:
		/// @brief Pointer to the buffer where the data are stored.
		char *data;
		/// @brief Size of the buffer.
		size_t size;

		NETKNOT_API RcBuffer(char *data, size_t size);
		NETKNOT_API virtual ~RcBuffer();

		virtual size_t incRef(size_t globalRc) = 0;
		virtual size_t decRef(size_t globalRc) = 0;
	};

	struct RcBufferRef final {
		peff::RcObjectPtr<RcBuffer> buffer;
		size_t offset;
		size_t size;

		NETKNOT_FORCEINLINE RcBufferRef() noexcept : buffer({}), offset(0), size(0) {
		}

		NETKNOT_FORCEINLINE RcBufferRef(RcBuffer *buffer) noexcept : buffer(buffer), offset(0), size(buffer->size) {
		}

		NETKNOT_FORCEINLINE RcBufferRef(RcBuffer *buffer, size_t offset, size_t size) noexcept : buffer(buffer), offset(offset), size(size) {
			if(offset >= buffer->size)
				std::terminate();
			if(offset + size >= buffer->size)
				std::terminate();
		}

		NETKNOT_FORCEINLINE operator bool() {
			return (bool)buffer;
		}
	};
}

#endif

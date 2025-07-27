#ifndef _NETKNOT_RC_BUFFER_H_
#define _NETKNOT_RC_BUFFER_H_

#include "except.h"
#include <peff/base/rcobj.h>

namespace netknot {
	class RcBuffer {
	public:
		char *data;
		size_t size;

		NETKNOT_API RcBuffer(char *data, size_t size);
		NETKNOT_API virtual ~RcBuffer();

		virtual size_t incRef() = 0;
		virtual size_t decRef() = 0;
	};

	struct RcBufferRef final {
		peff::RcObjectPtr<RcBuffer> buffer;
		size_t offset;
		size_t size;

		NETKNOT_FORCEINLINE RcBufferRef(RcBuffer *buffer) noexcept : buffer(buffer), offset(0), size(buffer->size) {
		}

		NETKNOT_FORCEINLINE RcBufferRef(RcBuffer *buffer, size_t offset, size_t size) noexcept : buffer(buffer), offset(offset), size(size) {
			assert(offset < buffer->size);
			assert(offset + size < buffer->size);
		}
	};
}

#endif

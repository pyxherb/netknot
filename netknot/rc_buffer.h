#ifndef _NETKNOT_RC_BUFFER_H_
#define _NETKNOT_RC_BUFFER_H_

#include "except.h"
#include <peff/base/rcobj.h>

namespace netknot {
	class RcBuffer : public peff::RcObject {
	public:
		char *data;
		size_t size;

		NETKNOT_API RcBuffer(char *data, size_t size);
		NETKNOT_API virtual ~RcBuffer();
	};
}

#endif

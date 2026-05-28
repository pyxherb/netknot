#ifndef _NETKNOT_IO_SERVICE_H_
#define _NETKNOT_IO_SERVICE_H_

#include "socket.h"

namespace netknot {
	struct IOServiceCreationParams {
		peff::RcObjectPtr<peff::Alloc> allocator;
		size_t nWorkerThreads = 0;
		size_t szWorkerThreadStackSize = 0;

		NETKNOT_API IOServiceCreationParams(peff::Alloc *paramsAllocator, peff::Alloc *allocator);
		NETKNOT_API ~IOServiceCreationParams();
	};

	typedef ExceptionPointer (*AddressCompiler)(peff::Alloc *allocator, const Address &address, char *&bufferOut, size_t &szBufferOut);

	class IOService {
	public:
		NETKNOT_API IOService();
		NETKNOT_API ~IOService();

		virtual void dealloc() noexcept = 0;

		[[nodiscard]] virtual ExceptionPointer run() = 0;
		[[nodiscard]] virtual ExceptionPointer stop() = 0;

		virtual ExceptionPointer post_async_task(AsyncTask *task) noexcept = 0;

		virtual ExceptionPointer create_socket(peff::Alloc *allocator, const peff::UUID &address_family, const peff::UUID &socketType, Socket *&socketOut) noexcept = 0;

		virtual ExceptionPointer translate_addr(peff::Alloc *allocator, const Address *address, TranslatedAddress **compiledAddressOut, size_t *compiledAddressSizeOut = nullptr) noexcept = 0;
		virtual ExceptionPointer detranslate_addr(peff::Alloc *allocator, const peff::UUID &address_family, const TranslatedAddress *address, Address &addressOut) noexcept = 0;
	};

	ExceptionPointer create_default_io_service(IOService *&ioServiceOut, const IOServiceCreationParams &params) noexcept;
}

#endif

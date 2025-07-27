#include "io_service.h"

using namespace netknot;

NETKNOT_API IOService::IOService() {
}

NETKNOT_API IOService::~IOService() {
}

NETKNOT_API IOServiceCreationParams::IOServiceCreationParams(peff::Alloc *paramsAllocator, peff::Alloc *allocator) : allocator(allocator) {
}

NETKNOT_API IOServiceCreationParams::~IOServiceCreationParams() {
}

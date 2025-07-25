#include "except_base.h"

using namespace netknot;

NETKNOT_API Exception::Exception(const peff::UUID &kind) : kind(kind) {
}

NETKNOT_API Exception::~Exception() {
}

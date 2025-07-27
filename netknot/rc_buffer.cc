#include "rc_buffer.h"

using namespace netknot;

NETKNOT_API RcBuffer::RcBuffer(char *data, size_t size) : data(data), size(size) {
}

NETKNOT_API RcBuffer::~RcBuffer() {
}

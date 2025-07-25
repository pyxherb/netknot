#include "async_buffer.h"

using namespace netknot;

NETKNOT_API AsyncBuffer::AsyncBuffer(char *data, size_t size) : data(data), size(size) {
}

NETKNOT_API AsyncBuffer::~AsyncBuffer() {
}

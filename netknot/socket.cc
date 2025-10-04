#include "socket.h"

using namespace netknot;

NETKNOT_API CompiledAddress::CompiledAddress() {
}

NETKNOT_API CompiledAddress::~CompiledAddress() {
}

NETKNOT_API AsyncTask::AsyncTask(AsyncTaskType taskType) : _taskType(taskType) {
}

NETKNOT_API AsyncTask::~AsyncTask() {
}

NETKNOT_API ReadAsyncTask::ReadAsyncTask() : AsyncTask(AsyncTaskType::Read) {
}

NETKNOT_API ReadAsyncTask::~ReadAsyncTask() {
}

NETKNOT_API WriteAsyncTask::WriteAsyncTask() : AsyncTask(AsyncTaskType::Write) {
}

NETKNOT_API WriteAsyncTask::~WriteAsyncTask() {
}

NETKNOT_API AcceptAsyncTask::AcceptAsyncTask() : AsyncTask(AsyncTaskType::Accept) {
}

NETKNOT_API AcceptAsyncTask::~AcceptAsyncTask() {
}

NETKNOT_API ReadAsyncCallback::ReadAsyncCallback() {
}

NETKNOT_API ReadAsyncCallback::~ReadAsyncCallback() {
}

NETKNOT_API WriteAsyncCallback::WriteAsyncCallback() {
}

NETKNOT_API WriteAsyncCallback::~WriteAsyncCallback() {
}

NETKNOT_API AcceptAsyncCallback::AcceptAsyncCallback() {
}

NETKNOT_API AcceptAsyncCallback::~AcceptAsyncCallback() {
}

NETKNOT_API Socket::Socket() {
}

NETKNOT_API Socket::~Socket() {
}

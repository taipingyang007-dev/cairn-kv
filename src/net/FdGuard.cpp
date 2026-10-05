#include "FdGuard.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

FdGuard::FdGuard(int fd) { fd_ = fd; }
FdGuard::~FdGuard() { closeFd(); }
void FdGuard::closeFd()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}
int FdGuard::getFd() const { return fd_; }
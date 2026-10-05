#pragma once

class FdGuard
{
public:
    explicit FdGuard(int fd);
    ~FdGuard();

    FdGuard(const FdGuard &) = delete;
    FdGuard &operator=(const FdGuard &) = delete;
    FdGuard(FdGuard &&) = delete;
    FdGuard &operator=(FdGuard &&) = delete;

    int getFd() const;

private:
    int fd_;
    void closeFd();
};
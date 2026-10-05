#include "net/Buffer.h"

#include <cassert>
#include <cstring> // memmove / memcpy

Buffer::Buffer() : buf_(kInitialSize), reader_index_(0), writer_index_(0) {}
const char *Buffer::peek() const { return buf_.data() + reader_index_; }
size_t Buffer::readable() const { return writer_index_ - reader_index_; }
std::string_view Buffer::readableView() const { return {peek(), readable()}; }
size_t Buffer::writable() const { return buf_.size() - writer_index_; }
size_t Buffer::capacity() const { return buf_.size(); }

void Buffer::append(const char *data, size_t len)
{
    ensureWritable(len);
    std::memcpy(buf_.data() + writer_index_, data, len);
    writer_index_ += len;
}

void Buffer::retrieve(size_t n)
{
    assert(n <= readable()); // 取多于可读是调用方的 bug，不是运行期错误
    reader_index_ += n;
    if (reader_index_ == writer_index_) // 全取完：两个下标归零，零成本回收
    {
        reader_index_ = 0;
        writer_index_ = 0;
    }
}

void Buffer::ensureWritable(size_t len)
{
    if (writable() >= len)
        return; // 尾部够放，什么都不用做

    if (reader_index_ + writable() >= len)
    {
        // 前面已取走的空间 + 尾部空闲 够用 → 把可读数据搬到最前面（延迟挪动）
        size_t n = readable();
        std::memmove(buf_.data(), buf_.data() + reader_index_, n);
        reader_index_ = 0;
        writer_index_ = n;
    }
    else
    {
        buf_.resize(writer_index_ + len); // 真不够了才长大
    }
}

#pragma once
#include <cstddef>
#include <vector>
#include <string_view>

// 输入缓冲区：字节的候车室。只管字节，不认识 RESP。
//
//   buf_:  [ 已取走 | 可读 | 空闲 ]
//          0     reader  writer  buf_.size()
//
// 回收策略：双索引 + 延迟挪动（尾部放不下、前面又空得够多时，才把可读数据搬到最前面）
class Buffer
{
public:
    static constexpr size_t kInitialSize = 1024;

    Buffer();
    ~Buffer() = default;

    void append(const char *data, size_t len); // 追加 len 个字节
    const char *peek() const;                  // 可读数据的起点，不取走
    size_t readable() const;                   // 可读字节数
    std::string_view readableView() const;
    void retrieve(size_t n); // 取走前 n 个字节
    size_t capacity() const; // 内部容量（单测用来验证回收生效）

private:
    size_t writable() const;         // 尾部空闲字节数
    void ensureWritable(size_t len); // 保证尾部至少能放下 len 字节

    std::vector<char> buf_;
    size_t reader_index_;
    size_t writer_index_;
};

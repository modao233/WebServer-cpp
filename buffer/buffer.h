#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>
#include <assert.h>
#include <string>

class Buffer{
public:
    Buffer(int init_size = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;
    size_t ReadableBytes() const;
    size_t PrependableBytes() const;

    const char *Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char *end);

    void RetrieveAll();
    std::string RetrieveAllToStr();

    const char *BeginWriteConst() const;
    char *BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* save_errno);
    ssize_t WriteFd(int fd, int* save_errno);
private:
    char *BeginPtr();
    const char *BeginPtr() const;
    void MakeSpace(size_t len);

    std::vector<char> buffer_;
    std::atomic<std::size_t> read_pos_;     //读偏移量
    std::atomic<std::size_t> write_pos_;    //写偏移量
};
#endif
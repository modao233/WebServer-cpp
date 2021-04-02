#include "buffer.h"

Buffer::Buffer(int init_size):buffer_(init_size), read_pos_(0), write_pos_(0) {}

size_t Buffer::ReadableBytes() const {
    return write_pos_ - read_pos_;
}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - write_pos_;
}

size_t Buffer::PrependableBytes() const {
    return read_pos_;
}

const char* Buffer::Peek() const {
    return BeginPtr() + read_pos_;
}

//读取了 len 个，将读指针移动 len
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    read_pos_ += len;
}

//读取到 end 处
void Buffer::RetrieveUntil(const char *end) {
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

//读取整个缓冲区，重置缓冲区，将偏移量归零处理
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    read_pos_ = 0;
    write_pos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr() + write_pos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr() + write_pos_;
}

void Buffer::HasWritten(size_t len) {
    write_pos_ += len;
}

/*
    为buffer附加更多的空间，于ReadFd中调用
 */
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len){
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str+len, BeginWrite());
    HasWritten(len);
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace(len);
    }
    assert(WritableBytes() >= len);
}

ssize_t Buffer::ReadFd(int fd, int* save_errno) {
    char buf[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    /*分散读，保证数据全部读完 */
    iov[0].iov_base = BeginPtr() + write_pos_;
    iov[0].iov_len  = writable;
    iov[1].iov_base = buf;
    iov[1].iov_len  = sizeof(buf);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *save_errno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        write_pos_ += len;
    }
    else {
        write_pos_ = buffer_.size();
        Append(buf, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* save_errno) {
    size_t read_size = ReadableBytes();
    ssize_t len = write(fd, Peek(), read_size);
    if(len < 0){
        *save_errno = errno;
        return len;
    }
    read_pos_ += len;
    return len;
}

/*
    提供缓冲区的基地址，配合偏移量，实现缓冲区读写复用
    缓冲区结构：| 预备区 | 可读区 | 可写区 |
        第一根竖线：BeginPtr()       
        第二根竖线：read_pos_
        第三根竖线：write_pos_
        第四根竖线：buffer_.size()
 */
char* Buffer::BeginPtr() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr() const {
    return &*buffer_.begin();
}

void Buffer::MakeSpace(size_t len) {
    if(WritableBytes() + PrependableBytes() < len){
        buffer_.resize(write_pos_ + len + 1);
    }
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr() + read_pos_, BeginPtr() + write_pos_, BeginPtr());
        read_pos_ = 0;
        write_pos_ = read_pos_ + readable;
        assert(readable == ReadableBytes());
    }
}
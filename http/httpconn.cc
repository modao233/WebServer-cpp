#include "httpconn.h"

const char* HttpConn::src_dir;
std::atomic<int> HttpConn::user_count;
bool HttpConn::is_ET;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    is_close_ = true;
};

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::init(int fd, const sockaddr_in &addr) {
    assert(fd > 0);
    user_count++;
    addr_ = addr;
    fd_ = fd;
    write_buff_.RetrieveAll();
    read_buff_.RetrieveAll();
    is_close_ = false;
    LOG_INFO("Client[%d](%s:%d) in, user_count: %d", fd_, GetIP(), GetPort(), (int)user_count);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(is_close_ == false) {
        is_close_ = true;
        user_count--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, user_count: %d", fd_, GetIP(), GetPort(), (int)user_count);
    }
}

int HttpConn::GetFd() const {
    return fd_;
}

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int *save_error) {
    ssize_t len = -1;
    do {
        len = read_buff_.ReadFd(fd_, save_error);
        if(len <= 0) {
            break;
        }
    }while(is_ET);
    return len;
}

ssize_t HttpConn::write(int *save_error) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iov_cnt_);
        if(len <= 0) {
            *save_error = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0) { /* 传输结束 */
            break;
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len = (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                write_buff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_len + len;
            iov_[0].iov_len -= len;
            write_buff_.Retrieve(len);
        }

    }while(is_ET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConn::process() {
    request_.Init();
    if(read_buff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(read_buff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(src_dir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else {
        response_.Init(src_dir, request_.path(), false, 400);
    }

    response_.MakeResponse(write_buff_);

    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(write_buff_.Peek());
    iov_[0].iov_len = write_buff_.ReadableBytes();
    iov_cnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iov_cnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d to %d", response_.FileLen(), iov_cnt_, ToWriteBytes());
    return true;
}
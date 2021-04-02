#ifndef HTTPCONN_H
#define HTTPCONN_H

#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>    

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockfd, const sockaddr_in& addr);

    ssize_t read (int *save_errno);
    ssize_t write(int *save_errno);

    void Close();

    int GetFd() const;
    int GetPort() const;
    const char *GetIP() const;
    sockaddr_in GetAddr() const;

    bool process();

    int ToWriteBytes() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool is_ET;
    static const char *src_dir;
    static std::atomic<int> user_count;

private:
    int fd_;
    struct sockaddr_in addr_;

    bool is_close_;

    int iov_cnt_;
    struct iovec iov_[2];

    Buffer read_buff_;
    Buffer write_buff_;

    HttpRequest request_;
    HttpResponse response_;
};

#endif
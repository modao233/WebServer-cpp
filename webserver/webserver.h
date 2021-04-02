#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "../config/config.hpp"
#include "../buffer/buffer.h"
#include "../epoller/epoller.h"
#include "../http/httpconn.h"
#include "../log/log.h"
#include "../mysql/mysqlpool.h"
#include "../threadpool/threadpool.hpp"
#include "../timer/heaptimer.h"

class WebServer {
public:
	WebServer(Config val);
	~WebServer();
	void Start();
private:
    static const int MAX_FD = 65536;

	Config config_;

    //int port_;
    //bool open_linger_;
    //int timeout_ms_;
    bool is_close_;
    int listenfd_;
    char *src_dir_;

	uint32_t listen_event_;
	uint32_t conn_event_;

	std::unique_ptr<HeapTimer> timer_;
	std::unique_ptr<ThreadPool> thread_pool_;
	std::unique_ptr<Epoller> epoller_;
	std::unordered_map<int, HttpConn> users_;

	bool InitSocket(); 
    void InitEventMode(int trigMode);
    void AddClient(int fd, sockaddr_in addr);
  
    void DealListen();
    void DealWrite(HttpConn* client);
    void DealRead(HttpConn* client);

    void SendError(int fd, const char*info);
    void ExtentTime(HttpConn* client);
    void CloseConn(HttpConn* client);

    void OnRead(HttpConn* client);
    void OnWrite(HttpConn* client);
    void OnProcess(HttpConn* client);

    static int SetFdNonblock(int fd);
};

#endif

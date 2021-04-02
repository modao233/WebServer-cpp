#include "webserver.h"

WebServer::WebServer(Config con):config_(con),
                timer_(new HeapTimer()), 
                thread_pool_(new ThreadPool(con.threadpool_num)),
                epoller_(new Epoller())
{
    // timer = new HeapTimer();
    // thread_pool = new ThreadPool(config.threadpool_num);
    // epoller = new Epoller();

    //返回工作路径
    src_dir_ = getcwd(nullptr, 256);
    assert(src_dir_);

    strncat(src_dir_, con.resources_src.c_str(), con.resources_src.size()+1);
    //strncat(src_dir_, "/resources/", 16);
    HttpConn::user_count = 0;
    HttpConn::src_dir = src_dir_;
    MysqlPool::Instance()->Init("localhost", con.mysql_port, con.mysql_user.c_str(),
        con.mysql_pass.c_str(), con.mysql_database_name.c_str(), con.connpool_num);
    
    is_close_ = false;
    InitEventMode(con.trig_mode);
    if(!InitSocket()) {
        is_close_ = true;
    }

    if(con.log_opt) {
        Log::GetInstance()->init(con.log_level, "./log", ".log", con.log_queue_size);
        if(is_close_) {
            LOG_ERROR("======== Server init error!========");
        }
        else {
            Log::GetInstance()->init(con.log_level, "./log", ".log", con.log_queue_size);
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", con.port, con.exit_opt? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listen_event_ & EPOLLET ? "ET": "LT"),
                            (conn_event_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", con.log_level);
            LOG_INFO("srcDir: %s", HttpConn::src_dir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", con.connpool_num, con.threadpool_num);
        }
    }
}

WebServer::~WebServer() {
    close(listenfd_);
    is_close_ = true;
    free(src_dir_);
    MysqlPool::Instance()->ClosePool();
}

void WebServer::InitEventMode(int trig_mode) {
    listen_event_ = EPOLLRDHUP;
    conn_event_ = EPOLLONESHOT | EPOLLRDHUP;
    switch(trig_mode)
    {
    case 0:
        break;
    case 1:
        conn_event_ |= EPOLLET;
        break;
    case 2:
        listen_event_ |= EPOLLET;
        break;
    case 3:
        listen_event_ |= EPOLLET;
        conn_event_ |= EPOLLET;
        break;
    }
    HttpConn::is_ET = (conn_event_ & EPOLLET);
}

void WebServer::Start() {
    //-1：epoll一直阻塞
    int time_ms = -1;
    if(!is_close_) {
        LOG_INFO("======== Server start =======");
    }
    while(!is_close_) {
        if(config_.timeout_ms > 0) {
            time_ms = timer_->GetNextTick();
        }
        int event_cnt = epoller_->Wait(time_ms);
        for(int i = 0; i < event_cnt; ++i) {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenfd_) {
                DealListen();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite(&users_[fd]);
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError(int fd, const char *info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_ERROR("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn(HttpConn *client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(config_.timeout_ms > 0) {
        timer_->add(fd, config_.timeout_ms, std::bind(&WebServer::CloseConn, this, &users_[fd]));//超时回调函数为关闭连接
    }
    epoller_->AddFd(fd, EPOLLIN | conn_event_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenfd_, (struct sockaddr*)&addr, &len);
        if(fd < 0) { return; }
        else if(HttpConn::user_count >= MAX_FD) {
            SendError(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient(fd, addr);
    }while(listen_event_ & EPOLLET);
}

void WebServer::DealRead(HttpConn *client) {
    assert(client);
    ExtentTime(client);
    thread_pool_->AddTask(std::bind(&WebServer::OnRead, this, client));
}

void WebServer::DealWrite(HttpConn *client) {
    assert(client);
    ExtentTime(client);
    thread_pool_->AddTask(std::bind(&WebServer::OnWrite, this, client));
}

void WebServer::ExtentTime(HttpConn *client) {
    assert(client);
    if(config_.timeout_ms > 0) {
        timer_->adjust(client->GetFd(), config_.timeout_ms);
    }
}

void WebServer::OnRead(HttpConn *client) {
    assert(client);
    int ret = -1;
    int read_error = 0;
    ret = client->read(&read_error);
    if(ret <= 0 && read_error != EAGAIN) {
        CloseConn(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn *client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
    }
    else {
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);
    }
}

void WebServer::OnWrite(HttpConn *client) {
    assert(client);
    int ret = -1;
    int write_errno = 0;
    ret = client->write(&write_errno);
    if(client->ToWriteBytes() == 0) {
        //传输完成
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(write_errno == EAGAIN) {
            epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
            return;
        }
    }
    CloseConn(client);
}


/* Create listenFd */
bool WebServer::InitSocket() {
    int ret;
    int port = config_.port;
    struct sockaddr_in addr;
    if(port > 65535 || port < 1024) {
        LOG_ERROR("Port:%d error!",  port);
        return false;
    }
    addr.sin_family = AF_INET;
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_addr.s_addr = inet_addr(config_.ip.c_str());
    addr.sin_port = htons(port);
    struct linger opt_linger = { 0 };
    if(config_.exit_opt) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        opt_linger.l_onoff = 1;
        opt_linger.l_linger = 1;
    }

    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd_ < 0) {
        LOG_ERROR("Create socket error!", port);
        return false;
    }
    //当连接中断时，需要延迟关闭(linger)以保证所有数据都被传输，所以需要打开SO_LINGER这个选项，即优雅关闭
    ret = setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
    if(ret < 0) {
        close(listenfd_);
        LOG_ERROR("Init linger error!");
        return false;
    }

    int optval = 1;
    /*一般来说，一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用 */
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    /*客户执行主动关闭并进入 TIME_WAIT ，这是正常的情况，因为服务器通常执行被动关闭，不会进入 TIME_WAIT 状态。
      如果我们终止一个客户程序，并立即重新启动这个客户程序，则这个新客户程序将不能重用相同的本地端口。
      这不会带来什么问题，因为客户使用本地端口，而并不关心这个端口号是什么。
      然而，对于服务器，情况就有所不同，因为服务器使用周知端口。
      如果我们终止一个已经建立连接的服务器程序，并试图立即重新启动这个服务器程序，
      服务器程序将不能把它的这个周知端口赋值给它的端点，因为那个端口是处于 2MSL 连接的一部分。
      在重新启动服务器程序前，它需要在 1~4 分钟。
      这就是很多网络服务器程序被杀死后不能够马上重新启动的原因（错误提示为“ Address already in use ”）。 */
    ret = setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenfd_);
        return false;
    }

    ret = bind(listenfd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port);
        close(listenfd_);
        return false;
    }
    //内核应该为相应套接字排队的最大连接个数：已完成连接+未完成连接
    ret = listen(listenfd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port);
        close(listenfd_);
        return false;
    }
    ret = epoller_->AddFd(listenfd_,  listen_event_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenfd_);
        return false;
    }
    SetFdNonblock(listenfd_);
    LOG_INFO("Server run: [%s:%d]", config_.ip, port);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
#include "mysqlpool.h"

MysqlPool::MysqlPool(){
    used_count = 0;
    free_count = 0;
}

MysqlPool* MysqlPool::Instance(){
    static MysqlPool pool;
    return &pool;
}

void MysqlPool::Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* db_name, int conn_size = 10)
{
    assert(conn_size > 0);
    for(int i = 0; i < conn_size; ++i){
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if(!sql){
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, 
                                 user, pwd,
                                 db_name, port, nullptr, 0);
        if(!sql){
            LOG_ERROR("MySql Connect error!");
        }
        conn_queue.push(sql);
    }
    max_conn = conn_size;
    sem_init(&sem_id, 0, max_conn);
}

MYSQL* MysqlPool::GetConn(){
    MYSQL *sql = nullptr;
    if(conn_queue.empty()){
        LOG_WARN("SqlConnPool Busy!");
        return nullptr;
    }
    sem_wait(&sem_id);
    {
        std::lock_guard<std::mutex> locker(mtx);
        sql = conn_queue.front();
        conn_queue.pop();
    }
    return sql;
}

void MysqlPool::FreeConn(MYSQL *sql){
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx);
    conn_queue.push(sql);
    sem_post(&sem_id);
}

void MysqlPool::ClosePool(){
    std::lock_guard<std::mutex> locker(mtx);
    while(!conn_queue.empty()){
        auto item = conn_queue.front();
        conn_queue.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int MysqlPool::GetFreeConnCount(){
    std::lock_guard<std::mutex> locker(mtx);
    return conn_queue.size();
}

MysqlPool::~MysqlPool(){
    ClosePool();
}
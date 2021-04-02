#ifndef MYSQLPOOL_H
#define MYSQLPOOL_H

#include "../log/log.h"

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>

class MysqlPool{
public:
    static MysqlPool *Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL *conn);
    int GetFreeConnCount();

void Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* db_name, int conn_size);

    void ClosePool();

private:
    MysqlPool();
    ~MysqlPool();

    int max_conn;
    int used_count;
    int free_count;

    std::queue<MYSQL*> conn_queue;
    std::mutex mtx;
    sem_t sem_id;
};
#endif
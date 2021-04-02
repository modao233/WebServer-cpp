#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

typedef struct TimerNode {
    int                 id;         //conn_fd
    TimeStamp           expires;    //连接存活期的截止时间
    TimeoutCallBack     cb;         //到期回调函数

    bool operator < (const TimerNode& t) {  //重载运算符，以用于调整堆
        return expires < t.expires;
    }
}TimerNode;

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }
    void adjust(int id, int new_expires);//调整堆
    void add(int id, int timeout, const TimeoutCallBack& cb);//添加堆结点
    void doWork(int id);
    void clear();
    void tick();
    void pop();
    int  GetNextTick();
private:
    void del(size_t i);
    void siftup(size_t i);
    bool siftdown(size_t index, size_t n);
    void SwapNode(size_t i, size_t j);

    std::vector<TimerNode> heap_;        //最小堆
    std::unordered_map<int, size_t> ref_;//存放fd到heap索引的映射，heap[ref[fd]]

};

#endif
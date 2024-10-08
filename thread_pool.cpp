#include <iostream>
#include<thread>
#include<mutex>
#include <chrono>
#include<time.h>
#include<vector>
#include<queue>
#include<future>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <condition_variable>
#include<string>
#include< shared_mutex>
using namespace std;

template<typename T>
struct safe_queue {
    // �̰߳�ȫ�Ķ�д����
    queue<T>que;
    shared_mutex _mutex;
    bool empty() {
        shared_lock<shared_mutex> read(_mutex);
        return que.empty();
    }
    auto size() {
        shared_lock<shared_mutex> read(_mutex);
        return que.size();
    }
    void push(T& t) {
        unique_lock<shared_mutex> write(_mutex);
        que.push(t);
    }
    bool pop(T& t) {
        unique_lock<shared_mutex> write(_mutex);
        if (que.empty()) { return false; }
        t = que.front();
        que.pop();
        return true;
    }
};
class ThreadPool {
private:
    class worker {//ÿ���߳�ִ�зº���worker
    public:
        ThreadPool* pool;
        worker(ThreadPool* _pool) : pool{ _pool } {}
        void operator ()() {
            while (pool->shut_down == false) {
                {
                    unique_lock<mutex> _lock;
                    pool->cv.wait(_lock, [&]()->bool {
                        if (pool->que.empty()!=true&&pool->shut_down==false)
                            return true;
                        else
                            return false;
                        });
                }
                function<void()> work;
                if (pool->que.pop(work)) {
                    work();
                }
            }
        }
    };
public:
    bool shut_down;
    vector<std::thread>threads;
    safe_queue<std::function<void()>> que;
    mutex _mutex;
    condition_variable cv;
    ThreadPool(int n) :threads(n),shut_down(false){
        for (auto& t : threads) {
            t = thread(worker(this));//�˴��º���work���β�
        }
    }
    //ÿ���̻߳�ִ�� worker::operator()���ò������� worker ���������ˣ�ʵ�ʻ����߳�ִ���ض��Ĺ�����
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    template<typename T,typename ...Args>
    auto submit(T&&f,Args&& ...args) -> std::future<decltype(f(args...))>
    {
        /*function<void()> _f= [f, args...] { f(args...); };
        packaged_task<void()> task(_f);
        function<void()> f_ = [&task]() {task(); };
        que.push(f_);
        cv.notify_one();
        return task.get_future();*/
        function<void()> func = [&f, args...]() { f(args...); };
        auto task_ptr = std::make_shared<std::packaged_task<void()>>(func);
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
            };//ָ���Ǳ�֤��������
        que.push(warpper_func);
        cv.notify_one();
        return task_ptr->get_future();
    }
    ~ThreadPool() {
        auto f = submit([]() {});
        f.get();
        shut_down = true;
        cv.notify_all(); // ֪ͨ���������й����߳�
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }
};
mutex _m;
int main()
{
    ThreadPool pool(1);
    int n = 20;
    for (int i = 1; i <= n; i++) {
        pool.submit([](int id) {
            if (id % 2 == 1) {
                this_thread::sleep_for(0.2s);
            }
            unique_lock<mutex> lc(_m);
            cout << "id : " << id << endl;
            }, i);
    }
}
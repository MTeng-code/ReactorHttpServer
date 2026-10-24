#ifndef __M_SERVER_HPP__
#define __M_SERVER_HPP__
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include <sys/eventfd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <sys/timerfd.h>
#include <signal.h>

#define BUFFER_SIZE 1024

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG
#define LOG(level, format, ...)                                                                                            \
    do                                                                                                                     \
    {                                                                                                                      \
        if (level < LOG_LEVEL)                                                                                             \
            break;                                                                                                         \
        const time_t tm = time(nullptr);                                                                                   \
        const struct tm *ltm = localtime(&tm);                                                                             \
        char time_str[32] = {0};                                                                                           \
        strftime(time_str, 31, "%Y-%m-%d %H:%M:%S", ltm);                                                                  \
        fprintf(stdout, "%p %s [%s:%d]" format "\n", (void *)pthread_self(), time_str, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
// 缓冲区模块
class Buffer
{
public:
    Buffer() : _buffer(BUFFER_SIZE), _read_idx(0), _write_idx(0) {}
    char *begin() { return &*_buffer.begin(); }
    char *end() { return &*_buffer.end(); }
    // 获取当前写位置地址
    char *WritePos()
    {
        return begin() + _write_idx;
    }
    // 获取当前读位置
    char *ReadPos()
    {
        return begin() + _read_idx;
    }
    // 获取头部空间大小
    uint64_t HeadIdleSize()
    {
        return _read_idx;
    }
    // 获取尾部空间大小
    uint64_t TailIdleSize()
    {
        return _buffer.size() - _write_idx;
    }
    // 获取可读数据大小
    uint64_t ReadableSize()
    {
        return _write_idx - _read_idx;
    }
    // 读位置向后移动
    void MoveReadPos(uint64_t len)
    {
        assert(len <= ReadableSize());
        _read_idx += len;
        if (_read_idx == _write_idx)
        {
            _read_idx = 0;
            _write_idx = 0;
        }
    }
    // 写位置向后移动
    void MoveWritePos(uint64_t len)
    {
        assert(len <= TailIdleSize());
        _write_idx += len;
    }
    // 确保空间足够(够了移动数据，不够扩容)
    void EnsureSpace(uint64_t len)
    {
        if (len > HeadIdleSize() + TailIdleSize())
        {
            _buffer.resize(_write_idx + len); // 扩容，不移动数据
            std::cout << "buffer resize to " << _buffer.size() << std::endl;
        }
        else
        {
            uint64_t readable = ReadableSize();
            std::copy(ReadPos(), WritePos(), begin());
            _read_idx = 0;
            _write_idx = readable;
        }
    }
    // 读取数据
    void Read(void *buf, uint64_t len)
    {
        // 读取数据前，确保可读数据大小大于等于len
        assert(len <= ReadableSize());
        char *b = static_cast<char *>(buf);
        std::copy(ReadPos(), ReadPos() + len, b);
    }
    // 读取并移动读位置
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadPos(len);
    }
    // 读取字符串
    std::string ReadString(uint64_t len)
    {
        assert(len <= ReadableSize());
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    // 读取并移动读位置
    std::string ReadAndPopString(uint64_t len)
    {
        assert(len <= ReadableSize());
        std::string str = ReadString(len);
        MoveReadPos(len);
        return str;
    }
    // 写入数据
    void Write(const void *data, uint64_t len)
    {
        // 写入数据前，确保尾部空间大小大于等于len
        EnsureSpace(len);
        const char *d = static_cast<const char *>(data);
        std::copy(d, d + len, WritePos());
        MoveWritePos(len);
    }
    // 写入数据并移动写位置
    // void WriteAndPush(const void* data, uint64_t len)
    // {
    //     Write(data, len);
    //     MoveWritePos(len);
    // }
    // 专门写入字符串
    void WriteString(const std::string &data)
    {
        Write(data.c_str(), data.size());
    }
    // void WriteAndPushString(const std::string &data)
    // {
    //     WriteString(data);
    //     MoveWritePos(data.size());
    // }
    // 写入缓冲区
    void WriteBuffer(Buffer &data)
    {
        Write(data.ReadPos(), data.ReadableSize());
    }

    // 查找\r\n
    char *FindCRLF()
    {
        void *res = std::memchr(ReadPos(), '\n', ReadableSize());
        return (char *)res;
    }

    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        return ReadString(pos - ReadPos() + 1);
    }

    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadPos(str.size());
        return str;
    }
    // 清空缓冲区
    void Clear()
    {
        _read_idx = 0;
        _write_idx = 0;
    }

private:
    std::vector<char> _buffer; // 使用vector<char>来存储数据
    uint64_t _read_idx;        // 读索引
    uint64_t _write_idx;       // 写索引
};

#define MAX_LISTEN 5
// 套接字模块
class Socket
{
public:
    Socket()
    {
        _sockfd = -1;
    }
    Socket(int sockfd) : _sockfd(sockfd) {}
    ~Socket() { Close(); }
    int GetSockfd() { return _sockfd; }
    // 创建套接字
    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            ERR_LOG("socket create failed");
            return false;
        }
        return true;
    }
    // 绑定套接字地址
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int ret = bind(_sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0)
        {
            ERR_LOG("bind failed!!!");
            return false;
        }
        return true;
    }
    // 开始监听
    bool Listen(int backlog = MAX_LISTEN)
    {
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("listen failed!!!");
            return false;
        }
        return true;
    }
    // 向服务器发起连接
    bool Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int ret = connect(_sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0)
        {
            ERR_LOG("bind failed!!!");
            return false;
        }
        return true;
    }
    // 接受客户端连接
    int Accept()
    {
        int newfd = accept(_sockfd, nullptr, nullptr);
        if (newfd < 0)
        {
            ERR_LOG("accept failed!!!");
            return -1;
        }
        return newfd;
    }

    // 接收数据
    // ssize_t Recv(void *buf, size_t len, int flags = 0)
    // {
    //     ssize_t ret = recv(_sockfd, buf, len, flags);
    //     if (ret <= 0)
    //     {
    //         // EAGAIN: 资源暂时不可用，非阻塞套接字没有数据可读
    //         // EWOULDBLOCK: 非阻塞套接字没有数据可读
    //         if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
    //         {
    //             return 0;
    //         }
    //         ERR_LOG("recv failed!!!");
    //         return -1;
    //     }
    //     return ret; // 返回实际接收的字节数
    // }
    ssize_t Recv(void *buf, size_t len, int flags = 0)
    {
        ssize_t ret = recv(_sockfd, buf, len, flags);

        if (ret > 0)
        {
            return ret;
        }

        // 对端正常关闭连接
        if (ret == 0)
        {
            return 0;
        }

        // 非阻塞套接字暂时没有数据
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -2; // 单独表示暂时无数据
        }

        // 系统调用被信号中断
        if (errno == EINTR)
        {
            return -2; // 也可以选择重新调用 recv()
        }

        ERR_LOG("recv failed: errno=%d", errno);
        return -1;
    }
    // 非阻塞接收数据
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // 如果现在没有数据，不要卡住等待，立刻返回。
    }
    // 发送数据
    ssize_t Send(const void *buf, size_t len, int flags = 0)
    {
        ssize_t ret = send(_sockfd, buf, len, flags);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0; // 非阻塞套接字发送缓冲区已满，无法发送数据
            }
            ERR_LOG("send failed!!!");
            return -1;
        }
        return ret; // 返回实际发送的字节数
    }
    // 非阻塞发送数据
    ssize_t NonBlockSend(const void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT); // 如果现在不能发送，不要卡住等待，立刻返回。
    }

    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    // 创建一个服务端连接
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0")
    {
        if (!Create())
            return false;
        // NonBlock();
        ReuseAddress();
        if (!Bind(ip, port))
            return false;
        if (!Listen())
            return false;
        return true;
    }
    // 创建一个客户端连接
    bool CreateClient(const std::string &ip, uint16_t port)
    {
        if (!Create())
            return false;
        // NonBlock();
        if (!Connect(ip, port))
            return false;
        return true;
    }
    // 设置套接字选项--开启地址端口重用
    void ReuseAddress()
    {
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        // SO_REUSEADDR方便服务器重启后重新绑定地址。
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
        // SO_REUSEPORT 允许多个 socket 共同使用同一个监听端口。
    }
    // 设置套接字阻塞属性--设置为非阻塞
    void NonBlock()
    {
        int flags = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK); // O_NONBLOCK,把整个 socket 设置为非阻塞
    }
    // ReuseAddress()：设置 socket 的地址/端口重用属性，
    // 主要解决服务器重启、多个监听 socket 共用端口等问题。

    // NonBlock()：把 socket 改成非阻塞模式，使 accept/recv/send/connect
    // 等操作在暂时无法完成时不会一直卡住线程，这正是 epoll + Reactor 高并发模型的重要基础。

private:
    int _sockfd;
};

class Poller;
class EventLoop;
class Channel
{
private:
    int _fd;
    EventLoop *_loop;  // 事件循环对象
    uint32_t _envents; // 需要监控的事件
    uint32_t _revents; // 当前触发的事件
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 读事件回调函数
    EventCallback _write_callback; // 写事件回调函数
    EventCallback _error_callback; // 错误事件回调函数
    EventCallback _close_callback; // 关闭事件回调函数
    EventCallback _enevt_callback; // 任意 事件回调函数
public:
    Channel(EventLoop *loop, int fd) : _loop(loop), _fd(fd), _envents(0), _revents(0) {}
    int Fd() { return _fd; }
    void SetRevents(uint32_t revents) { _revents = revents; }
    uint32_t Events() { return _envents; }
    void SetReadCallback(const EventCallback cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback cb) { _write_callback = cb; }
    void SetErrorCallback(const EventCallback cb) { _error_callback = cb; }
    void SetCloseCallback(const EventCallback cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback cb) { _enevt_callback = cb; }
    // 是否监控了可读
    bool ReadAble() { return _envents & EPOLLIN; }
    // 是否监控了可写
    bool WriteAble() { return _envents & EPOLLOUT; }
    // 启动读事件监控
    void EnableRead()
    {
        _envents |= EPOLLIN;
        Update();
    }
    // 启动写事件监控
    void EnableWrite()
    {
        _envents |= EPOLLOUT;
        Update();
    }
    // 禁止读事件监控
    void DisableRead()
    {
        _envents &= ~EPOLLIN;
        Update();
    }
    // 禁止写事件监控
    void DisableWrite()
    {
        _envents &= ~EPOLLOUT;
        Update();
    }
    // 关闭所有事件监控
    void DisableAll()
    {
        _envents = 0;
        Update();
    }
    // 移除监控
    void Remove();
    // 更新监控
    void Update();
    // 连接触发了事件，调用对应的回调函数
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLPRI) || (_revents & EPOLLRDHUP))
        {
            // 任何事件都调用的函数,读完调用一次
            if (_read_callback)
                _read_callback();
        }
        if ((_revents & EPOLLOUT))
        {
            // 任何事件都调用的函数
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback)
                _error_callback();
        }
        else if (_revents & EPOLLHUP)
        {

            if (_close_callback)
                _close_callback();
        }
        if (_enevt_callback)
            _enevt_callback();
    }
};

#define MAX_EOPLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EOPLLEVENTS];
    std::unordered_map<int, Channel *> _channels; // fd -> channel
private:
    // 把channel里设置的监控事件，添加到epoll中
    void Update(Channel *channel, uint32_t op)
    {
        int fd = channel->Fd();
        struct epoll_event ev{};
        ev.data.fd = fd;
        // int epoll_ctl( int epfd, int op, int fd, struct epoll_event *event);
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG(
                "epoll_ctl failed: epfd=%d op=%u fd=%d errno=%d error=%s",
                _epfd,
                op,
                fd,
                errno,
                strerror(errno));
            abort(); // 直接退出程序
        }
    }
    // 判断一个channel是否已经在epoll中注册过了
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
            return true;
        return false;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EOPLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("epoll_create failed!!!");
            abort();
        }
    }
    // 更新或添加一个channel到epoll中
    void UpdateChannel(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret)
        {
            Update(channel, EPOLL_CTL_MOD);
        }
        else
        {
            _channels[channel->Fd()] = channel;
            Update(channel, EPOLL_CTL_ADD);
        }
    }
    // 从epoll中删除一个channel
    void RemoveEvent(Channel *channel)
    {
        if (!HasChannel(channel))
        {
            ERR_LOG("channel not found in epoll");
            return;
        }
        Update(channel, EPOLL_CTL_DEL);
        _channels.erase(channel->Fd());
    }
    // 开始监控，返回活跃连接.返回这一次已经发生事件的 Channel
    void Poll(std::vector<Channel *> &active)
    {
        int nfds = epoll_wait(_epfd, _evs, MAX_EOPLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR) // 被信号中断
            {
                return;
            }
            ERR_LOG("epoll_wait failed!!!");
            abort();
        }
        for (int i = 0; i < nfds; i++)
        {
            int fd = _evs[i].data.fd;
            auto it = _channels.find(fd);
            assert(it != _channels.end());
            it->second->SetRevents(_evs[i].events);
            active.push_back(it->second);
        }
    }
};

using Taskfunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
// 定时任务类
class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t delay, const Taskfunc &cb) : _id(id),
                                                                 _timeout(delay),
                                                                 _task_cb(cb),
                                                                 _canceled(false)
    {
    }

    void SetRelease(const ReleaseFunc &cb)
    {
        _release = cb;
    }

    uint32_t DelayTime()
    {
        return _timeout;
    }

    void Cancel()
    {
        _canceled = true;
    }

    ~TimerTask()
    {
        if (!_canceled)
        {
            _task_cb();
        }
        _release();
    }

private:
    uint64_t _id;         // 任务id
    uint32_t _timeout;    // 任务超时时间
    bool _canceled;       // 任务是否被取消,true表示取消,false表示未取消
    Taskfunc _task_cb;    // 任务回调函数
    ReleaseFunc _release; // 删除Timerwheel中保存的定时器对象信息
};

class TimerWheel
{
private:
    using PtrTask = std::shared_ptr<TimerTask>;
    using WeakTask = std::weak_ptr<TimerTask>;
    int _capacity;                                  // 时间轮的容量
    int _tick;                                      // 时间轮的刻度
    std::unordered_map<uint64_t, WeakTask> _timers; // 任务id和任务的映射关系
    std::vector<std::vector<PtrTask>> _wheel;       // 时间轮的槽

    EventLoop *_loop;
    int _timerfd;
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

    static int Createtimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            perror("timerfd_create");
            return 1;
        }

        struct itimerspec itime;
        itime.it_value.tv_sec = 1; // 设置初始到期时间为1秒
        itime.it_value.tv_nsec = 0;
        itime.it_interval.tv_sec = 1; // 设置间隔时间为1秒
        itime.it_interval.tv_nsec = 0;
        // 启动定时器
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }

    int ReadTimerfd()
    {
        uint64_t times;
        ssize_t ret = read(_timerfd, &times, sizeof(times));
        if (ret < 0)
        {
            ERR_LOG("read timerfd failed");
            abort();
        }
        return times;
    }

    void RunTimerTask() // 执行定时任务
    {
        // std::cout << "RunTimerTask tick = " << _tick << std::endl;
        _tick = (_tick + 1) % _capacity; // 时间轮的刻度加1
        _wheel[_tick].clear();           // 清空当前刻度的槽
    }

    void OnTime()
    {
        // 根据实际超时的次数，执行对应的超时任务
        int times = ReadTimerfd();
        for (int i = 0; i < times; i++)
        {
            RunTimerTask();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const Taskfunc &cb) // 添加定时任务
    {

        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id)); // 设置回调函数
        _timers[id] = pt;                                              // 将任务id和任务对象保存到映射关系中
        _wheel[(_tick + delay) % _capacity].push_back(pt);             // 将任务对象添加到时间轮中
    }

    void TimerReflashInLoop(uint64_t id) // 刷新定时任务
    {
        // 通过保存在定时器对象_timers中的weak_ptr构造一个shared_ptr对象,添加到轮中
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            // std::cout << "TimerReflashInLoop: " << id << std::endl;
            PtrTask pt = it->second.lock(); // weak_ptr::lock() 的作用是安全地尝试获取
                                            // 一个 shared_ptr；对象存在就返回有效指针，对象已经销毁就返回空指针。
            if (!pt)
            {
                std::cout << "TimerTask已经失效！" << std::endl;
                _timers.erase(it);
                return;
            }
            int delay = pt->DelayTime();

            _wheel[(_tick + delay) % _capacity].push_back(pt);
        }
    }

    void TimerCancelInLoop(uint64_t id) // 取消定时任务
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            PtrTask pt = it->second.lock();

            std::cout << "CancelTimer id=" << id
                      << " pt=" << pt.get()
                      << std::endl;
            if (!pt)
            {
                std::cout << "TimerTask已经失效！" << std::endl;
                _timers.erase(it);
                return;
            }

            pt->Cancel(); // 设置定时器对象的_canceled为true,表示取消定时器
            // _timers.erase(it);
        }
    }

    bool _HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        return it != _timers.end();
    }

public:
    TimerWheel(EventLoop *loop) : _loop(loop),
                                  _capacity(60),
                                  _tick(0),
                                  _wheel(_capacity),
                                  _timerfd(Createtimerfd()),
                                  _timer_channel(new Channel(loop, _timerfd))

    {
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));

        _timer_channel->EnableRead(); // 启动读事件监控
    }
    // 保证所有timer定时器操作，都在eventloop线程中执行，避免多线程操作定时器带来的线程安全问题
    void TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb); // 添加定时任务

    void TimerReflash(uint64_t id); // 添加定时任务

    void TimerCancel(uint64_t id); // 添加定时任务

    bool HasTimer(uint64_t id);
};

class EventLoop
{
    using Functor = std::function<void()>;

public:
    EventLoop() : _thread_id(std::this_thread::get_id()), // 获取当前线程的id
                  _eventfd(CreatEventfd()),
                  _eventfd_channel(new Channel(this, _eventfd)),
                  _timer_wheel(this)

    {
        _eventfd_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this)); // 设置事件fd的读事件回调函数,读取事件fd的值，清空事件fd的状态
        _eventfd_channel->EnableRead();                                              // 启动读事件监控
    }

    void Start() // 事件监控-》就绪事件处理-》执行任务
    {
        while (true)
        {
            // 事件监控
            std::vector<Channel *> actives;
            _poller.Poll(actives);
            // 事件处理
            for (auto channel : actives)
            {
                channel->HandleEvent();
            }
            // 执行任务
            RunAllTask(); // 执行任务队列中的所有任务
        }
    }
    bool IsInLoopThread() // 判断是否在当前线程
    {
        return std::this_thread::get_id() == _thread_id;
    }
    void AssertInLoopThread() // 断言是否在当前线程
    {
        if (!IsInLoopThread())
        {
            DBG_LOG(
                "thread id = %zu\n",
                std::hash<std::thread::id>{}(std::this_thread::get_id()));
            abort();
        }
    }
    void RunInLoop(const Functor &cb) // 判断要执行的任务是否处于当前线程，是则执行，不是则压入任务队列
    {
        if (IsInLoopThread())
        {
            cb();
        }
        else
        {
            QueueInLoop(cb);
        }
    }
    void QueueInLoop(const Functor &cb) // 压入队列
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        WakeupEventfd(); // 唤醒线程，向事件fd写入数据，触发读事件
    }

    void UpdateEvent(Channel *channel) // 更新事件
    {
        _poller.UpdateChannel(channel);
    }
    void RemoveEvent(Channel *channel) // 移除事件
    {
        _poller.RemoveEvent(channel);
    }
    void AddTimer(uint64_t id, uint32_t delay, const Taskfunc &cb) // 添加定时任务
    {
        _timer_wheel.TimerAdd(id, delay, cb);
    }
    void DelayTimer(uint64_t id)
    {
        _timer_wheel.TimerReflash(id);
    }
    void CancelTimer(uint64_t id)
    {
        _timer_wheel.TimerCancel(id);
    }
    bool HasTimer(uint64_t id)
    {
        return _timer_wheel.HasTimer(id);
    }

public:
    void RunAllTask() // 执行任务队列中的所有任务
    {
        std::vector<Functor> tasks;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            tasks.swap(_tasks);
        }
        for (const auto &task : tasks)
        {
            task();
        }
    }
    static int CreatEventfd()
    {
        int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (efd < 0)
        {
            ERR_LOG("eventfd failed");
            abort();
        }
        return efd;
    }
    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = read(_eventfd, &res, sizeof(res));
        if (ret < 0)
        {
            // EAGAIN: 资源暂时不可用，非阻塞套接字没有数据可读
            // EWOULDBLOCK: 非阻塞套接字没有数据可读
            // EINTR: 被中断的系统调用
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                return;
            }
            ERR_LOG("read eventfd failed");
            abort();
        }
    }
    void WakeupEventfd() // 唤醒线程，向事件fd写入数据，触发读事件
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
            {
                return;
            }
            ERR_LOG("write eventfd failed");
            abort();
        }
    }

private:
    std::thread::id _thread_id;
    int _eventfd;                              // 用于唤醒线程的事件fd
    Poller _poller;                            // 事件循环对象,进行epoll_wait监控事件
    std::unique_ptr<Channel> _eventfd_channel; // 事件fd对应的channel对象
    std::mutex _mutex;                         // 互斥锁，用于保护任务队列的访问
    std::vector<Functor> _tasks;               // 任务队列

    TimerWheel _timer_wheel; // 定时器轮对象
};

class LoopThread
{
private:
    EventLoop *_loop;              // 事件循环对象，必须在LoopThread的线程中创建·
    std::thread _thread;           // 线程对象
    std::mutex _mutex;             // 互斥锁，用于保护事件循环对象的访问
    std::condition_variable _cond; // 条件变量，用于等待事件循环对象的创建
private:
    void ThreadEntry() // 线程入口函数,实例化eventloop对象，并启动事件循环
    {
        EventLoop loop; // 在当前线程中创建事件循环对象
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all(); // 通知等待的线程，事件循环对象已经创建
        }
        loop.Start(); // 启动事件循环
    }

public:
    LoopThread() : _loop(nullptr), _thread(std::thread(&LoopThread::ThreadEntry, this)) {}
    // ~LoopThread();
    EventLoop *GetLoop()
    {
        EventLoop *Loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this]()
                       { return _loop != nullptr; }); // 等待事件循环对象的创建
            Loop = _loop;
        }
        return Loop;
    }
};

class LoopThreadPool
{
public:
    LoopThreadPool(EventLoop *base_loop) : _base_loop(base_loop),
                                           _thread_count(0),
                                           _next_index(0),
                                           _threads()
    {
    }
    // 设置线程池中线程的数量
    void SetThreadCount(int count)
    {
        _thread_count = count;
    }

    // 创建线程池中的线程对象，并启动线程
    void CreateThreads()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            for (int i = 0; i < _thread_count; ++i)
            {
                _threads[i].reset(new LoopThread());
                _loops[i] = _threads[i]->GetLoop();
            }
        }
    }

    // 获取下一个线程的事件循环对象，采用轮询的方式
    EventLoop *GetNextLoop()
    {
        if (_thread_count == 0)
        {
            return _base_loop;
        }
        _next_index = (_next_index + 1) % _loops.size();
        return _loops[_next_index];
    }

private:
    int _thread_count;                                 // 线程池中线程的数量
    int _next_index;                                   // 线程池中当前线程的索引
    EventLoop *_base_loop;                             // 主线程中的事件循环对象
    std::vector<std::unique_ptr<LoopThread>> _threads; // 线程池中的线程对象
    std::vector<EventLoop *> _loops;                   // 线程池中每个线程的事件循环对象
};

class Any
{
private:
    class holder
    {
    public:
        virtual ~holder() {};
        virtual const std::type_info &type() = 0;
        virtual holder *clone() const = 0;
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}

        virtual const std::type_info &type() // 获取子类中保存的数据类型
        {
            return typeid(T);
        }
        virtual holder *clone() const // 针对当前对象进行深拷贝，返回一个新的holder对象
        {
            return new placeholder(_val);
        }

    public:
        T _val;
    };

    holder *_content;

public:
    Any() : _content(nullptr) {}

    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {}
    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {}
    ~Any() { delete _content; }

    // 返回子类对象中保存的数据指针
    template <class T>
    T *get() // 获取保存的数据
    {
        // 获取的数据类型必须和子类中保存的数据类型一致，否则返回nullptr
        assert(_content->type() == typeid(T));
        // if(_content->type() != typeid(T))
        // {
        //     return nullptr;
        // }
        return &((placeholder<T> *)_content)->_val;
    }

    template <class T>
    Any &operator=(const T &val)
    {
        Any tmp(val);
        std::swap(_content, tmp._content);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any temp(other);
        std::swap(_content, temp._content);
        return *this;
    }
};

typedef enum
{
    DISCONNECTED = 0, // 连接断开
    CONNECTING = 1,   // 连接中
    CONNECTED = 2,    // 连接建立
    DISCONNECTING = 3 // 连接断开中
} ConnStatus;
class Connection;
using PtrConnection = std::shared_ptr<Connection>;
using ConnectionCallback = std::function<void(const PtrConnection &)>;
using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
using ClosedCallback = std::function<void(const PtrConnection &)>;
using AnyEventCallback = std::function<void(const PtrConnection &, uint32_t)>;

class Connection : public std::enable_shared_from_this<Connection>
{
private:
    // 描述符可读事件触发后调用函数，接受socket数据放到缓冲区中，然后调用_message_callback回调函数
    void HandleRead() // 处理读事件，读取数据到读缓冲区，调用消息回调函数
    {
        // 接收socket数据，放入读缓冲区
        char buf[65535];
        ssize_t ret = _socket.NonBlockRecv(buf, sizeof(buf));
        if (ret < 0)
        {
            // 读取数据失败，关闭连接,但不直接关闭
            ShutdownInLoop();
            return;
        }
        _in_buffer.Write(buf, ret);
        if (_in_buffer.ReadableSize() > 0)
        {
            if (_message_callback)
            {
                // std::shared_ptr<Connection> conn;管理当前connection，
                // 正在这个 Connection 自己的成员函数里,回调要求的是：const PtrConnection&
                // 不能直接：_message_callback(this, &_in_buffer); // 类型不匹配
                // 需要：shared_from_this()把：当前这个Connection对象转换成一个共享所有权的 shared_ptr<Connection>
                _message_callback(shared_from_this(), &_in_buffer);
            }
        }
    }

    void HandleWrite() // 处理写事件，将写缓冲区的数据发送出去，调用写事件回调函数
    {

        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPos(), _out_buffer.ReadableSize());
        if (ret < 0)
        {
            // 发送错误了，就关闭连接
            Release();
            return;
        }
        _out_buffer.MoveReadPos(ret);
        if (_out_buffer.ReadableSize() == 0)
        {
            _channel.DisableWrite(); // 禁止写事件监控
            if (_status == DISCONNECTING)
            {
                // 如果连接正在断开中，并且写缓冲区已经发送完了，就关闭连接
                Release();
            }
        }
    }
    void HandleClose() // 处理关闭事件，关闭连接，调用关闭回调函数
    {
        // 为什么不判断outbuffer，因为这里一般表示对端已经挂了，如果outbuffer还有数据没有发送完，就不应该关闭连接，应该等到数据发送完再关闭连接。
        if (_in_buffer.ReadableSize() > 0)
        {
            if (_message_callback)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
        }
        Release();
    }
    void HandleError() // 处理错误事件，关闭连接，调用关闭回调函数
    {
        HandleClose();
    }
    // 1.刷新连接的活跃度，2.调用组件使用者的任意时间回调
    void HandleEvent() // 处理任意事件，调用任意事件回调函数
    {
        if (_enable_inactive_release)
        {
            // 刷新连接的活跃度
            _loop->DelayTimer(_conn_id);
        }
        if (_any_event_callback)
        {
            _any_event_callback(shared_from_this(), _channel.Events());
        }
    }

    void EstablishedInLoop() // 连接建立，设置连接状态为已连接，启动读事件监控，调用连接建立回调函数
    {
        // 1. 修改连接状态； 2.启动读事件监控； 3.调用连接建立回调函数
        assert(_status == CONNECTING); // 必须是上层的半连接状态
        _status = CONNECTED;           // 修改连接状态为已连接
        _channel.EnableRead();         // 启动读事件监控
        if (_connection_callback)
        {
            _connection_callback(shared_from_this());
        }
    }

    void ReleaseInLoop() // 释放连接资源，关闭连接，释放缓冲区，取消事件监控
    {
        // chatgpt修改
        PtrConnection self = shared_from_this(); // 获取当前对象的shared_ptr，防止在回调函数中被释放
        // 修改连接状态，将其置为DISCONNECTED，
        if (_status == DISCONNECTED)
        {
            return; // 如果连接已经断开，就不需要再释放连接资源了
        }
        _status = DISCONNECTED;
        // 先从epoll和Poller中删除channel，避免在回调函数中再次触发事件
        _channel.Remove();
        // 移除连接事件监控
        // _channel.DisableAll();
        // 关闭套接字
        _socket.Close();
        // 如果当前定时器队列中还有定时销毁任务，则取消定时销毁任务
        if (_loop->HasTimer(_conn_id))
            CancelInavtiveReleaseInLoop();
        // 调用组件使用者的连接关闭回调函数，先调用组件使用者的回调函数，再调用组件内的回调函数，保证组件使用者可以在回调函数中获取到连接对象
        if (_closed_callback)
        {
            _closed_callback(shared_from_this());
        }
        // 移除服务器内部管理的连接信息
        if (_server_closed_callback)
        {
            _server_closed_callback(shared_from_this());
        }
    }
    void SendInLoop(Buffer &buf) // 发送数据，将数据放到写缓冲区中，等待发送,并非发送给对端
    {
        if (_status == DISCONNECTED)
            return; // 如果连接已经断开，就不需要再发送数据了
        _out_buffer.WriteBuffer(buf);
        if (!_channel.WriteAble())
        {
            _channel.EnableWrite(); // 启动写事件监控
        }
    }
    void ShutdownInLoop() // 提供给外部调用的关闭连接接口，关闭连接，但并不实际关闭，需要判断有没有数据待处理
    {
        if (_status == DISCONNECTED)
            return;              // 如果连接已经断开，就不需要再关闭连接了
        _status = DISCONNECTING; // 修改连接状态为断开中
        if (_in_buffer.ReadableSize() > 0)
        {
            if (_message_callback)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
        }
        if (_out_buffer.ReadableSize() > 0)
        {
            if (!_channel.WriteAble())
            {
                _channel.EnableWrite(); // 启动写事件监控
            }
        }
        if (_out_buffer.ReadableSize() == 0)
        {
            Release(); // 如果写缓冲区没有数据，就直接释放连接资源
        }
    }
    void EnableInavtiveReleaseInLoop(int delay) // 启用非活动连接释放，设置超时时间，超过时间没有数据交互就释放连接
    {
        _enable_inactive_release = true;
        // 存在则刷新定时器，不存在则添加定时器
        if (_loop->HasTimer(_conn_id))
        {
            _loop->DelayTimer(_conn_id); // 刷新定时器
        }
        else
        {
            _loop->AddTimer(_conn_id, delay, std::bind(&Connection::Release, this));
        }
    }
    void CancelInavtiveReleaseInLoop() // 取消非活动连接释放，
    {
        _enable_inactive_release = false;
        if (_loop->HasTimer(_conn_id))
            _loop->CancelTimer(_conn_id); // 取消定时器
    }
    void UpgradeInLoop(const Any &context,
                       const ConnectionCallback &connection_cb,
                       const MessageCallback &message_cb,
                       const ClosedCallback &closed_cb,
                       const AnyEventCallback &any_event_cb) // 切换协议--重置上下文以及阶段性处理函数
    {
        _context = context;
        _connection_callback = connection_cb;
        _message_callback = message_cb;
        _closed_callback = closed_cb;
        _any_event_callback = any_event_cb;
    }

public:
    Connection(EventLoop *loop, int sockfd, uint64_t conn_id) : _conn_id(conn_id),
                                                                // _time_id(conn_id),
                                                                _sockfd(sockfd),
                                                                _socket(sockfd),
                                                                _enable_inactive_release(false),
                                                                _loop(loop),
                                                                _status(CONNECTING),
                                                                _in_buffer(),
                                                                _out_buffer(),
                                                                _channel(loop, sockfd),
                                                                _context()
    {
        // 设置连接的事件回调函数
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
    }
    ~Connection()
    {
        DBG_LOG("Connection::~Connection() %p", this);
    }
    int Fd()
    {
        return _sockfd;
    }
    int Id()
    {
        return _conn_id;
    }
    bool IsConnected() // 判断连接是否建立
    {
        return _status == CONNECTED;
    }
    void SetContext(const Any &context) // 设置连接的上下文信息，可以存储任意类型的数据
    {
        _context = context;
    }
    Any *GetContext() // 获取连接的上下文信息，可以存储任意类型的数据
    {
        return &_context;
    }
    void SetConnectionCallback(const ConnectionCallback &cb)
    {
        _connection_callback = cb;
    }
    void SetMessageCallback(const MessageCallback &cb)
    {
        _message_callback = cb;
    }
    void SetClosedCallback(const ClosedCallback &cb)
    {
        _closed_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb)
    {
        _any_event_callback = cb;
    }
    void SetServerClosedCallback(const ClosedCallback &cb)
    {
        _server_closed_callback = cb;
    }
    void Established() // 连接建立，设置连接状态为已连接，启动读事件监控，调用连接建立回调函数
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
        // codex修改
        // _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, shared_from_this()));
    }
    void Send(const char *data, size_t len) // 发送数据，将数据放到写缓冲区中，等待发送
    {
        Buffer buf;
        buf.Write(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
    }
    void Shutdown() // 提供给外部调用的关闭连接接口，关闭连接，但并不实际关闭，需要判断有没有数据待处理
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }
    void Release() // 释放连接资源，关闭连接，释放缓冲区，取消事件监控
    {
        _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }
    void EnableInavtiveRelease(int delay) // 启用非活动连接释放，设置超时时间，超过时间没有数据交互就释放连接
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInavtiveReleaseInLoop, this, delay));
    }
    void CancelInavtiveRelease() // 取消非活动连接释放，取消超时时间，连接不会被释放
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInavtiveReleaseInLoop, this));
    }
    // 切换协议--重置上下文以及阶段性处理函数
    // 这个接口必须在eventloop线程中调用，保证线程安全
    // 防备新事件触发后，处理的时候，切换任务还没有被执行，会导致数据使用原协议处理
    void Upgrade(const Any &context,
                 const ConnectionCallback &connection_cb,
                 const MessageCallback &message_cb,
                 const ClosedCallback &closed_cb,
                 const AnyEventCallback &any_event_cb)
    {
        _loop->AssertInLoopThread();
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, connection_cb, message_cb, closed_cb, any_event_cb));
    }

private:
    uint64_t _conn_id; // 连接id,他们都唯一标识，所以可以使用同一个id
    // uint64_t _time_id;             // 定时器id,用于非活动连接释放，
    // 他们都唯一标识，所以可以使用同一个id
    int _sockfd;                   // 套接字描述符
    Socket _socket;                // 套接字对象
    bool _enable_inactive_release; // 是否启用非活动连接释放
    EventLoop *_loop;              // 事件循环对象
    ConnStatus _status;            // 连接状态
    Buffer _in_buffer;             // 读缓冲区
    Buffer _out_buffer;            // 写缓冲区
    Channel _channel;              // 连接的事件管理
    Any _context;                  // 连接的上下文信息，可以存储任意类型的数据

    // 由组件使用者来设置的回调函数

    ConnectionCallback _connection_callback; // 连接建立回调函数
    MessageCallback _message_callback;       // 消息到达回调函数
    ClosedCallback _closed_callback;         // 连接关闭回调函数
    AnyEventCallback _any_event_callback;    // 任意事件回调函数
    // 组件内的连接关闭回调
    ClosedCallback _server_closed_callback; // 连接关闭回调函数,由组件内使用，释放连接资源
};

class Acceptor
{
    using AcceptCallback = std::function<void(int)>;

public:
    Acceptor(EventLoop *loop, int port) : _loop(loop),
                                          _listen_sock(CreateListenSocket(port)),
                                          _accept_channel(loop, _listen_sock.GetSockfd())
    {
        _accept_channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }
    void SetAcceptCallback(const AcceptCallback &cb)
    {
        _accept_callback = cb;
    }
    void Listen()
    {
        _accept_channel.EnableRead(); // 启动读事件监控
    }

private:
    // Accepter类的实现
    Socket _listen_sock; // 监听套接字
    EventLoop *_loop;
    Channel _accept_channel;         // 监听套接字的事件管理
    AcceptCallback _accept_callback; // 新连接到达回调函数
private:
    // 目的：处理新连接到达的逻辑，调用组件使用者设置的回调函数
    void HandleRead()
    {
        // 处理新连接到达的逻辑
        int newfd = _listen_sock.Accept();
        if (newfd >= 0)
        {
            if (_accept_callback)
            {
                _accept_callback(newfd);
            }
        }
    }
    // 目的：创建一个监听套接字，绑定端口，开始监听，并返回套接字描述符
    int CreateListenSocket(int port)
    {
        bool ret = _listen_sock.CreateServer(port);
        assert(ret);
        return _listen_sock.GetSockfd();
    }
};

class TcpServer
{
public:
    TcpServer(int port) : _next_conn_id(0),
                          _next_time_id(0),
                          _port(port),
                          _acceptor(&_base_loop, port),
                          _thread_pool(&_base_loop),
                          _timeout(0),
                          _enable_inactive_release(false)
    {

        _acceptor.SetAcceptCallback(std::bind(&TcpServer::HandleNewConnection, this, std::placeholders::_1));
    }

    void SetThreadCount(int count)
    {
        _thread_pool.SetThreadCount(count);
    }
    void SetConnectionCallback(const ConnectionCallback &cb)
    {
        _connection_callback = cb;
    }
    void SetMessageCallback(const MessageCallback &cb)
    {
        _message_callback = cb;
    }
    void SetClosedCallback(const ClosedCallback &cb)
    {
        _closed_callback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback &cb)
    {
        _any_event_callback = cb;
    }
    void EnableInactivateRelease(int timeout)
    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }
    // 添加定时任务
    void RunAfter(const Taskfunc &task, int delay)
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
    }

    void Start()
    {
        _thread_pool.CreateThreads();
        _acceptor.Listen();
        _base_loop.Start();
    }

private:
    void HandleNewConnection(int sockfd)
    {
        uint64_t conn_id = _next_conn_id++;
        PtrConnection conn(new Connection(_thread_pool.GetNextLoop(), sockfd, conn_id));
        conn->SetConnectionCallback(_connection_callback);
        conn->SetMessageCallback(_message_callback);
        conn->SetClosedCallback(_closed_callback);
        conn->SetAnyEventCallback(_any_event_callback);
        conn->SetServerClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));

        if (_enable_inactive_release)
        {
            conn->EnableInavtiveRelease(_timeout); // 启用非活动连接释放，设置超时时间为5秒
        }
        // conn->EnableInavtiveRelease(5); // 启用非活动连接释放，设置超时时间为5秒
        // conn->SetTimerId(_next_time_id++);

        _conns.insert(std::make_pair(conn_id, conn)); // 将连接对象存储到映射关系中
        conn->Established();                          // 启动读事件监控
    }

    void RemoveConnectionInLoop(const PtrConnection &conn)
    {
        auto it = _conns.find(conn->Id());
        if (it != _conns.end())
        {
            _conns.erase(it);
        }
        // _conns.erase(conn->Id());
    }
    void RemoveConnection(const PtrConnection &conn)
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
    }
    void RunAfterInLoop(const Taskfunc &task, int delay)
    {
        uint64_t time_id = ++_next_time_id;
        _base_loop.AddTimer(time_id, delay, task);
    }

private:
    uint64_t _next_conn_id;                             // 连接id,
    uint64_t _next_time_id;                             // // TcpServer普通定时任务的timer id
    int _port;                                          // 服务器监听的端口号
    EventLoop _base_loop;                               // 主线程的事件循环对象,负责监听事件的处理
    Acceptor _acceptor;                                 // 监听套接字的事件管理
    LoopThreadPool _thread_pool;                        // 线程池对象
    std::unordered_map<uint64_t, PtrConnection> _conns; // 连接id和连接对象的映射关系
    int _timeout;                                       // 连接超时时间，单位秒
    bool _enable_inactive_release;                      // 是否启用非活动连接释放

    ConnectionCallback _connection_callback; // 连接建立回调函数
    MessageCallback _message_callback;       // 消息到达回调函数
    ClosedCallback _closed_callback;         // 连接关闭回调函数
    AnyEventCallback _any_event_callback;    // 任意事件回调函数
};

void Channel::Remove()
{
    _loop->RemoveEvent(this);
}
// 更新监控
void Channel::Update()
{
    _loop->UpdateEvent(this);
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const Taskfunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}

void TimerWheel::TimerReflash(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerReflashInLoop, this, id));
}

void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

bool TimerWheel::HasTimer(uint64_t id)
{
    return _loop->IsInLoopThread() ? _HasTimer(id) : false;
}

class NetWork
{
public:
    NetWork()
    {
        signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号，防止写入已关闭的socket时程序崩溃
    }
};
static NetWork g_network; // 全局对象，构造函数中忽略SIGPIPE信号
#endif
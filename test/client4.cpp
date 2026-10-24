// 业务处理超时（当服务器达到性能瓶颈， 业务处理时间过长时，可能会出现超时的情况）
// 1.在一次业务处理中， 耗费太长时间，导致其他链接也被连累超时
//    假设12345描述符就绪了，在处理1的时候花费了30秒，导致2、3、4、5都超时了
//    如果接下来2345描述符都是通信连接描述符，如果都就绪了，就并不影响，因为接下来就会进行处理并刷新活跃度
//    如果2号描述符是定时器时间描述符，定时器触发超时， 执行定时任务，就会加345描述符释放掉
//    而345描述符对应的连接一但被释放，接下来在处理345事件的时候就会导致持续崩溃（内存访问错误）
//    因此，在事件处理中，并不能直接对连接进行释放，而是应该将释放操作压入任务池当中，

#include <iostream>
#include "../source/server.hpp"

int main()
{
    signal(SIGCHLD, SIG_IGN); // 忽略SIGPIPE信号，防止客户端断开连接时，服务器发送数据导致进程退出
    for (int i = 0; i < 10; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            DBG_LOG("fork error");
            exit(1);
        }
        else if (pid == 0)
        {
            Socket cli_sock;
            cli_sock.CreateClient("127.0.0.1", 8080);
            std::string req = "GET /hello HTTP/1.1\r\nconnection: keep-alive\r\ncontent-length: 100\r\n\r\n";
            while (true)
            {
                assert(cli_sock.Send(req.c_str(), req.size()) != -1);
                char buf[1024] = {0};
                int n = cli_sock.Recv(buf, sizeof(buf) - 1);
                if (n == 0)
                {
                    std::cout << "Server closed the connection" << std::endl;
                    break;
                }
                if (n < 0)
                {
                    std::cerr << "recv error" << std::endl;
                    break;
                }
                buf[n] = '\0';
                DBG_LOG("Received: %s", buf);
            }
            cli_sock.Close();
            exit(0);
        }
    }

    return 0;
}
// 给服务器发送一个数据，告诉服务器要发送1024字节的数据，但是实际发送的数据不足1024，查看服务器处理结果
// 1.如果数据 只发送一次，服务器将得不到完整请求，就不会进行业务处理，客户端就得不到响应，最终超时关闭连接
// 2.如果给服务器发送了多次请求，服务器将会在接收完所有请求后，进行业务处理，客户端就会得到响应

#include <iostream>
#include "../source/server.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient("127.0.0.1", 8080);
    std::string req = "GET /hello HTTP/1.1\r\nconnection: keep-alive\r\ncontent-length: 100\r\n\r\nniyaoyouofferle";
    while (true)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        // assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        // assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        // assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        // assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        // assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buf[1024] = {0};
        int n = cli_sock.Recv(buf, sizeof(buf) - 1);
        if (n == 0)
        {
            std::cout << "Server closed the connection" << std::endl;
            break;
        }   
        if(n < 0)
        {
            std::cerr << "recv error" << std::endl;
            break;
        }
        buf[n] = '\0';
        DBG_LOG("Received: %s", buf);
        sleep(3);
    }
    cli_sock.Close();
    return 0;
}
// 多条请求测试

#include <iostream>
#include "../source/server.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient("127.0.0.1", 8080);
    std::string req = "GET /hello HTTP/1.1\r\nconnection: keep-alive\r\ncontent-length: 0\r\n\r\n";
    req += "GET /hello HTTP/1.1\r\nconnection: keep-alive\r\ncontent-length: 0\r\n\r\n"; 
    req += "GET /hello HTTP/1.1\r\nconnection: keep-alive\r\ncontent-length: 0\r\n\r\n"; 
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
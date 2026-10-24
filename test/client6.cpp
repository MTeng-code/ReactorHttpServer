// 大文件传输测试

#include <iostream>
#include "../source/server.hpp"
#include "/opt/data/private/project_muduo/source/http/http.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient("127.0.0.1", 8080);
    std::string req = "PUT /update.txt HTTP/1.1\r\nconnection: keep-alive\r\n";
    std::string body;
    Util::ReadFile("./hello.txt", &body);
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

    assert(cli_sock.Send(req.c_str(), req.size()) != -1);
    assert(cli_sock.Send(body.c_str(), body.size()) != -1);

    char buf[1024] = {0};
    int n = cli_sock.Recv(buf, sizeof(buf) - 1);
    if (n == 0)
    {
        std::cout << "Server closed the connection" << std::endl;
        
    }
    if (n < 0)
    {
        std::cerr << "recv error" << std::endl;
        
    }
    buf[n] = '\0';
    DBG_LOG("Received: %s", buf);
    sleep(3);

    cli_sock.Close();
    return 0;
}
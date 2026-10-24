#include "http.hpp"

#define WWWROOT "./wwwroot"

std::string RequestStr(const HttpRequest &req)
{
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._version << "\r\n";
    for(auto &it : req._query_params)
    {
        ss <<  it.first << ": " << it.second << "\r\n";
    }

    for(auto &it : req._headers)
    {
        ss <<  it.first << ": " << it.second << "\r\n";
    }
    ss << "\r\n";
    ss << req._body;
    return ss.str();
}

void Hello(const HttpRequest &req, HttpResponse &resp)
{
    resp.SetContent(RequestStr(req), "text/plain");
    // sleep(30); // 模拟业务处理耗时30秒

}

// void Login(const HttpRequest &req, HttpResponse &resp)
// {
//     // std::string body = "<html><head><title>Login</title></head><body><h1>Login</h1></body></html>";
//     resp.SetContent(RequestStr(req), "text/plain");
// }

void Login(const HttpRequest &req, HttpResponse &resp)
{
    std::stringstream ss;

    ss << "Method: " << req._method << "\n";
    ss << "Path: " << req._path << "\n";
    ss << "Body size: " << req._body.size() << "\n";
    ss << "Body: [" << req._body << "]\n";

    resp.SetContent(ss.str(), "text/plain");
}

void Update(const HttpRequest &req, HttpResponse &resp)
{
    // std::string body = "<html><head><title>Login</title></head><body><h1>Login</h1></body></html>";
    // resp.SetContent(RequestStr(req), "text/plain");
    std::string pathname = WWWROOT + req._path;
    Util::WriteFile(pathname, req._body);
}
void DelFile(const HttpRequest &req, HttpResponse &resp)
{
    // std::string body = "<html><head><title>Login</title></head><body><h1>Login</h1></body></html>";
    resp.SetContent(RequestStr(req), "text/plain");
}

int main()
{
    HttpServer server(8080);
    server.SetThreadCount(3);
    server.SetBaseDir(WWWROOT);
    server.SetGet("/hello", Hello);
    server.SetPost("/login", Login);
    server.SetPut("/update.txt", Update);
    server.SetDelete("/delete", DelFile);
    server.listen();


    return 0;
}
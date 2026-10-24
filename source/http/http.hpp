#include "../server.hpp"
#include <fstream>
#include <sys/stat.h>
#include <regex>

#define DEFALT_TIMEOUT 10 // 默认超时时间，单位秒

std::unordered_map<int, std::string> _statu_msg = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-state"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}};

std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};

class Util
{
public:
    // 字符串分割函数,将src按照sep分割，分割后的结果存放在arry中，最终返回字符串的数量
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry)
    {
        size_t count = 0;
        int offset = 0;
        while (offset < src.size())
        {
            size_t pos = src.find(sep, offset);
            if (pos == std::string::npos) // 没有找到分隔符,src中剩余的部分就是最后一个子串
            {
                count++;
                arry->push_back(src.substr(offset, src.size() - offset));
                break;
            }
            else if (pos == offset) // 找到的分隔符在offset位置,说明有空子串
            {
                offset = pos + sep.size();
                continue;
            }
            else
            {
                count++;
                arry->push_back(src.substr(offset, pos - offset));
                offset = pos + sep.size();
            }
        }
        // return arry->size();
        return count;
    }
    // 读取文件内容,放到buffer中
    static bool ReadFile(const std::string &filename, std::string *buf)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs.is_open())
        {
            ERR_LOG("open file %s failed", filename.c_str());
            return false;
        }
        ifs.seekg(0, std::ios::end);   // seekg() 是 C++ 输入流里用来移动“读指针”位置的函数
        size_t filesize = ifs.tellg(); // tellg() 是 C++ 输入流里用来获取当前“读指针”位置的函数，ifstream 里经常用它来获取文件大小。
        ifs.seekg(0, std::ios::beg);   // 确定大小后跳转到开头
        // std::string content;
        buf->resize(filesize);
        ifs.read(&(*buf)[0], filesize);
        if (!ifs.good())
        {
            ERR_LOG("read file %s failed", filename.c_str());
            return false;
        }
        // buf->WriteString(content);
        ifs.close();
        return true;
    }
    // 向文件写入数据
    static bool WriteFile(const std::string &filename, const std::string &buf)
    {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            ERR_LOG("open file %s failed", filename.c_str());
            return false;
        }
        ofs.write(buf.c_str(), buf.size());
        if (!ofs.good())
        {
            ERR_LOG("write file %s failed", filename.c_str());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }

    // URL编码, 避免URL中资源路径与查询字符串中的特殊字符与http请求中的特殊字符冲突, 例如: / ? & = 等
    // 编码格式：将特殊字符的ASCII值，转化为两个16进制字符，前缀为%，例如空格的ASCII值是32，转化为%20
    // 在RFC3986中规定的URL绝对不编码的字符：A-Z, a-z, 0-9, -, ., _, ~
    // RFC3986中规定的URL绝对编码的字符：! * ' ( ) ; : @ & = + $ , / ? # [ ]
    // RFC3986文档规定，编码格式 %HH
    // W3C文档规定，查询字符串中，空格要编码为+，但是在路径中，空格必须编码为%20
    static std::string UrlEncode(const std::string &src, bool encode_space_as_plus = true)
    {
        std::string res;
        for (auto &c : src)
        {
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c)) // RFC3986中规定的URL绝对不编码的字符
            {
                res += c;
            }
            else if (c == ' ' && encode_space_as_plus)
            {
                res += '+'; // W3C文档规定，查询字符串中，空格要编码为+
            }
            else // 剩下的字符都是需要编码为%HH的格式
            {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                res += buf;
            }
        }
        return res;
    }

    static int HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        else
        {
            return -1;
        }
    }
    // URL解码, 将%HH格式的字符转化为原始字符, 将+转化为空格
    static std::string UrlDecode(const std::string &url, bool encode_space_as_plus = true)
    {
        std::string res;
        for (size_t i = 0; i < url.size(); i++)
        {
            if (url[i] == '+' && encode_space_as_plus)
            {
                res += ' ';
            }
            else if (url[i] == '%' && i + 2 < url.size())
            {
                int hex1 = HEXTOI(url[i + 1]);
                int hex2 = HEXTOI(url[i + 2]);
                if (hex1 != -1 && hex2 != -1)
                {
                    res += static_cast<char>((hex1 << 4) | hex2);
                    i += 2; // 跳过已处理的两个字符
                }
                else
                {
                    res += url[i]; // 如果不是有效的%HH格式,则原样输出
                }
            }
            else
            {
                res += url[i];
            }
        }
        return res;
    }
    // 响应状态码描述， 获取响应状态码的描述信息
    static std::string StatuDesc(int state_code)
    {
        auto it = _statu_msg.find(state_code);
        if (it != _statu_msg.end())
        {
            return it->second;
        }
        else
        {
            return "Unknown";
        }
    }

    // 根据文件后缀获取文件mime类型，获取扩展名
    static std::string ExtMime(const std::string &filename)
    {
        size_t pos = filename.rfind('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream"; // 默认的mime类型
        }
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream"; // 默认的mime类型
        }
        else
        {
            return it->second;
        }
    }

    // 判断路径是否是目录
    static bool IsDirectory(const std::string &filename)
    {
        struct stat st;
        if (stat(filename.c_str(), &st) < 0)
        {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }

    // 判断路径是否是文件
    static bool IfRegular(const std::string &filename)
    {
        struct stat st;
        if (stat(filename.c_str(), &st) < 0)
        {
            return false;
        }
        return S_ISREG(st.st_mode);
    }

    // http请求的资源路径的有效性判断
    // POST /login HTTP/1.1
    // Host: 127.0.0.1:8080
    // Content-Type: application/x-www-form-urlencoded
    // Content-Length: 18
    // Connection: close

    // username=tom&age=18

    static bool ValidPath(const std::string &path)
    {
        std::vector<std::string> parts;
        size_t count = Split(path, "/", &parts);
        int level = 0;
        for (auto &part : parts)
        {
            if (part == "..")
            {
                level--;
                if (level < 0)
                {
                    return false; // 不能访问上级目录
                }
            }
            else if (part == "." || part.empty())
            {
                continue; // 当前目录或者空目录，忽略
            }
            else
            {
                level++;
            }
        }
        return true;
    }
    static std::string ToLower(std::string str)
    {
        std::transform(
            str.begin(),
            str.end(),
            str.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        return str;
    }
};

// 存储HTTP请求的相关信息
class HttpRequest
{
public:
    std::string _method;                                        // 请求方法
    std::string _path;                                          // 请求路径
    std::string _version;                                       // HTTP版本
    std::string _body;                                          // 请求体
    std::smatch _matches;                                       // 正则匹配结果
    std::unordered_map<std::string, std::string> _headers;      // 请求头
    std::unordered_map<std::string, std::string> _query_params; // 查询参数
public:
    // 重置
    void Reset()
    {
        _method.clear();
        _path.clear();
        _version.clear();
        _body.clear();
        _matches = std::smatch();
        _headers.clear();
        _query_params.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &value)
    {
        // _headers.insert(std::make_pair(key, value));
        _headers[Util::ToLower(key)] = value;
    }
    // 判断是否存在指定头部字段
    bool HasHeader(const std::string &key)
    {
        // return _headers.find(key) != _headers.end();
        return _headers.find(Util::ToLower(key)) != _headers.end();
    }
    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        // auto it = _headers.find(key);
        auto it = _headers.find(Util::ToLower(key));
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }

    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &value)
    {
        _query_params.insert(std::make_pair(key, value));
    }

    // 判断是否存在指定查询参数
    bool HasParam(const std::string &key)
    {
        return _query_params.find(key) != _query_params.end();
    }

    // 获取指定查询参数的值
    std::string GetParam(const std::string &key)
    {
        auto it = _query_params.find(key);
        if (it == _query_params.end())
        {
            return "";
        }
        return it->second;
    }

    // 获取正文长度
    size_t GetContentLength()
    {
        if (HasHeader("Content-Length"))
        {
            std::string content_length = GetHeader("Content-Length");
            return std::stoul(content_length); // stoul() 将字符串转换为无符号长整数
        }
        return 0;
        // return _body.size();
    }

    // 判断是否是短连接
    // 没有指定Connection头部字段，或者指定Connection: close，则为短连接
    bool CloseConnection()
    {
        if (HasHeader("Connection"))
        {
            std::string connection = GetHeader("Connection");
            return connection == "close";
        }
        return false;
    }
};

// HttpResponse 就是用来表示服务器准备返回给客户端的一条 HTTP 响应的对象
class HttpResponse
{
public:
    int _state_code;                                       // 响应状态码,表示服务器处理客户端请求的结果。
    bool _redirect_flag;                                   // 是否是重定向,这是一个布尔变量，用来告诉服务器：当前响应是否需要让客户端跳转到另一个地址。
    std::string _body;                                     // 响应体，包含响应的主体内容。
    std::string _redirect_url;                             // 重定向的URL，重定向目标地址，这个成员保存客户端应该跳转到哪里。
    std::unordered_map<std::string, std::string> _headers; // 响应头部字段，一个保存多个键值对的容器，用来存储响应的附加信息。
public:
    HttpResponse() : _redirect_flag(false), _state_code(200) {}
    HttpResponse(int state_code) : _redirect_flag(false), _state_code(state_code) {}
    // 重置
    void ReSet()
    {
        _state_code = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &value)
    {
        _headers[Util::ToLower(key)] = value;
    }
    // 判断是否存在指定头部字段
    bool HasHeader(const std::string &key)
    {
        return _headers.find(Util::ToLower(key)) != _headers.end();
    }
    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(Util::ToLower(key));

        if (it == _headers.end())
        {
            return "";
        }

        return it->second;
    }
    // 设置正文
    void SetContent(const std::string &body, std::string content_type = "text/html")
    {
        _body = body;
        SetHeader("Content-Length", std::to_string(_body.size()));
        SetHeader("Content-Type", content_type);
    }

    // 设置输出重定向
    void SetRedirect(const std::string &url, int state_code = 302)
    {
        _state_code = state_code;
        _redirect_flag = true;
        _redirect_url = url;
    }

    // 判断是否是短连接
    bool CloseConnection()
    {
        if (HasHeader("Connection"))
        {
            std::string connection = GetHeader("Connection");
            return connection == "close";
        }
        return false;
    }
};

typedef enum
{
    RECV_HTTP_LINE, // 正在解析请求行
    RECV_HTTP_HEAD, // 正在解析请求头
    RECV_HTTP_BODY, // 正在解析正文
    RECV_HTTP_OVER, // 一条HTTP请求完整了
    RECV_HTTP_ERROR // 请求有错误
} HttpRecvstate;

#define HTTP_LINE_MAX 8192
class HttpContext
{
private:
    HttpRequest _request; // 解析的到的请求信息
    HttpResponse _response;
    HttpRecvstate _recv_state;
    int _resp_state; // 接收状态
private:
    bool ParseHttpLine(const std::string &line) // 解析请求行
    {
        std::smatch matches;
        std::regex e("(GET|POST|HEAD|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e); // 匹配
        if (!ret)
        {
            _resp_state = 400; // 请求行格式错误
            _recv_state = RECV_HTTP_ERROR;
            return false;
        }
        // 解析请求行的各个部分
        _request._method = matches[1].str();
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper); // 将请求方法转为大写
        // 资源路径获取，如果资源路径中有URL编码，则需要解码
        _request._path = Util::UrlDecode(matches[2].str(), false);
        _request._version = matches[4].str();

        std::vector<std::string> params_arry;
        std::string query_string = matches[3].str();
        // 查询字符串的解析，按照&分割，然后按照=分割键值对
        Util::Split(query_string, "&", &params_arry);
        for (auto &parm : params_arry)
        {
            size_t pos = parm.find("=");
            if (pos == std::string::npos)
            {
                _resp_state = 400; // 请求参数格式错误
                _recv_state = RECV_HTTP_ERROR;
                return false;
            }
            std::string key = Util::UrlDecode(parm.substr(0, pos), true);
            std::string value = Util::UrlDecode(parm.substr(pos + 1), true);
            _request.SetParam(key, value);
        }

        return true;
    }

    bool RecvHttpLine(Buffer &buf) // 接收请求行
    {
        if (_recv_state != RECV_HTTP_LINE)
        {
            return false;
        }
        std::string line = buf.GetLineAndPop();
        if (line.empty())
        {
            if (buf.ReadableSize() > HTTP_LINE_MAX)
            {
                _resp_state = 414; // 请求URI过长
                _recv_state = RECV_HTTP_ERROR;
                return false;
            }
            return true; // 请求行不完整，继续接收
        }
        if (line.size() > HTTP_LINE_MAX)
        {
            _resp_state = 414; // 请求URI过长
            _recv_state = RECV_HTTP_ERROR;
            return false;
        }
        bool ret = ParseHttpLine(line);
        if (!ret)
        {
            _resp_state = 400; // 请求行格式错误
            _recv_state = RECV_HTTP_ERROR;
            return false;
        }
        _recv_state = RECV_HTTP_HEAD;
        return true;
    }

    bool ParseHttpHead(const std::string &line) // 解析请求头
    {
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _resp_state = 400; // 请求头格式错误
            _recv_state = RECV_HTTP_ERROR;
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 2); // 跳过 ": "
        // 去掉结尾的 \r\n
        if (value.size() >= 2 &&
            value[value.size() - 2] == '\r' &&
            value[value.size() - 1] == '\n')
        {
            value.resize(value.size() - 2);
        }
        _request.SetHeader(key, value);
        return true;
    }
    bool RecvHttpHead(Buffer &buf) // 接收请求头
    {
        // 解析请求头，一行一行取出数据，知道遇到空行
        if (_recv_state != RECV_HTTP_HEAD)
        {
            return false;
        }
        while (true)
        {
            std::string line = buf.GetLineAndPop();
            if (line.empty())
            {
                if (buf.ReadableSize() > HTTP_LINE_MAX)
                {
                    _resp_state = 431; // 请求头过长
                    _recv_state = RECV_HTTP_ERROR;
                    return false;
                }
                return true; // 请求头不完整，继续接收
            }
            if (line.size() > HTTP_LINE_MAX)
            {
                _resp_state = 431; // 请求头过长
                _recv_state = RECV_HTTP_ERROR;
                return false;
            }
            if (line == "\r" || line == "\r\n")
            {
                break;
            }
            bool ret = ParseHttpHead(line);
            if (!ret)
            {
                _resp_state = 400; // 请求头格式错误
                _recv_state = RECV_HTTP_ERROR;
                return false;
            }
        }
        _recv_state = RECV_HTTP_BODY;
        return true;
    }

    bool RecvHttpBody(Buffer &buf) // 接收请求体
    {
        // 缓冲区中包含了请求的完整正文，就取出所需数据，否则取出数据但继续接收数据
        // 获取正文长度，如果正文长度为0，则说明没有请求体，直接返回
        if (_recv_state != RECV_HTTP_BODY)
        {
            return false;
        }
        size_t content_length = _request.GetContentLength();
        if (content_length == 0)
        {
            // 没有请求体，直接返回
            _recv_state = RECV_HTTP_OVER;
            return true;
        }
        // 判断body中接受了多少数据
        // buf中除了包含请求体数据，还可能包含了下一条HTTP请求的数据，所以不能直接判断buf中是否有content_length大小的数据
        size_t real_length = content_length - _request._body.size(); // 真实还剩下多少数据未接收
        if (real_length == 0)
        {
            _recv_state = RECV_HTTP_OVER;
            return true;
        }
        if (buf.ReadableSize() >= real_length)
        {
            // 说明buf中包含了完整的请求体数据
            _request._body += buf.ReadAndPopString(real_length);
            _recv_state = RECV_HTTP_OVER;
            return true;
        }
        else
        {
            // 说明还有数据没有接收完，继续接收数据
            _request._body += buf.ReadAndPopString(buf.ReadableSize());
            return true;
        }
    }

public:
    HttpContext() : _resp_state(200), _recv_state(RECV_HTTP_LINE) {}
    void Reset()
    {
        _request.Reset();
        _response.ReSet();
        _resp_state = 200;
        _recv_state = RECV_HTTP_LINE;
    }
    int Respstate()
    {
        return _resp_state;
    }
    HttpRecvstate Recvstate()
    {
        return _recv_state;
    }
    HttpRequest &Request()
    {
        return _request;
    }
    HttpResponse &Response()
    {
        return _response;
    }
    // 接收并解析http请求
    void RecvHttpRequest(Buffer &buf)
    {
        // 不同的状态，根据状态调用不同的解析函数
        // 但不要break，因为处理完请求行后，就处理头部，而且可能一次性接收了多条HTTP请求，所以要继续解析
        switch (_recv_state)
        {
        case RECV_HTTP_LINE:
            RecvHttpLine(buf);
        case RECV_HTTP_HEAD:
            RecvHttpHead(buf);
        case RECV_HTTP_BODY:
            RecvHttpBody(buf);
        }
    }
};

class HttpServer
{
    // 用于实现Http服务器的搭建
    // HttpServer 主要干的事情大概就是：
    // TcpServer收到数据
    //         ↓
    // MessageCallback
    //         ↓
    // HttpServer::OnMessage()
    //         ↓
    // 拿到这个Connection对应的HttpContext
    //         ↓
    // HttpContext解析HTTP请求
    //         ↓
    // HttpRequest
    //         ↓
    // 找到对应业务处理函数
    //         ↓
    // 生成HttpResponse
    //         ↓
    // 序列化成HTTP响应字符串
    //         ↓
    // Connection::Send()
private:
    using Headler = std::function<void(const HttpRequest &, HttpResponse &)>;
    std::vector<std::pair<std::regex, Headler>> _get_route;
    std::vector<std::pair<std::regex, Headler>> _post_route;
    std::vector<std::pair<std::regex, Headler>> _put_route;
    std::vector<std::pair<std::regex, Headler>> _delete_route;
    std::string _basedir; // 静态资源的根目录
    TcpServer _server;

private:
    void ErrorHandle(const HttpRequest &req, HttpResponse &resp) // 错误处理
    {
        // 组织一个错误展示页面
        // 将页面数据当做响应正文放到rsp中，设置状态码为400，Content-Type为text/html
        std::string body;
        body += "<pre>";
        body += "<html>";
        body += "<title>";
        body += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(resp._state_code);
        body += " ";
        body += Util::StatuDesc(resp._state_code);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        body += "</pre>";

        resp.SetContent(body, "text/html");
    }
    // 将httpresponse中的要素按照http协议格式进行组织，发送
    void SendHttpResponse(const PtrConnection &conn, HttpRequest &req, HttpResponse &resp)
    {
        // 完善头部字段，

        // 发送给客户端
        if (req.CloseConnection())
        {
            resp.SetHeader("Connection", "close");
        }
        else
        {
            resp.SetHeader("Connection", "keep-alive");
        }
        if (!resp._body.empty() && !resp.HasHeader("Content-Length"))
        {
            resp.SetHeader("Content-Length", std::to_string(resp._body.size()));
        }
        if (!resp._body.empty() && !resp.HasHeader("Content-Type"))
        {
            resp.SetHeader("Content-Type", "application/octet-stream");
        }
        if (resp._redirect_flag)
        {
            resp.SetHeader("Location", resp._redirect_url);
        }
        // 将rsp中的要素，按照http协议格式组织成字符串，
        std::stringstream ss;
        ss << req._version << " " << std::to_string(resp._state_code) << " " << Util::StatuDesc(resp._state_code) << "\r\n";
        for (auto &header : resp._headers)
        {
            ss << header.first << ": " << header.second << "\r\n";
        }
        ss << "\r\n";
        ss << resp._body;
        std::string response_str = ss.str();
        conn->Send(response_str.c_str(), response_str.size());
    }
    // 静态资源的请求处理
    // 将静态资源文件的路径读取出来
    bool FileHandler(HttpRequest &req, HttpResponse &resp) // 静态资源的请求处理
    {
        std::string filepath = _basedir + req._path;
        if (req._path.back() == '/')
        {
            filepath += "index.html"; // 如果请求路径是目录，则默认访问index.html
        }
        bool ret = Util::ReadFile(filepath, &resp._body);
        if (!ret)
        {
            resp._state_code = 404; // 文件不存在
            ErrorHandle(req, resp);
            return false;
        }
        std::string mime = Util::ExtMime(filepath);
        resp.SetHeader("Content-Type", mime);
        // resp._state_code = 200;
        return true;
    }
    // 判断是否是静态资源请求
    bool IsFileHeadler(HttpRequest &req)
    {
        // 必须设置了静态资源的根目录，且请求方法是GET或者HEAD，且请求路径不是功能性请求的路由表中存在的路径，请求的资源必须存在
        // 1. 判断是否设置了静态资源的根目录
        if (_basedir.empty())
        {
            return false;
        }
        // 2. 判断请求方法是否是GET或者HEAD
        if (req._method != "GET" && req._method != "HEAD")
        {
            return false;
        }
        // 3. 判断请求路径是否是功能性请求的路由表中存在的路径
        if (Util::ValidPath(req._path) == false)
        {
            return false;
        }
        // 4. 判断请求的资源是否存在
        std::string filepath = _basedir + req._path;
        if (req._path.back() == '/')
        {
            filepath += "index.html"; // 如果请求路径是目录，则默认访问index.html
            if (Util::IfRegular(filepath) == false)
            {
                return false;
            }
            // req._path = filepath; // 更新请求路径为实际文件路径
            return true;
        }
        if (Util::IfRegular(filepath) == false)
        {
            return false;
        }
        return true;
    }
    // 功能性请求的分发处理
    void Dispathcher(HttpRequest &req, HttpResponse &resp, std::vector<std::pair<std::regex, Headler>> &handlers) // 功能性请求的分发处理
    {
        // 在对应请求方法的路由表中查找请求路径对应的处理函数，如果找到了，就调用处理函数，否则返回404错误
        // 思想：路由表是一个map，key是请求路径，value是处理函数
        // 使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应的函数进行处理
        for (auto &handler : handlers)
        {
            // std::string re(handler.first);
            std::regex re(handler.first);
            Headler &functor = handler.second;
            bool ret = std::regex_match(req._path, req._matches, std::regex(re));
            if (!ret)
            {
                continue;
            }
            return functor(req, resp);
        }
        resp._state_code = 404; // 没有找到对应的处理函数，返回404错误
    }
    void Route(HttpRequest &req, HttpResponse &resp) // 路由分发
    {
        // 根据请求方法和请求路径，找到对应的处理函数，并调用
        // 1. 对请求进行分辨，是否是静态资源请求，还是功能性请求
        // 2. 如果是静态资源请求，则调用FileHandler()方法处理
        // 3. 如果是功能性请求，则调用Dispathcher()方法处理
        if (IsFileHeadler(req))
        {
            FileHandler(req, resp);
            return;
        }
        if (req._method == "GET" || req._method == "HEAD")
        {
            return Dispathcher(req, resp, _get_route);
        }
        else if (req._method == "POST")
        {
            return Dispathcher(req, resp, _post_route);
        }
        else if (req._method == "PUT")
        {
            return Dispathcher(req, resp, _put_route);
        }
        else if (req._method == "DELETE")
        {
            return Dispathcher(req, resp, _delete_route);
        }
        resp._state_code = 405; // 方法不允许
    }
    void Onconnected(const PtrConnection &conn) // 设置上下文
    {
        conn->SetContext(HttpContext());
        DBG_LOG("new connection from %p", conn.get());
    }

    void OnMessage(const PtrConnection &conn, Buffer *buf) // 处理http请求
    {
        // 1. 获取当前连接的上下文对象
        // 2. 调用上下文对象的RecvHttpRequest()方法，解析HTTP请求
        // 3. 判断解析状态，如果解析完成，则调用Route()方法进行路由
        // 4. 如果解析出错，则返回错误响应
        // 5. 对HTTPResponse进行序列化，发送给客户端
        // 6. 如果是短连接，则关闭连接
        // 1. 获取当前连接的上下文对象
        HttpContext *context = conn->GetContext()->get<HttpContext>();

        while (true)
        {

            // 2. 调用上下文对象的RecvHttpRequest()方法，解析HTTP请求
            context->RecvHttpRequest(*buf);
            if (context->Recvstate() != RECV_HTTP_OVER && context->Recvstate() != RECV_HTTP_ERROR)
            {
                return; // 请求还没有接收完，继续接收数据
            }
            HttpRequest &req = context->Request();
            HttpResponse &resp = context->Response();
            if (context->Respstate() >= 400)
            {
                // 解析出错，返回错误响应
                resp._state_code = context->Respstate();
                ErrorHandle(req, resp);
                SendHttpResponse(conn, req, resp);
                context->Reset();
                buf->Clear(); // 清空缓冲区
                conn->Shutdown();
                return;
            }
            // 3. 请求还没有接收完整，等待下一次消息回调
            if (context->Recvstate() != RECV_HTTP_OVER)
            {
                return;
            }
            // 4. 请求完整，执行路由和业务处理
            Route(req, resp);
            // 5. 生成并发送 HTTP 响应
            SendHttpResponse(conn, req, resp);
            // 6. 如果响应要求关闭连接，就不再处理下一条请求
            if (resp.CloseConnection())
            {
                conn->Shutdown();
            }

            // 7. 当前请求处理完成，清理上下文
            context->Reset();

            // 8. Buffer 没有剩余数据，等待下一次 TCP 数据到达
            if (buf->ReadableSize() == 0)
            {
                return;
            }
        }
    }

public:
    HttpServer(int port, int timeout = DEFALT_TIMEOUT) : _server(port)
    {
        _server.EnableInactivateRelease(timeout);
        _server.SetConnectionCallback(std::bind(&HttpServer::Onconnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void SetBaseDir(const std::string &path)
    {
        bool ret = Util::IsDirectory(path);
        if(!ret)
        {
            ERR_LOG("SetBaseDir failed, %s is not a directory", path.c_str());
            return;
        }
        _basedir = path;
    }
    // 设置请求与处理函数的映射关系
    void SetGet(const std::string &path, const Headler &handler)
    {
        _get_route.push_back(std::make_pair(std::regex(path), handler));
    }
    void SetPost(const std::string &path, const Headler &handler)
    {
        _post_route.push_back(std::make_pair(std::regex(path), handler));
    }
    void SetPut(const std::string &path, const Headler &handler)
    {
        _put_route.push_back(std::make_pair(std::regex(path), handler));
    }
    void SetDelete(const std::string &path, const Headler &handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(path), handler));
    }
    void SetThreadCount(int count)
    {
        _server.SetThreadCount(count);
    }
    void listen()
    {
        _server.Start();
    }
};
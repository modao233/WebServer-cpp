#include "httprequest.h"

namespace url {
    unsigned char ToHex(unsigned char x) 
{ 
    return  x > 9 ? x + 55 : x + 48; 
}
 
unsigned char FromHex(unsigned char x) 
{ 
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}
 
std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char)str[i]) || 
            (str[i] == '-') ||
            (str[i] == '_') || 
            (str[i] == '.') || 
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}
 
std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '+') strTemp += ' ';
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high*16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}
}

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome",
    "/video", "/picture"
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1}
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer &buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH){
        const char *lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF+2);
        std::string line(buff.Peek(), lineEnd);
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine(line)){
                return false;
            }
            ParsePath();
            break;
        case HEADERS:
            ParseHeader(line);
            if(buff.ReadableBytes() <= 2){
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()){ break; }
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath() {
    // path_ += "index.html";
    if(path_ == "/") {
        path_ = "/index.html";
    }
    else {
        // for(auto &item : DEFAULT_HTML) {
        //     if(item == path_) {
        //         path_ += ".html";
        //         break;
        //     }
        // }
        if(path_[path_.size()-1] == '/')path_ += "index.html";
    }
    path_ = url::UrlDecode(path_);
}

bool HttpRequest::ParseRequestLine(const std::string &line) {
    //行首 ^ ：匹配以...开头
    //[^ ] ：范围匹配取反，除空格外
    //* ：匹配0次或多次
    //行末 $ ：匹配以...结尾
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch sub_match;
    if(regex_match(line, sub_match, patten)) {
        method_ = sub_match[1];
        path_ = sub_match[2];
        version_ = sub_match[3];
        state_ = HEADERS;
        return true;
    }
}

void HttpRequest::ParseHeader(const std::string &line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch sub_match;
    if(regex_match(line, sub_match, patten)) {
        header_[sub_match[1]] = sub_match[2];
    }
    else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody(const std::string &line) {
    body_ = line;
    ParsePost();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch){
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::ParsePost() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool is_login = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], is_login)) {
                    path_ = "/welcome.html";
                }
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded() {
    if(body_.size() == 0) { return; }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; ++i) {
        char ch = body_[i];
        switch(ch){
        case '=':
            key = body_.substr(j, i-j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i+1]*16 + ConverHex(body_[i+2]));
            body_[i+2] = num%10 + '0';
            body_[i+1] = num/10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i-j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
        assert(j <= i);
        if(post_.count(key) == 0 && j < i) {
            value = body_.substr(j, i-j);
            post_[key] = value;
        }
    }
}


bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool is_login) {
    if(name == "" || pwd == ""){ return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;
    sql = MysqlPool::Instance() -> GetConn();
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if(!is_login) {
        flag = true;
    }
    snprintf(order, 256, "SELECT username, password FROM user \
            WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_field(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        if(is_login) {
            if(pwd == password) {
                flag = true;
            }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        //注册行为，但查到用户名，提示用户名已经被使用
        else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    //注册行为 且 用户名未被使用
    if(!is_login && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password\
                VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    MysqlPool::Instance() -> FreeConn(sql);
    LOG_DEBUG("UserVerify success!");
    return flag;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char *key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    return "";
}
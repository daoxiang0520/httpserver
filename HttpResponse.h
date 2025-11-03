#include<vector>
#include<string>
#include<iostream>
#include <unordered_map>
#include<chrono>
#include<ctime>
#include<sstream>
inline std::string nowDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm)) {
        return std::string(buf);
    }
    return std::string();
}
class HttpResponse
{
private:
    std::vector<std::string> Default_doc;
    std::unordered_map<std::string,std::string> header_;
    std::string version_;
    std::string reason_;
    std::string ContentType_;
    int status_;
    std::string body_;
    bool AllCheck;
    std::string All;
public:
    HttpResponse(int status=200,std::string reason="OK",std::string version="HTTP/1.1");
    ~HttpResponse()=default;
    void setHeader(std::string key,std::string Content){header_[key]=Content,AllCheck=0;};
    void setStatus(int status,std::string reason);
    void setBody(std::string content){body_=content;};
    std::string AllContent();
};
HttpResponse::HttpResponse(int status,std::string reason,std::string version):
status_(status),reason_(reason),version_(version){
    setHeader("Date",nowDate());
    setHeader("Server","DXServer(WIN64)");
}
std::string HttpResponse::AllContent(){
    if(AllCheck)return All;
    std::string ans="HTTP/1.1 ";
    ans=ans+std::to_string(status_)+' '+reason_+"\r\n";
    for(auto it = header_.begin(); it != header_.end(); ++it) {
        ans=ans+it->first+':'+it->second+"\r\n";
    }ans+="\r\n";
    ans+=body_;
    ans+="\r\n\r\n";
    All=ans;
    AllCheck=1;
    return All;
}

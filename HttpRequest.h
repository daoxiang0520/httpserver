#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <map>
#include <unordered_map>

class HttpRequest {
    public:
        HttpRequest(std::string str);
        ~HttpRequest()=default;
        std::string method(){return method_;};
        std::string url(){return url_;};
        std::string version(){return version_;};
        std::string body(){return body_;};
        std::unordered_map<std::string,std::string> header_;
        std::string query(){return query_;};
        std::string priurl(){return priurl_;};
        void setbody(std::string BODY){body_=BODY;}
        void seturl(std::string URL){url_=URL;}
    private:
        std::string method_;
        std::string url_;
        std::string version_;
        std::string body_;
        std::string query_;
        std::string priurl_;
};
HttpRequest::HttpRequest(std::string str){
    int pos1=str.find("\r\n\r\n");
    int sep=4;
    if(pos1==std::string::npos)pos1=str.find("\n\n"),sep=2;
    std::istringstream sstmp(str.substr(0,pos1));
    std::string headera;
    getline(sstmp,headera);
    std::istringstream headers(headera);
    headers>>method_>>url_>>version_;
    priurl_=url_;
    if(version_.find("HTTP/")!=std::string::npos)version_=version_.substr(5);
    std::string tmp_,tmph_,tmpc_;
    while(std::getline(sstmp,tmp_)){
        if(tmp_.empty())break;
        if(tmp_.back()=='\r')tmp_.pop_back();
        int posm=tmp_.find(":");
        tmph_=tmp_.substr(0,posm);
        tmpc_=tmp_.substr(posm+1);
        while(tmpc_[0]==' ')tmpc_=tmpc_.substr(1);
        header_[tmph_]=tmpc_;
    }if(url_[0]='/')url_=url_.substr(1);
    body_= str.substr(pos1+sep,str.size()-pos1-sep-1);
    int query_index=url_.find("?");
    if(query_index!=std::string::npos){
        query_=url_.substr(query_index+1);
        url_=url_.substr(0,query_index);
    }
};

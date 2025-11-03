#pragma once
#include<iostream>
#include<string>
#if defined(_WIN32)
#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>
#include<winsock.h>
#include<ws2tcpip.h>
#else
#define INVALID_SOCKET -1
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#endif

#include<thread>
#include<mutex>
#include<vector>
#include<fstream>
#include<filesystem>
#include <cctype>
#include <cerrno>

#include"HttpResponse.h"
#include"HttpRequest.h"
#include"../threadpool/ThreadPool.h"

#define CGI_FORK_ERROR -1
#define CGI_SUCCESS 1
#define DEFAULT_PORT 8080

using namespace std;

static inline std::string to_cgi_env_key(const std::string& k) {
    std::string s; s.reserve(k.size()+5);
    s = "HTTP_";
    for (unsigned char c : k) s += (c=='-'? '_' : std::toupper(c));
    return s;
}

class HttpServer{
    public:
        HttpServer(int port);
        ~HttpServer(){stop();};
        void prestart();
        void run();
        void stop();
        void handleGET(int Clientsocket,HttpRequest& Re_);
        int handleCGI(int Clientsocket,string path,string name,HttpRequest& Re);
        int handlephp(int Clientsocket, std::string scriptPath, HttpRequest& Re);
        void handle404(int Clientsocket);
        vector<string> default_doc;
        int ServerSend(string str,int ClientSocket);
    private:
        ThreadPool Trds;
        vector<string> _404file;
        int serverSocket;
        int port;
        bool running;
        sockaddr_in serverAddr;
        void handleClient(int clientSocket);
        #ifdef _WIN32
        WSADATA wsa;
        #endif
};
HttpServer::HttpServer(int port):
    Trds(ThreadPool(std::thread::hardware_concurrency())){
            default_doc={"index.html","index.htm","index.php"};
            this->port=port;
            this->running=false;
            this->serverSocket=-1;
            memset(&serverAddr,0,sizeof(serverAddr));
            serverAddr.sin_family=AF_INET;
            serverAddr.sin_port=htons(port);
            serverAddr.sin_addr.s_addr=INADDR_ANY;
            _404file={"404.html"};
        };
void HttpServer::prestart(){
            running=1;
            int yes=1;
            //setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#ifndef _WIN32
            signal(SIGPIPE, SIG_IGN);
#endif
            #ifdef _WIN32
            if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){
                throw "WSAStartup error";
                return;
            }
            #endif
            serverSocket=socket(AF_INET,SOCK_STREAM,0);
            if(serverSocket<0){
                throw "Socketbuilding error";
                return;
            }
            
#ifndef _WIN32
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

            if(bind(serverSocket,(sockaddr*)&serverAddr,sizeof(serverAddr))<0){
                throw "bind error";
                stop();
                return;
            }if(listen(serverSocket,50)<0){
                throw "listen error";
                stop();
                return;
            }
        }
void HttpServer::stop(){
            if(running){
                running=0;
                #if defined(_WIN32)
                WSACleanup();
                #endif
                closesocket(serverSocket);
            }
        };
void HttpServer::run(){
    while(running){
        sockaddr_in ClientAddr;
        socklen_t len=sizeof(sockaddr_in);
        int client_link=accept(serverSocket,(sockaddr*)&ClientAddr,&len);
        if(client_link!=INVALID_SOCKET){
            Trds.submit([this,client_link]{
                this->handleClient(client_link);
            });
        }else{
            if(errno==EINTR){continue;}
            perror("accept");
            continue;
        }
    }
}
int HttpServer::handleCGI(int Clientsocket,string path,string name,HttpRequest& Re){
    int inpipe[2],outpipe[2];
    if(pipe(inpipe)<0||pipe(outpipe)<0) return -1;
    pid_t pid_=fork();
    if(pid_<0)return -1;
    else if(pid_==0){
        dup2(inpipe[0],STDIN_FILENO);
        dup2(outpipe[1],STDOUT_FILENO);
        close(inpipe[1]);
        close(outpipe[0]);
        setenv("GATEWAY_INTERFACE","CGI/1.1",1);
        setenv("REQUEST_METHOD",Re.method().c_str(),1);
        setenv("QUERY_STRING",Re.query().c_str(),1); 
        setenv("SCRIPT_NAME",name.c_str(),1);
        setenv("PATH_TRANSLATED",path.c_str(),1);
        /*for(auto i=Re.header_.begin();i!=Re.header_.end();i++){
            setenv(i->first.c_str(),i->second.c_str(),1);
        }*/setenv("SERVER_PROTOCOL","HTTP/1.1",1);
        auto cl = Re.header_["Content-Length"];
        if(!cl.empty()) setenv("CONTENT_LENGTH", cl.c_str(), 1); 
        auto ct = Re.header_["Content-Type"];   
        if(!ct.empty()) setenv("CONTENT_TYPE",  ct.c_str(), 1); 
        for (auto &kv : Re.header_){
            std::string key = kv.first;
            if (key == "Content-Length" || key == "Content-Type") continue;
            setenv(to_cgi_env_key(key).c_str(), kv.second.c_str(), 1);
        }
        char portbuf[16];
        snprintf(portbuf, sizeof(portbuf), "%d", ntohs(serverAddr.sin_port));
        setenv("SERVER_PORT", portbuf, 1);
        execl(path.c_str(), path.c_str(), (char*)NULL);
        return 1;
        _exit(127);
    }else{
        close(inpipe[0]);
        close(outpipe[1]);
        int written=0;
        int remain=Re.body().size();
        const char* Body_=Re.body().c_str();
        while(remain){
            int n=write(inpipe[1],Body_+written,remain);
            if(n<0)break;
            written+=n;
            remain-=n;
        }close(inpipe[1]);
        char buf[8192];
        int n;
        string ans;
        while((n=read(outpipe[0],buf,sizeof(buf)))>0)ans.append(buf,buf+n);
        close(outpipe[0]);

        if(ans.rfind("HTTP/",0)!=0){
            std::string status_line = "200 OK";
            auto hdr_end = ans.find("\r\n\r\n");
            auto p = ans.find("Status:");
            if (p != std::string::npos && (hdr_end == std::string::npos || p < hdr_end)) {
                auto line_end = ans.find("\r\n", p);
                auto val = ans.substr(p + 7, line_end - (p + 7));
                auto b = val.find_first_not_of(" \t"); auto e = val.find_last_not_of(" \t");
                if (b != std::string::npos) status_line = val.substr(b, e - b + 1);
            }
            if (hdr_end == std::string::npos) ans.append("\r\n\r\n");
            ans = "HTTP/1.1 " + status_line + "\r\n" + ans;
        }if (ans.find("\r\nConnection:") == std::string::npos)
            ans.insert(ans.find("\r\n\r\n"), "\r\nConnection: close");
        if (ans.find("\r\nDate:") == std::string::npos)
            ans.insert(ans.find("\r\n\r\n"), "\r\nDate: " + nowDate());
        int status=0;
        waitpid(pid_,&status,0);
        ServerSend(ans,Clientsocket);
        closesocket(Clientsocket);
        return 1;
    }
}
void HttpServer::handleGET(int ClientSocket,HttpRequest& Re_){
    string AimFile;
        bool is_find=0;
        /*if(Re_.url()==""||Re_.url()=="/"){
            for (vector<string>::iterator it = default_doc.begin(); it != default_doc.end(); ++it){
                if(filesystem::exists(*it)){
                    string tmp=*it;
                    AimFile=*it,is_find=1;
                    if(tmp.size() >= 4 && tmp.substr(tmp.size()-4) == ".php"){
                        Re_.seturl("/"+tmp);
                        handlephp(ClientSocket,"./"+tmp,Re_);
                    }
                    break;
                }
            }
        }else*/
        if(filesystem::exists(Re_.url()))is_find=1,AimFile=Re_.url();
        HttpResponse ReS;
        if(!is_find){
            handle404(ClientSocket);
            return;
        }
        auto pos=AimFile.find_last_of('.');
        if(pos==std::string::npos)ReS.setHeader("Content-Type","application/octet-stream");
        else{
            std::string Type=AimFile.substr(pos+1);
            if(Type=="html"||Type=="htm")ReS.setHeader("Content-Type","text/html; charset=utf-8");
            else if(Type=="js")ReS.setHeader("Content-Type","application/javascript");
            else if(Type=="json")ReS.setHeader("Content-Type","application/json");
            else if(Type=="png")ReS.setHeader("Content-Type","image/png");
            else if(Type=="txt")ReS.setHeader("Content-Type","text/plain; charset=utf-8");
            else if(Type=="jpg"||Type=="jpeg")ReS.setHeader("Content-Type","image/jpeg");
            else if(Type=="css")ReS.setHeader("Content-Type","text/css");
            else ReS.setHeader("Content-Type","application/octet-stream");
        }
        int SendedSize=0;ifstream fin;
        fin.open(AimFile,ios::in | ios::binary);
        string buf;
        char c;
        if(fin){ buf.assign((istreambuf_iterator<char>(fin)), {}); }
        ReS.setBody(buf);
        //if(!fin.is_open())cout<<"!!!";
        ReS.setHeader("Content-Length",to_string(buf.size()));
        ReS.setHeader("Date",nowDate());
        if(!ServerSend(ReS.AllContent(),ClientSocket));
        closesocket(ClientSocket);
        return;
}
int HttpServer::ServerSend(string str,int ClientSocket){
    int SendedSize=0,total=str.size();
    const char* Sending=str.c_str();
    while(SendedSize<total){
        int a=send(ClientSocket,Sending+SendedSize,total-SendedSize,0);
        if(a<0){
            if (errno == EINTR) continue;
            closesocket(ClientSocket);
            return -1;
        }if(a==0)break;
        SendedSize+=a;
    }return SendedSize;
}
void HttpServer::handleClient(int ClientSocket){ 
    char buf[8192];
    string rec;
    int n=0,ed=-1;
    while((n=recv(ClientSocket,buf,sizeof(buf),0))>0){
        if(n <= 0){ closesocket(ClientSocket); return; }
        rec.append(buf,n);
        auto edt=rec.find("\r\n\r\n");
        if(edt!=string::npos){ed=edt+4;break;}
        if(rec.size() > 64*1024){ closesocket(ClientSocket); return; }
    }
    HttpRequest Re_(rec.substr(0,ed));
    cout<<rec<<endl;
    int Rsize=-1;
    if(Re_.header_["Content-Length"]!=""){try{Rsize=stoi(Re_.header_["Content-Length"]);}catch(...){Rsize=0;}}
    string BODY;
    if(Rsize>0){
        int already=rec.size()-ed;
        if(already){
            BODY.append(rec.data()+ed,already>Rsize?Rsize:already);
        }
        int tmp=0,remain=Rsize-BODY.size();
        while (remain&&(tmp=recv(ClientSocket,buf,remain>sizeof(buf)?sizeof(buf):remain,0))>0){
            if(tmp<=0){closesocket(ClientSocket); return;}
            BODY.append(buf,tmp);
            remain-=tmp;
        }Re_.setbody(BODY);
    }
        if(Re_.url()==""||Re_.url()=="/"){
            for (vector<string>::iterator it = default_doc.begin(); it != default_doc.end(); ++it){
                if(filesystem::exists(*it)){
                    string tmp=*it;
                    //AimFile=*it,is_find=1;
                    Re_.seturl("/"+tmp);
                    break;
                }
            }
        }const std::string& url = Re_.url();
    bool is_php = url.size() >= 4 && url.substr(url.size()-4) == ".php";
    if(Re_.method()=="GET"&&!is_php){
        handleGET(ClientSocket,Re_);
    }else{
        if(is_php){
            std::string scriptPath = (url.size() && url[0]=='/') ? ("." + url) : url;
            if (!std::filesystem::exists(scriptPath)){handle404(ClientSocket);return;}
            handlephp(ClientSocket,scriptPath,Re_);
            return;
        }
        string CGIpath="./cgi-bin",CGIname;
        for (const auto& entry : filesystem::directory_iterator(CGIpath)){
            string Filepath=entry.path().string();
            if(Filepath.find(".cgi")!=string::npos){
                CGIpath=Filepath;
                CGIname=std::filesystem::path(Filepath).filename().string();
                break;
            }
        }
        handleCGI(ClientSocket,CGIpath,CGIname,Re_);
    }
}
void HttpServer::handle404(int ClientSocket){
    std::string AimFile;
    for (vector<string>::iterator it = _404file.begin(); it != _404file.end(); ++it){
                if(filesystem::exists(*it)){AimFile=*it;break;}
            }ifstream Fin;
            Fin.open(AimFile,ios::in|ios::binary);
            HttpResponse ReS=HttpResponse(404,"NotFound");
            string buf;
            char c;
            if(Fin){ buf.assign((istreambuf_iterator<char>(Fin)), {}); }
            else { buf = "404 Not Found\n"; }
            if(AimFile.find(".css")!=string::npos)ReS.setHeader("Content-Type","text/css");
            else ReS.setHeader("Content-Type","text/html;charset=utf-8");
            ReS.setHeader("Content-Length", to_string(buf.size()));
            ReS.setHeader("Date", nowDate());
            ReS.setBody(buf);
            //cout<<ReS.AllContent()<<endl;
            ServerSend(ReS.AllContent(),ClientSocket);
            closesocket(ClientSocket);
}
int HttpServer::handlephp(int ClientSocket,std::string scriptPath,HttpRequest& Re){
    int inpipe[2],outpipe[2];
    if(pipe(inpipe)<0||pipe(outpipe)<0) return -1;
    pid_t pid_=fork();
    if(pid_<0)return -1;
    else if(pid_==0){
        dup2(inpipe[0],STDIN_FILENO);
        dup2(outpipe[1],STDOUT_FILENO);
        dup2(outpipe[1],STDERR_FILENO);
        close(inpipe[1]);
        close(outpipe[0]);
        close(inpipe[0]);
        close(outpipe[1]);
        setenv("GATEWAY_INTERFACE","CGI/1.1",1);
        setenv("REQUEST_URI",Re.priurl().c_str(),1);
        setenv("REQUEST_METHOD",Re.method().c_str(),1);
        setenv("QUERY_STRING",Re.query().c_str(),1); 
        setenv("SCRIPT_NAME",Re.url().c_str(),1);
        setenv("REDIRECT_STATUS","200",1);
        setenv("SCRIPT_FILENAME", scriptPath.c_str(), 1);
        /*for(auto i=Re.header_.begin();i!=Re.header_.end();i++){
            setenv(i->first.c_str(),i->second.c_str(),1);
        }*/setenv("SERVER_PROTOCOL","HTTP/1.1",1);
        char cwd[4096]; if (getcwd(cwd, sizeof(cwd))) setenv("DOCUMENT_ROOT", cwd, 1);
        auto cl = Re.header_["Content-Length"];
        if(!cl.empty()) setenv("CONTENT_LENGTH", cl.c_str(), 1); 
        auto ct = Re.header_["Content-Type"];   
        if(!ct.empty()) setenv("CONTENT_TYPE",  ct.c_str(), 1); 
        char portbuf[16]; snprintf(portbuf, sizeof(portbuf), "%d", ntohs(serverAddr.sin_port));
        setenv("SERVER_PORT", portbuf, 1);
        for (auto &kv : Re.header_){
            std::string key = kv.first;
            if (key == "Content-Length" || key == "Content-Type") continue;
            setenv(to_cgi_env_key(key).c_str(), kv.second.c_str(), 1);
        }sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        if (getpeername(ClientSocket, (sockaddr*)&peer, &plen) == 0) {
            char ip[64]; if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip))) setenv("REMOTE_ADDR", ip, 1);
            char rport[16]; snprintf(rport, sizeof(rport), "%d", ntohs(peer.sin_port)); setenv("REMOTE_PORT", rport, 1);
        }
        execlp("php-cgi","php-cgi", (char*)NULL);
        _exit(127);
    }else{
        close(inpipe[0]);
        close(outpipe[1]);
        int written=0;
        int remain=Re.body().size();
        std::string TTT=Re.body();
        const char* Body_=TTT.c_str();
        while(remain){
            int n=write(inpipe[1],Body_+written,remain);
            if(n<0)break;
            written+=n;
            remain-=n;
        }
        close(inpipe[1]);
        char buf[8192];
        int n;
        string ans;
        while((n=read(outpipe[0],buf,sizeof(buf)))>0)ans.append(buf,buf+n);
        close(outpipe[0]);
        if(ans.rfind("HTTP/",0)!=0){
            std::string status_line = "200 OK";
            auto hdr_end = ans.find("\r\n\r\n");
            auto p = ans.find("Status:");
            if (p != std::string::npos && (hdr_end == std::string::npos || p < hdr_end)) {
                auto line_end = ans.find("\r\n", p);
                auto val = ans.substr(p + 7, line_end - (p + 7));
                auto b = val.find_first_not_of(" \t"); auto e = val.find_last_not_of(" \t");
                if (b != std::string::npos) status_line = val.substr(b, e - b + 1);
            }
            if (hdr_end == std::string::npos) ans.append("\r\n\r\n");
            ans = "HTTP/1.1 " + status_line + "\r\n" + ans;
        }if (ans.find("\r\nConnection:") == std::string::npos)
            ans.insert(ans.find("\r\n\r\n"), "\r\nConnection: close");
        if (ans.find("\r\nDate:") == std::string::npos)
            ans.insert(ans.find("\r\n\r\n"), "\r\nDate: " + nowDate());
        int status=0;
        waitpid(pid_,&status,0);
        ServerSend(ans,ClientSocket);
        closesocket(ClientSocket);
        return 1;
    }
}
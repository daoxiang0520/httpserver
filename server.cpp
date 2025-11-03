#include<iostream>
#include<string>
#include<thread>
#include<mutex>
#include"httpserver.h"
#include"../threadpool/ThreadPool.h"
using namespace std;
int ui(int a){
    return a;
}
int main(int argc, char *argv[]){
    //std::function<void()> f(ui);
    int port=DEFAULT_PORT;
    for(int i=1;i<argc;i++){
        string tmp=argv[i];
        if(tmp.substr(0,2)=="-p:"){
            try{
                port=stoi(tmp.substr(2));
            }catch(...){
                perror("WRONG ARGUMENT");
                return 0;
            }
        }
    }
    try {
        HttpServer Server(port);
        Server.prestart();
        Server.run();
    } catch (const std::exception &e) {
        std::cerr << "uncaught std::exception: " << e.what() << std::endl;
        return 1;
    } catch (const char *s) {
        std::cerr << "uncaught exception (char const*): " << s << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "uncaught unknown exception" << std::endl;
        return 3;
    }
}
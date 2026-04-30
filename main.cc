#include "KeywordProcessor.h"
#include "Logger.h"
#include "PageProcessor.h"
#include "server.h"

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

int main()
{
    initLogger();
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(8080);
    
    KeyWordProcessor k;
    k.process("./data/corpus/CN/", "./data/corpus/EN/");
    PageProcessor p;
    p.process("./data/corpus/webpages");

    Server server(&loop, addr);
    server.start();
    loop.loop();
    shutdownLogger();
}
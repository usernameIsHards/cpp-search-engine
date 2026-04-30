#include "Logger.h"
#include "server.h"
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

int main()
{
    initLogger();
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(8080);
    Server server(&loop, addr);
    server.start();
    loop.loop();
    shutdownLogger();
}
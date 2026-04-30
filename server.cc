#include "server.h"
#include "DirectoryScanner.h"
#include "KeywordProcessor.h"
#include "Logger.h"

#include <fstream>
#include <map>
#include <ostream>
#include <set>
using namespace std;
using namespace muduo;
using namespace muduo::net;

Server::Server(EventLoop *loop, const InetAddress &addr)
    : server_(loop, addr, "SearchServer"),
      codec_([this](const muduo::net::TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg,
                    muduo::Timestamp ts) { onMessage(conn, msg, ts); })
{
    server_.setConnectionCallback([this](const TcpConnectionPtr &conn) { onConnection(conn); });

    server_.setMessageCallback(
        [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp ts) { codec_.onMessage(conn, buf, ts); });
}
void Server::start()
{
    server_.start();
}

void Server::onConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (conn->connected())
        LOG_INFO("客户端连接: {}", conn->peerAddress().toIpPort());
    else
        LOG_INFO("客户端断开: {}", conn->peerAddress().toIpPort());
}

void Server::onMessage(const muduo::net::TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg,
                       muduo::Timestamp receiveTime)
{
    //  用 using 简化长名字
    using MT = LengthHeaderCodec::MessageType;

    switch (msg.type)
    {
    case MT::RECOMMEND: {
        // 调用关键字推荐器
        string result = recommender_.recommend(msg.value, 5);
        LengthHeaderCodec::Message reply;
        reply.type = MT::RECOMMEND;
        reply.value = result;
        codec_.send(conn, reply);
        break;
    }
    case MT::SEARCH: {
        // 调用网页搜索器
        string result = searcher_.search(msg.value, 10);
        LengthHeaderCodec::Message reply;
        reply.type = MT::SEARCH;
        reply.value = result;
        codec_.send(conn, reply);
        break;
    }
    default:
        LOG_WARN("Unknown message type");
        conn->shutdown();
        break;
    }
}

#if 0
int main()
{
    initLogger();

    LOG_INFO("项目开始运行");
    KeyWordProcessor k;

    k.process("./data/corpus/CN/", "./data/corpus/EN/");

    LOG_INFO("项目结束运行");
    return 0;
}
#endif
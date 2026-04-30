#include "LengthHeaderCodec.h"
#include <iostream>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <string>
#include <thread>

using namespace muduo;
using namespace muduo::net;

class SearchClient
{
  public:
    SearchClient(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop), client_(loop, serverAddr, "SearchClient"),
          codec_([this](const TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg, Timestamp ts) {
              onMessage(conn, msg, ts);
          })
    {
        client_.setConnectionCallback([this](const TcpConnectionPtr &conn) { onConnection(conn); });

        client_.setMessageCallback(
            [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp ts) { codec_.onMessage(conn, buf, ts); });
    }

    void connect()
    {
        client_.connect();
    }

  private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            conn_ = conn;
            std::cout << "已连接服务器，请输入查询内容（输入 quit 退出）：\n";
            // 启动输入线程
            inputThread_ = std::thread([this]() { inputLoop(); });
            inputThread_.detach();
        }
        else
        {
            conn_.reset();
            loop_->quit();
        }
    }

    void onMessage(const TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg, Timestamp ts)
    {
        using MT = LengthHeaderCodec::MessageType;
        if (msg.type == MT::RECOMMEND)
        {
            std::cout << "\n推荐关键词：\n" << msg.value << "\n";
        }
        else if (msg.type == MT::SEARCH)
        {
            std::cout << "\n搜索结果：\n" << msg.value << "\n";
        }
        std::cout << "\n请输入：";
        std::cout.flush();
    }
    void inputLoop()
    {
        std::string line;
        std::cout << "命令：直接输入内容=搜索，输入 r <词>=关键字推荐，quit=退出\n";
        while (std::getline(std::cin, line))
        {
            if (line == "quit")
            {
                loop_->quit();
                break;
            }
            if (line.empty())
                continue;

            auto conn = conn_;
            if (!conn)
                continue;

            LengthHeaderCodec::Message msg;

            if (line.size() > 2 && line[0] == 'r' && line[1] == ' ')
            {
                // 推荐模式
                msg.type = LengthHeaderCodec::MessageType::RECOMMEND;
                msg.value = line.substr(2);
            }
            else
            {
                // 搜索模式
                msg.type = LengthHeaderCodec::MessageType::SEARCH;
                msg.value = line;
            }

            codec_.send(conn, msg);
        }
    }

    EventLoop *loop_;
    TcpClient client_;
    LengthHeaderCodec codec_;
    TcpConnectionPtr conn_;
    std::thread inputThread_;
};

int main(int argc, char *argv[])
{
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? atoi(argv[2]) : 8080;

    EventLoop loop;
    InetAddress serverAddr(ip, port);
    SearchClient client(&loop, serverAddr);
    client.connect();
    loop.loop();
}
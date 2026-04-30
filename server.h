#pragma once
#include "KeywordRecommender.h"
#include "LengthHeaderCodec.h"
#include "WebSearcher.h"
#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <set>

#include "LengthHeaderCodec.h"

class Server
{
  public:
    Server(muduo::net::EventLoop *loop, const muduo::net::InetAddress &listenAddr);
    ~Server() = default;

    void start();

  private:
    void onConnection(const muduo::net::TcpConnectionPtr &conn);

    void onMessage(const muduo::net::TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg,
                   muduo::Timestamp receiveTime);

  private:
    std::set<muduo::net::TcpConnectionPtr> conns_;
    muduo::net::TcpServer server_;
    LengthHeaderCodec codec_;
    KeywordRecommender recommender_;
    WebSearcher searcher_;
};
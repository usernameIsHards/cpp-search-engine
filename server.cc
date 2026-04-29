#include "DirectoryScanner.h"
#include "KeywordProcessor.h"

#include "server.h"
#include <fstream>
#include <map>
#include <ostream>
#include <set>

#include "Logger.h"
using namespace std;

void Server::onMessage(const muduo::net::TcpConnectionPtr &conn, const LengthHeaderCodec::Message &msg,
                       muduo::Timestamp receiveTime)
{
    //  用 using 简化长名字
    using MT = LengthHeaderCodec::MessageType;

    switch (msg.type)
    {
    case MT::RECOMMEND:
        // 调用关键字推荐器
        break;
    case MT::SEARCH:
        // 调用网页搜索器
        break;
    default:
        LOG_WARN("Unknown message type");
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
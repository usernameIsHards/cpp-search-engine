#pragma once
#include <functional>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>
#include <string>

class LengthHeaderCodec
{

  public:
    enum class MessageType : uint8_t // 强类型枚举，底层就是uint8_t（1字节）
    {
        UNKNOWN = 0,
        RECOMMEND = 1, // 关键字推荐
        SEARCH = 2     // 网页搜索
    };

    //  安全转换：uint8_t → MessageType
    inline MessageType toMessageType(uint8_t val)
    {
        switch (val)
        {
        case static_cast<uint8_t>(MessageType::RECOMMEND):
            return MessageType::RECOMMEND;
        case static_cast<uint8_t>(MessageType::SEARCH):
            return MessageType::SEARCH;
        default:
            return MessageType::UNKNOWN;
        }
    }

    struct Message
    {
        MessageType type; // 消息的类型

        std::string value; // 消息的内容
        // uint32_t length; // value 的长度 //可以用下面方法替代
        // 提供辅助方法
        uint32_t size() const
        {
            return static_cast<uint32_t>(value.size());
        }
    };

    using MessageCallback = std::function<void(const muduo::net::TcpConnectionPtr &,
                                               const Message &, // ← 传完整 Message
                                               muduo::Timestamp)>;

    explicit LengthHeaderCodec(const MessageCallback &cb) : messageCallback_(cb)
    {
    }

    void send(const muduo::net::TcpConnectionPtr &conn, const Message &msg);

    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buff, muduo::Timestamp time);

  private:
    // 协议头大小: type(1) + length(4) = 5字节
    static const size_t HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t);

    MessageCallback messageCallback_;
};
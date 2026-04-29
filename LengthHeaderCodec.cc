#include "LengthHeaderCodec.h"
#include <endian.h>

using namespace muduo::net;
using namespace muduo;
void LengthHeaderCodec::send(const TcpConnectionPtr &conn, const Message &msg)
{
    Buffer buff;

    // type(1字节) + length(4字节) + value
    // 自动转网络字节序
    buff.appendInt8(static_cast<int8_t>(msg.type));

    // 自动转网络字节序
    buff.appendInt32(static_cast<int32_t>(msg.value.size()));
    buff.append(msg.value.data(), msg.value.size());

    conn->send(&buff);
}

void LengthHeaderCodec::onMessage(const TcpConnectionPtr &conn, Buffer *buff, Timestamp receiveTime)
{

    while (buff->readableBytes() >= HEADER_SIZE)
    {
        // 读type(不移动readerIndex)
        //  读 type，直接转成枚举
        uint8_t rawType = static_cast<uint8_t>(buff->peekInt8());
        MessageType type = toMessageType(rawType); // ← 安全转换

        //  提前过滤非法类型
        if (type == MessageType::UNKNOWN)
        {
            conn->shutdown(); // 非法消息，断开连接
            break;
        }

        // 读取length
        const char *data = buff->peek();

        // 先跳过type  然后强制类型转换   大端解引用     转化成主机字节序
        uint32_t length =
            muduo::net::sockets::networkToHost32(*reinterpret_cast<const uint32_t *>(data + sizeof(uint8_t)));
        if (buff->readableBytes() < HEADER_SIZE + length)
            break;

        // 跳过头部，取出value
        buff->retrieve(HEADER_SIZE);
        string value(buff->peek(), length);
        buff->retrieve(length);

        Message msg;
        msg.type = type;
        msg.value = std::move(value);
        messageCallback_(conn, msg, receiveTime);
    }
}
#pragma once

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

inline void initLogger()
{
    //  创建 logs 目录
    system("mkdir -p logs");

    //  控制台 sink（info 以上才显示，不刷屏）
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);

    //  文件 sink（debug 以上全记录）
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/search.log",
                                                                            1024 * 1024 * 5, // 单个文件最大 5MB
                                                                            3                // 最多保留 3 个文件
    );
    file_sink->set_level(spdlog::level::debug);

    // 合并两个 sink
    auto logger = std::make_shared<spdlog::logger>("default", spdlog::sinks_init_list{console_sink, file_sink});

    logger->set_level(spdlog::level::debug); // logger 本身放行所有级别
    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%-8l%$] %v");
    spdlog::flush_on(spdlog::level::warn); // warn 以上立即刷入文件

    spdlog::set_default_logger(logger);
    spdlog::info("日志初始化完成 -> logs/search.log");
}

inline void shutdownLogger()
{
    spdlog::shutdown();
}

#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
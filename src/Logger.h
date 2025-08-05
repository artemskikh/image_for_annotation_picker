#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>
#include <memory>

class Logger
{
public:
    static void initialize()
    {
        if (s_initialized)
            return;

        try
        {
            // Create console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::debug);
            console_sink->set_pattern("[%T] [%^%l%$] %v");

            // Create file sink
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("annotation_picker.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %T] [%l] [%s:%#] %v");

            // Create logger with both sinks
            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            auto logger = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::info);

            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::debug);

            s_initialized = true;

            SPDLOG_INFO("Logger initialized successfully");
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            printf("Log initialization failed: %s\n", ex.what());
        }
    }

    static void setLevel(spdlog::level::level_enum level)
    {
        if (auto logger = spdlog::default_logger())
        {
            logger->set_level(level);
        }
    }

private:
    inline static bool s_initialized = false;
};

// Convenience macros
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#endif // LOGGER_H

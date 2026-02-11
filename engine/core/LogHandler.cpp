module;

export module LogHandler;
import std;

enum class LogLevel
{
    INFO,
    PERFORMANCE,
    WARNING,
    ERROR
    
};

namespace AngelBase
{
    using string = std::string;
}

static AngelBase::string ToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::INFO:
        return AngelBase::string("INFO");
        break;
        case LogLevel::PERFORMANCE:
        return AngelBase::string("PERFORMANCE");
        break;
        case LogLevel::WARNING:
        return AngelBase::string("WARNING");
        break;
        case LogLevel::ERROR:
        return AngelBase::string("ERROR");
        break;
    }
}

class LogHandler
{
    
};
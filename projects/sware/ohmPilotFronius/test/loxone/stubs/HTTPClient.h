#pragma once
#include "energySource.h"
#include <vector>
#include <cassert>
constexpr int HTTP_CODE_OK = 200;
struct Reply { int code; String body; };
class HTTPClient {
public:
    static inline std::vector<Reply> replies;
    static inline std::vector<String> urls;
    static inline unsigned next = 0;
    static inline int closed = 0;
    void setConnectTimeout(int) {}
    void setTimeout(int) {}
    bool begin(const String &url) { urls.push_back(url); return true; }
    void setAuthorization(const char*, const char*) {}
    int GET() { assert(next < replies.size()); return replies[next].code; }
    String getString() { return replies[next].body; }
    void end() { ++next; ++closed; }
};

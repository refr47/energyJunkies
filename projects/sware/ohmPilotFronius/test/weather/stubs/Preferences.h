#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
class Preferences {
    std::string space;
    static std::map<std::string, std::vector<unsigned char>> &store() {
        static std::map<std::string, std::vector<unsigned char>> value;
        return value;
    }
public:
    static unsigned &writes() { static unsigned count = 0; return count; }
    bool begin(const char *name, bool) { space = name; return true; }
    size_t getBytesLength(const char *key) { return store()[space + key].size(); }
    size_t getBytes(const char *key, void *output, size_t length) {
        const auto &value = store()[space + key];
        if (value.size() != length) return 0;
        std::memcpy(output, value.data(), length);
        return length;
    }
    size_t putBytes(const char *key, const void *input, size_t length) {
        const auto *p = static_cast<const unsigned char*>(input);
        store()[space + key] = std::vector<unsigned char>(p, p + length);
        ++writes();
        return length;
    }
    unsigned getUInt(const char *key, unsigned fallback) {
        unsigned value;
        return getBytes(key, &value, sizeof(value)) ? value : fallback;
    }
    void putUInt(const char *key, unsigned value) { putBytes(key, &value, sizeof(value)); }
    static void clear() { store().clear(); writes() = 0; }
};

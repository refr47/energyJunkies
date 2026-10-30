#include "devices/loxoneResponse.h"
#include <cassert>
#include <iostream>

int main()
{
    double value = 42;
    assert(loxoneParseValue(R"({"LL":{"Code":"200","value":"-1234.5"}})", value));
    assert(value == -1234.5);
    assert(loxoneParseValue(R"({"LL":{"Code":200,"value":0}})", value));
    assert(value == 0);
    assert(loxoneParseValue(R"({"LL":{"Code":200,"value":" 98.5 "}})", value));
    assert(value == 98.5);
    const char* invalid[] = {
        nullptr, "", "not json", "{}", "[]",
        R"({"LL":{"Code":401,"value":123}})",
        R"({"LL":{"Code":"200x","value":123}})",
        R"({"LL":{"value":123}})",
        R"({"LL":{"Code":200}})",
        R"({"LL":{"Code":200,"value":null}})",
        R"({"LL":{"Code":200,"value":true}})",
        R"({"LL":{"Code":200,"value":""}})",
        R"({"LL":{"Code":200,"value":"?"}})",
        R"({"LL":{"Code":200,"value":"12W"}})",
        R"({"LL":{"Code":200,"value":"NaN"}})",
        R"({"LL":{"Code":200,"value":"Infinity"}})",
        R"({"LL":{"Code":200,"value":"1e999"}})",
    };
    for (const char* payload : invalid) {
        value = 42;
        assert(!loxoneParseValue(payload, value));
        assert(value == 42);
    }
    std::cout << "Loxone response tests passed\n";
}

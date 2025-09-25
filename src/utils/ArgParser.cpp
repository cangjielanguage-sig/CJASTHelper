#include "utils/ArgParser.h"
#include <sstream>

/// ArgParser
ArgumentParser::ArgumentParser(ConStrMap<StrSet>& validOptions) : validOptions(validOptions)
{
}

void ArgumentParser::Parse(ConStrVec& args)
{
    for (const auto& arg : args) {
        if (arg.rfind("--", 0) != 0) {
            throw std::invalid_argument("ArgumentParser: Invalid argument format: " + arg);
        }

        size_t equalPos = arg.find('=');
        if (equalPos == Str::npos) {
            throw std::invalid_argument("ArgumentParser: Missing '=' in argument: " + arg);
        }

        Str option = arg.substr(2, equalPos - 2);
        Str valueStr = arg.substr(equalPos + 1);

        StrVec values;
        std::stringstream ss(valueStr);
        Str value;
        while (std::getline(ss, value, ',')) {
            values.push_back(value);
        }
        ValidateOption(option, values);
        parsedOptions[option] = values;
    }
}

Str ArgumentParser::GetSingleValue(ConStr& option) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end() || it->second.size() != 1) {
        throw std::invalid_argument("ArgumentParser: Invalid option: " + option);
    }
    return it->second[0];
}

Str ArgumentParser::GetSingleValue(ConStr& option, ConStr& dv) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end() || it->second.size() != 1) {
        return dv;
    }
    return it->second[0];
}

StrVec ArgumentParser::GetMultiValue(ConStr& option) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end()) {
        return {};
    }
    return it->second;
}

void ArgumentParser::ValidateOption(ConStr& option, ConStrVec& values) const
{
    auto it = validOptions.find(option);
    if (it == validOptions.end()) {
        throw std::invalid_argument("ArgumentParser: Invalid option: " + option);
    }
    // 未配置有效选项值，默认不限制
    if (it->second.empty()) {
        return;
    }
    for (const auto& value : values) {
        if (it->second.find(value) == it->second.end()) {
            throw std::invalid_argument("ArgumentParser: Invalid value for option " + option + ": " + value);
        }
    }
}

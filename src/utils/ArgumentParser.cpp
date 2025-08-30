#include "utils/ArgumentParser.h"
#include <sstream>
#include <stdexcept>

InvalidArgumentException::InvalidArgumentException(const std::string& msg) noexcept : message(msg)
{
}

const char* InvalidArgumentException::what() const noexcept
{
    return message.c_str();
}

ArgumentParser::ArgumentParser(const std::unordered_map<std::string, std::unordered_set<std::string>>& validOptions)
    : validOptions(validOptions)
{
}

void ArgumentParser::Parse(const std::vector<std::string>& args)
{
    for (const auto& arg : args) {
        if (arg.rfind("--", 0) != 0) {
            throw InvalidArgumentException("Invalid argument format: " + arg);
        }

        size_t equalPos = arg.find('=');
        if (equalPos == std::string::npos) {
            throw InvalidArgumentException("Missing '=' in argument: " + arg);
        }

        std::string option = arg.substr(2, equalPos - 2);
        std::string valueStr = arg.substr(equalPos + 1);

        std::vector<std::string> values;
        std::stringstream ss(valueStr);
        std::string value;
        while (std::getline(ss, value, ',')) {
            values.push_back(value);
        }
        ValidateOption(option, values);
        parsedOptions[option] = values;
    }
}

std::string ArgumentParser::GetSingleValue(const std::string& option, const std::string& dv) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end() || it->second.size() != 1) {
        return dv;
    }
    return it->second[0];
}

std::vector<std::string> ArgumentParser::GetMultiValue(const std::string& option) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end()) {
        return {};
    }
    return it->second;
}

void ArgumentParser::ValidateOption(const std::string& option, const std::vector<std::string>& values) const
{
    auto it = validOptions.find(option);
    if (it == validOptions.end()) {
        throw InvalidArgumentException("Invalid option: " + option);
    }
    // 未配置有效选项值，默认不限制
    if (it->second.empty()) {
        return;
    }
    for (const auto& value : values) {
        if (it->second.find(value) == it->second.end()) {
            throw InvalidArgumentException("Invalid value for option " + option + ": " + value);
        }
    }
}
#ifndef ARGUMENT_PARSER_H
#define ARGUMENT_PARSER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Invalid Argument Exception
class InvalidArgumentException : public std::exception {
private:
    std::string message;

public:
    explicit InvalidArgumentException(const std::string& msg) noexcept;
    const char* what() const noexcept override;
};

/**
 * @class ArgumentParser
 * @brief 用于解析命令行参数的类。
 */
class ArgumentParser {
public:
    /**
     * @brief 构造函数，初始化合法选项及其取值范围。
     * @param validOptions 合法选项及其对应的取值集合。
     */
    explicit ArgumentParser(const std::unordered_map<std::string, std::unordered_set<std::string>>& validOptions);

    /**
     * @brief 解析命令行参数。
     * @param args 命令行参数列表。
     */
    void Parse(const std::vector<std::string>& args);

    /**
     * @brief 获取单值选项的值。
     * @param option 选项名称。
     * @return 选项的值。
     */
    std::string GetSingleValue(const std::string& option) const;

    /**
     * @brief 获取多值选项的值。
     * @param option 选项名称。
     * @return 选项的值列表。
     */
    std::vector<std::string> GetMultiValue(const std::string& option) const;

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> validOptions; // 合法选项及其取值范围
    std::unordered_map<std::string, std::vector<std::string>> parsedOptions;       // 已解析的选项及其值

    /**
     * @brief 检查选项和值的合法性。
     * @param option 选项名称。
     * @param values 选项的值列表。
     */
    void ValidateOption(const std::string& option, const std::vector<std::string>& values) const;
};

#endif // ARGUMENT_PARSER_H
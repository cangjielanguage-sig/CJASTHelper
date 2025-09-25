/**
 * @file
 *
 * This file declares the ArgParser.
 */

#include "utils/types/TypeAlias.h"

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
    explicit ArgumentParser(ConStrMap<StrSet>& validOptions);

    /**
     * @brief 解析命令行参数。
     * @param args 命令行参数列表。
     */
    void Parse(ConStrVec& args);

    /**
     * @brief 获取单值选项的值。
     * @param option 选项名称。
     * @return 选项的值, 如果不存在则抛异常。
     */
    Str GetSingleValue(ConStr& option) const;

    /**
     * @brief 获取单值选项的值。
     * @param option 选项名称。
     * @param dv 如果不存在的话，返回默认值。
     * @return 选项的值。
     */
    Str GetSingleValue(ConStr& option, ConStr& dv) const;

    /**
     * @brief 获取多值选项的值，不存在返回空列表。
     * @param option 选项名称。
     * @return 选项的值列表。
     */
    StrVec GetMultiValue(ConStr& option) const;

private:
    StrMap<StrSet> validOptions;  // 合法选项及其取值范围
    StrMap<StrVec> parsedOptions; // 已解析的选项及其值

    /**
     * @brief 检查选项和值的合法性。
     * @param option 选项名称。
     * @param values 选项的值列表。
     */
    void ValidateOption(ConStr& option, ConStrVec& values) const;
};

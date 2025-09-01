#include "utils/Cast.h"
#include <gtest/gtest.h>
#include <memory>

class Base {
public:
    virtual ~Base() = default;
};

class Derived : public Base {
public:
    int value = 42;
};

// 模拟具有get()方法的类型
template <typename T> class SmartPointerLike {
private:
    T* ptr;

public:
    explicit SmartPointerLike(T* p) : ptr(p)
    {
    }

    T* get() const
    {
        return ptr;
    }
};

// 基本测试用例
TEST(CastTest, BasicCast)
{
    EXPECT_TRUE(1 + 1 == 2);
    EXPECT_FALSE(2 * 2 == 5);
    EXPECT_EQ(2 + 2, 4);
}

// 测试源类型具有.get()方法的情况
TEST(CastTest, PointerLikeCast)
{
    auto derived = std::make_unique<Derived>();
    SmartPointerLike<Derived> wrapper(derived.get());

    // 从具有get()方法的类型转换到指针
    Base* basePtr = Cast<Base*>(wrapper);
    EXPECT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr, derived.get());

    // 从具有get()方法的类型转换到引用
    Base& baseRef = Cast<Base&>(wrapper);
    EXPECT_EQ(&baseRef, derived.get());
}

// 测试目标是指针，源是引用的情况
TEST(CastTest, ReferenceToPointerCast)
{
    Derived derived;

    // 从引用转换到指针
    Base* basePtr = Cast<Base*>(&derived);
    EXPECT_EQ(basePtr, &derived);

    // 从引用转换到相同类型的指针
    Derived* derivedPtr = Cast<Derived*>(&derived);
    EXPECT_EQ(derivedPtr, &derived);
}

// 测试目标是引用，源是指针的情况
TEST(CastTest, PointerToReferenceCast)
{
    Derived derived;

    // 从指针转换到引用
    Derived& derivedRef = Cast<Derived&>(&derived);
    EXPECT_EQ(&derivedRef, &derived);
}

// 测试源和目标都是指针的情况
TEST(CastTest, PointerToPointerCast)
{
    Derived derived;

    // 向上转换指针
    Base* basePtr = Cast<Base*>(&derived);
    EXPECT_EQ(basePtr, &derived);

    // 同类型指针转换
    Derived* anotherPtr = Cast<Derived*>(&derived);
    EXPECT_EQ(anotherPtr, &derived);

    // 同类型指针转换
    Derived* derivedPtr = Cast<Derived*>(basePtr);
    EXPECT_EQ(basePtr, derivedPtr);
}

// 测试源和目标都是引用的情况
TEST(CastTest, ReferenceToReferenceCast)
{
    Derived derived;

    // 向上转换引用
    Base& baseRef = Cast<Base&>(derived);
    EXPECT_EQ(&baseRef, &derived);

    // 同类型引用转换
    Derived& derivedRef = Cast<Derived&>(derived);
    EXPECT_EQ(&derivedRef, &derived);

    // 向下转换引用
    Derived& derivedRef1 = Cast<Derived&>(baseRef);
    EXPECT_EQ(&derivedRef1, &baseRef);
}

// 测试基本静态转换
TEST(CastTest, BasicStaticCast)
{
    int value = 42;

    // 值类型转换
    double doubleValue = static_cast<double>(value);
    EXPECT_EQ(doubleValue, 42.0);

    // 常量转换 (注意：不能从const引用转换为非const引用)
    const int& constRef = value;
    const int& ref = Cast<const int&>(constRef);
    EXPECT_EQ(&ref, &value);
}

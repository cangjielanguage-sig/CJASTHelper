#include "utils/Printer.h"
#include "wrapper/WrapperAst.h"
#include <gtest/gtest.h>

TEST(PrinterTest, ConceptTest)
{
    static_assert(dereferenceable<OwnedPtr<Decl>>);
    static_assert(count_deref_v<OwnedPtr<Decl>> == 1);
    // copy 构造被删掉了， 所以这里的check会失败
    // static_assert(std::convertible_to<std::remove_cvref_t<deref_t<OwnedPtr<Decl>>>, Decl>);
    // static_assert(std::convertible_to<Decl, Decl>);
    static_assert(deref_to_n<OwnedPtr<Decl>, Decl, 1>);
    static_assert(container_deref_to<std::vector<OwnedPtr<Decl>>, Decl>);

    std::vector<OwnedPtr<Decl>> con;
    printcc<Decl>(std::cout, con, [](const Decl& decl) {});
}

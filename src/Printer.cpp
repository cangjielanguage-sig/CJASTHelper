/**
 * @file
 *
 * This file implements the general Printer.
 */
#include "Printer.h"

/// public implementation
Printer::Printer(std::ostream& os, int indent) : os_(os), indent_(indent), currentIndent_(0), needIndent_(true)
{
}

void Printer::Indent()
{
    currentIndent_++;
}

Printer& Printer::PNL()
{
    os_ << std::endl;
    needIndent_ = true;
    return *this;
}

void Printer::Unindent()
{
    if (currentIndent_ > 0) {
        currentIndent_--;
    }
}

/// private implementation
void Printer::EnsureIndent()
{
    if (needIndent_) {
        os_ << std::string(currentIndent_ * indent_, ' ');
        needIndent_ = false;
    }
}

#include "Printer.h"

/// public implementation
Printer::Printer(std::ostream &os, int indent) : os_(os), indent_(indent), currentIndent_(0), needIndent_(true) {}

void Printer::indent()
{
    currentIndent_++;
}

Printer &Printer::pnl()
{
    os_ << std::endl;
    needIndent_ = true;
    return *this;
}

void Printer::unindent()
{
    if (currentIndent_ > 0) {
        currentIndent_--;
    }
}

/// private implementation
void Printer::ensureIndent()
{
    if (needIndent_) {
        os_ << std::string(currentIndent_ * indent_, ' ');
        needIndent_ = false;
    }
}

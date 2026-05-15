#pragma once
#include "prelude.h"
#include "rpt_symbol.h"

#include <cstring>

SSymbol::SSymbol(const char* string) {
	value = SSymbol::hash(string);
}

SSymbol::SSymbol() 
	: value(~0ULL)
{}

SSymbol::SSymbol(const SSymbol& other)
	: value(static_cast<u64>(other))
{}

SSymbol::SSymbol(u64 inValue)
	: value(inValue)
{}
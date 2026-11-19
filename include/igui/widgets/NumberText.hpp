// Numbers a person types and numbers shown to one, in one place (docs/ROUND11.md, section 3): what a numeric field accepts, and how a
// number is written. Both ignore the locale of the program: the decimal separator written is always '.', and the one read is '.' or ','.
#pragma once

#include "String.hpp"

namespace ig
{
namespace retained
{
namespace numtext
{

/// The number in `text` (`length` bytes), as a person types it into a numeric field.
/// Accepted: blanks around it, an optional sign, digits with AT MOST ONE decimal separator, '.' or ',' (so "1.5" and "1,5" are 1.5, ".5" and "5."
/// are 0.5 and 5, whatever the locale of the program is), and an optional exponent ("1e3", "2,5E-2"). The separator is a decimal point and nothing
/// else: "1,000" is 1, not one thousand, and a text with two separators ("1,000.5", "1.2.3") is not a number (the field keeps the value it had: a
/// text that could be read two ways is not read). Not accepted: nothing but blanks, "inf", "nan", hexadecimal, a unit or any other trailing text,
/// a value outside the double (1e999), more than 100 bytes. Returns false and leaves `out` alone then; the rounding is the nearest double, exact.
bool parseDecimal(const char* text, size_t length, double& out);
inline bool parseDecimal(const String& text, double& out)
{
    return parseDecimal(text.c_str(), text.size(), out);
}

/// parseDecimal, then the value brought into [lo, hi]: a typed number outside the field's range becomes its limit, never anything else (it
/// does not wrap and is not refused). `clamped` (may be null) says that it happened, for a field that shows it. False: not a number.
bool parseClamped(const String& text, double lo, double hi, double& out, bool* clamped = nullptr);

/// parseClamped for an integer field: the number is taken as parseDecimal does, brought into [lo, hi], and cut toward zero ("12.7" is 12).
/// An integer typed with more digits than a long long, or a power of ten, saturates at lo or hi like any other.
bool parseInteger(const String& text, long long lo, long long hi, long long& out, bool* clamped = nullptr);

/// The number a text STARTS with (blanks, sign, digits, '.', exponent: what strtod read in the "C" locale, with the same rounding, and for every
/// locale of the program), for sorting a column of cells that are numbers. Only '.' is a decimal point here: a cell is text an application wrote,
/// and a "," in it may be a thousands separator. False: it does not start with a number. (inf, nan and hexadecimal are numbers, as for strtod.)
bool leadingNumber(const String& text, double& out);

/// The same text as snprintf(out, cap, "%.*f", decimals, v) in the "C" locale (always '.'), and the same return value (the length the whole
/// text has, which may be more than cap - 1); about three times faster than snprintf for what a label is made of (decimals 0 to 9,
/// |v| < 1e15), snprintf itself for the rest.
int fixedInto(char* out, size_t cap, double v, int decimals);
/// The same as snprintf(out, cap, "%.*g", precision, v) in the "C" locale.
int generalInto(char* out, size_t cap, double v, int precision);
/// snprintf in the "C" locale: the number conversions (%f, %g, %e) write '.' whatever LC_NUMERIC the program has set, and nothing else changes
/// (a %s with a comma in it keeps it). For a label that is not hot enough to be worth its own conversion; same return value as snprintf.
int cformat(char* out, size_t cap, const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 3, 4)))
#endif
        ;
/// What printf wrote in a locale with another decimal separator, made '.' again: replaces that separator in text[0, length).
void pointToDot(char* text, size_t length);
/// The shortest text that reads back as exactly the same float (0.1f is "0.1"), '.' always.
String shortest(float v);
/// The shortest text that reads back as exactly the same double (0.1, not 0.10000000000000001), '.' always: for what a person is going to
/// edit, where "%.9g" showed digits that were never typed. Not for a documented format.
String shortest(double v);
/// The same as fixedInto, as a String.
String fixed(double v, int decimals);

} // namespace numtext
} // namespace retained
} // namespace ig

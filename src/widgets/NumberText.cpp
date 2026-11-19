#include "pch.hpp"

#include "NumberText.hpp"
#include "ct/numconv.hpp"

#include "stdarg.h"

namespace ig
{
namespace retained
{
namespace numtext
{

namespace
{

bool isBlank(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

} // namespace

bool parseDecimal(const char* text, size_t length, double& out)
{
    size_t begin = 0;
    size_t end = length;
    while (begin < end && isBlank(text[begin])) ++begin;
    while (end > begin && isBlank(text[end - 1])) --end;
    const size_t n = end - begin;
    if (n == 0 || n > 100) return false;

    // The syntax first (this is what keeps "inf", "nan" and "0x10", which the conversion below would read, out), copied with the separator
    // as '.': sign, digits with one separator and at least one digit around it, an exponent.
    char buf[104];
    size_t i = 0;
    size_t o = 0;
    if (text[begin] == '-' || text[begin] == '+') buf[o++] = text[begin + i++];
    int digits = 0;
    bool separator = false;
    for (; i < n; ++i)
    {
        const char c = text[begin + i];
        if (isDigit(c)) ++digits;
        else if (c == '.' || c == ',')
        {
            if (separator) return false;
            separator = true;
            buf[o++] = '.';
            continue;
        }
        else
            break;
        buf[o++] = c;
    }
    if (digits == 0) return false;
    if (i < n)
    {
        if (text[begin + i] != 'e' && text[begin + i] != 'E') return false;
        buf[o++] = 'e';
        ++i;
        if (i < n && (text[begin + i] == '-' || text[begin + i] == '+')) buf[o++] = text[begin + i++];
        int exponentDigits = 0;
        for (; i < n && isDigit(text[begin + i]); ++i, ++exponentDigits) buf[o++] = text[begin + i];
        if (exponentDigits == 0 || i != n) return false;
    }
    double value = 0.0;
    if (ct::parse_double(buf, buf + o, value) != buf + o) return false;
    if (!(value - value == 0.0)) return false; // 1e999
    out = value;
    return true;
}

bool parseClamped(const String& text, double lo, double hi, double& out, bool* clamped)
{
    double v = 0.0;
    if (clamped) *clamped = false;
    if (!parseDecimal(text, v)) return false;
    if (v < lo)
    {
        v = lo;
        if (clamped) *clamped = true;
    }
    else if (v > hi)
    {
        v = hi;
        if (clamped) *clamped = true;
    }
    out = v;
    return true;
}

bool parseInteger(const String& text, long long lo, long long hi, long long& out, bool* clamped)
{
    double v = 0.0;
    if (clamped) *clamped = false;
    if (!parseDecimal(text, v)) return false;
    // compared as doubles: the limits of a long long are not all doubles, and a double beyond them must end at the limit and not be cast
    const double dlo = static_cast<double>(lo);
    const double dhi = static_cast<double>(hi);
    long long result;
    if (v <= dlo)
    {
        result = lo;
        if (clamped && v < static_cast<double>(lo)) *clamped = true;
    }
    else if (v >= dhi)
    {
        result = hi;
        if (clamped && v > static_cast<double>(hi)) *clamped = true;
    }
    else
        result = static_cast<long long>(v); // toward zero
    if (result < lo) result = lo;
    if (result > hi) result = hi;
    out = result;
    return true;
}

bool leadingNumber(const String& text, double& out)
{
    const char* p = text.c_str();
    const char* const end = p + text.size();
    while (p != end && isBlank(*p)) ++p;
    return ct::parse_double(p, end, out) != p;
}

namespace
{

// The digits of v (finite, not zero) as %.17g gives them: digit[0..count) with no trailing zeros, and the exponent of the first one (value =
// d0.d1d2... x 10^exponent). False when the conversion cannot be done without snprintf.
bool digitsOf(double v, char* digit, int& count, int& exponent)
{
    char text[40];
    char* const end = ct::detail::write_general(v, 17, text);
    if (!end) return false;
    const char* p = text;
    if (*p == '-') ++p;
    int n = 0;
    int pointAt = -1; // digits before the point
    int exp10 = 0;
    for (; p != end; ++p)
    {
        if (*p == '.') pointAt = n;
        else if (*p == 'e')
        {
            ++p;
            bool negative = false;
            if (p != end && (*p == '-' || *p == '+')) negative = *p++ == '-';
            for (; p != end; ++p) exp10 = exp10 * 10 + (*p - '0');
            if (negative) exp10 = -exp10;
            break;
        }
        else
        {
            if (n >= 24) return false;
            digit[n++] = *p;
        }
    }
    if (pointAt < 0) pointAt = n;
    // leading zeros ("0.00123"): the first digit that is not one
    int first = 0;
    while (first < n && digit[first] == '0') ++first;
    if (first == n) return false;
    exponent = pointAt - 1 - first + exp10;
    for (int i = first; i < n; ++i) digit[i - first] = digit[i];
    count = n - first;
    while (count > 1 && digit[count - 1] == '0') --count;
    return true;
}

// "%.*f" for the values a label is made of; -1 when it is not one of them (or when the rounding is a tie that only the exact value decides)
int fixedFast(char* out, size_t cap, double v, int decimals)
{
    if (decimals < 0 || decimals > 9 || !(v - v == 0.0)) return -1;
    const double a = v < 0.0 ? -v : v;
    if (a >= 1.0e15) return -1;
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    const bool negative = (bits >> 63) != 0; // "-0.00" too: that is what printf writes
    char digit[32];
    int count = 0;
    int exponent = 0;
    char whole[40]; // the value in units of 10^-decimals, as decimal digits
    int wholeCount = 0;
    if (a == 0.0)
    {
        whole[wholeCount++] = '0';
    }
    else
    {
        if (!digitsOf(v, digit, count, exponent)) return -1;
        const int keep = exponent + decimals + 1; // digits that are before the cut
        if (keep > 17) return -1; // more digits than the 17 that were made: the rest is in the exact value, which only snprintf reads
        if (keep < 0) whole[wholeCount++] = '0';
        else
        {
            bool up = false;
            if (keep < count)
            {
                const char next = digit[keep];
                if (next > '5') up = true;
                else if (next == '5')
                {
                    if (keep + 1 < count) up = true;
                    else
                        return -1; // exactly half in 17 digits: the exact value decides, and snprintf knows it
                }
            }
            if (keep == 0)
            {
                whole[wholeCount++] = up ? '1' : '0';
            }
            else
            {
                if (keep > 30) return -1;
                for (int i = 0; i < keep; ++i) whole[wholeCount++] = i < count ? digit[i] : '0';
                if (up)
                {
                    int i = wholeCount - 1;
                    while (i >= 0 && whole[i] == '9') whole[i--] = '0';
                    if (i >= 0) ++whole[i];
                    else
                    {
                        for (int j = wholeCount; j > 0; --j) whole[j] = whole[j - 1];
                        whole[0] = '1';
                        ++wholeCount;
                    }
                }
            }
        }
    }
    // the point goes `decimals` digits from the right, with zeros on the left if the number is shorter than that
    char text[64];
    int len = 0;
    if (negative) text[len++] = '-';
    int intDigits = wholeCount - decimals;
    if (intDigits <= 0)
    {
        text[len++] = '0';
        if (decimals > 0)
        {
            text[len++] = '.';
            for (int i = 0; i < -intDigits; ++i) text[len++] = '0';
            for (int i = 0; i < wholeCount; ++i) text[len++] = whole[i];
        }
    }
    else
    {
        for (int i = 0; i < intDigits; ++i) text[len++] = whole[i];
        if (decimals > 0)
        {
            text[len++] = '.';
            for (int i = intDigits; i < wholeCount; ++i) text[len++] = whole[i];
        }
    }
    if (cap > 0)
    {
        const size_t room = cap - 1;
        const size_t n = static_cast<size_t>(len) < room ? static_cast<size_t>(len) : room;
        memcpy(out, text, n);
        out[n] = 0;
    }
    return len;
}

// What a printf that ran in a locale with another decimal separator wrote: '.' again, in the bytes that were written.
void pointToDot(char* out, size_t cap, int written)
{
    if (written <= 0 || cap == 0) return;
    const char point = ct::detail::decimal_point();
    if (point == '.') return;
    const size_t n = static_cast<size_t>(written) < cap - 1 ? static_cast<size_t>(written) : cap - 1;
    for (size_t i = 0; i < n; ++i)
        if (out[i] == point) out[i] = '.';
}

} // namespace

void pointToDot(char* text, size_t length)
{
    pointToDot(text, length + 1, static_cast<int>(length));
}

int fixedInto(char* out, size_t cap, double v, int decimals)
{
    const int fast = fixedFast(out, cap, v, decimals);
    if (fast >= 0) return fast;
    const int n = snprintf(out, cap, "%.*f", decimals, v);
    pointToDot(out, cap, n);
    return n;
}

int generalInto(char* out, size_t cap, double v, int precision)
{
    char text[40];
    if (char* const end = ct::detail::write_general(v, precision, text))
    {
        const int len = static_cast<int>(end - text);
        if (cap > 0)
        {
            const size_t room = cap - 1;
            const size_t n = static_cast<size_t>(len) < room ? static_cast<size_t>(len) : room;
            memcpy(out, text, n);
            out[n] = 0;
        }
        return len;
    }
    const int n = snprintf(out, cap, "%.*g", precision, v);
    pointToDot(out, cap, n);
    return n;
}

int cformat(char* out, size_t cap, const char* format, ...)
{
    va_list args;
    va_start(args, format);
#if defined(_WIN32)
    static _locale_t c = _create_locale(LC_NUMERIC, "C");
    const int n = _vsnprintf_l(out, cap, format, c, args);
    if (n < 0 && cap > 0) out[cap - 1] = 0;
#else
    // the locale of this thread for the length of the call (uselocale does not touch the other threads or the program's own setting)
    static const locale_t c = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
    const locale_t was = c ? uselocale(c) : static_cast<locale_t>(0);
    const int n = vsnprintf(out, cap, format, args);
    if (c) uselocale(was);
#endif
    va_end(args);
    return n;
}

String shortest(float v)
{
    char text[32];
    return String(text, static_cast<size_t>(ct::write_shortest(v, text) - text));
}

String shortest(double v)
{
    char text[32];
    return String(text, static_cast<size_t>(ct::write_shortest(v, text) - text));
}

String fixed(double v, int decimals)
{
    char text[64];
    const int n = fixedInto(text, sizeof text, v, decimals);
    if (n < static_cast<int>(sizeof text)) return String(text, static_cast<size_t>(n > 0 ? n : 0));
    ct::Vector<char> big(static_cast<size_t>(n) + 1);
    fixedInto(big.data(), big.size(), v, decimals);
    return String(big.data(), static_cast<size_t>(n));
}

} // namespace numtext
} // namespace retained
} // namespace ig

#pragma once

#include <stdlib.h>

#include <ct/string.hpp>

namespace BuGUI
{

// Retained-widget text type. Storage, SSO and hashing come from ct::String;
// the small compatibility surface below covers the editing operations used by
// the pre-existing widget API.
class String : public ct::String
{
public:
    using ct::String::String;
    using ct::String::append;
    using ct::String::operator=;
    using size_type = ct::String::size_type;

    String() = default;
    String(const ct::String &value) : ct::String(value) {}
    String(ct::String &&value) : ct::String(static_cast<ct::String &&>(value)) {}

    String &append(size_type count, char character)
    {
        for (size_type i = 0; i < count; ++i)
            push_back(character);
        return *this;
    }

    String substr(size_type position, size_type count = npos) const
    {
        return String(ct::String::substr(position, count));
    }

    String &insert(size_type position, const String &value)
    {
        return insert(position, value.data(), value.size());
    }

    String &insert(size_type position, const char *value)
    {
        return insert(position, value, String(value).size());
    }

    String &insert(size_type position, size_type count, char character)
    {
        String value(count, character);
        return insert(position, value);
    }

    String &erase(size_type position = 0u, size_type count = npos)
    {
        if (position > size())
            return *this;
        const size_type remaining = size() - position;
        const size_type removed = count < remaining ? count : remaining;
        if (removed == 0u)
            return *this;
        String tail = substr(position + removed);
        resize(position);
        append(tail);
        return *this;
    }

    iterator erase(iterator first, iterator last)
    {
        const size_type position = static_cast<size_type>(first - begin());
        const size_type count = static_cast<size_type>(last - first);
        erase(position, count);
        return begin() + position;
    }

    int compare(size_type position, size_type count, const String &value) const
    {
        return substr(position, count).ct::String::compare(value);
    }

    int compare(size_type position, size_type count, const char *value) const
    {
        return compare(position, count, String(value));
    }

    size_type find_first_not_of(const char *characters, size_type position = 0u) const
    {
        for (size_type i = position; i < size(); ++i)
        {
            bool found = false;
            for (const char *character = characters; *character; ++character)
            {
                if (data()[i] == *character)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return i;
        }
        return npos;
    }

    size_type find_last_of(const char *characters) const
    {
        for (size_type i = size(); i > 0u; --i)
        {
            for (const char *character = characters; *character; ++character)
                if (data()[i - 1u] == *character)
                    return i - 1u;
        }
        return npos;
    }

    int to_int() const
    {
        return static_cast<int>(strtol(c_str(), nullptr, 10));
    }

    float to_float() const
    {
        return strtof(c_str(), nullptr);
    }

private:
    String &insert(size_type position, const char *value, size_type length)
    {
        if (position > size())
            return *this;
        String copy(value, length);
        String tail = substr(position);
        resize(position);
        append(copy);
        append(tail);
        return *this;
    }
};

} // namespace BuGUI

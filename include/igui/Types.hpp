#pragma once

#include <stdint.h>
#include <stddef.h>

#include <ct/span.hpp>
#include <ct/string.hpp>

namespace ig
{

using String = ct::String;
using StringView = ct::StringView;

template <typename T>
using Span = ct::Span<T>;

using WidgetId = uint64_t;

struct FontId
{
    uint32_t value;

    FontId() : value(0) {}
    explicit FontId(uint32_t v) : value(v) {}

    bool operator==(const FontId &other) const { return value == other.value; }
    bool operator!=(const FontId &other) const { return value != other.value; }
};

struct TextureId
{
    uint64_t value;

    TextureId() : value(0) {}
    explicit TextureId(uint64_t v) : value(v) {}

    bool operator==(const TextureId &other) const { return value == other.value; }
    bool operator!=(const TextureId &other) const { return value != other.value; }
};

static const WidgetId InvalidWidgetId = 0;

} // namespace ig

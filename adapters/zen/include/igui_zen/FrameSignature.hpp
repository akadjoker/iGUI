#pragma once

#include <igui/widgets/Retained.hpp>
#include <stddef.h>
#include <stdint.h>

namespace ig
{
namespace zen
{

// A hash of everything that decides the pixels of a frame: the target size, the number of samples, the background, the display
// size, per pass the camera, the vertices and the indices, per command the texture handle, the texture's version, the clips and
// the index range. Two frames with the same signature draw the same picture, so a backend may keep the last one (SoftRaster
// copies its cache, the OpenGL path of App neither draws nor swaps).
// textureVersion(handle) -> uint32_t gives the backend's version of a texture, which changes on every upload into it; it is
// called for every command that has a texture.
template <class VersionFn>
inline uint64_t frameSignature(const ig::retained::DrawData& data, int width, int height, int samples,
        uint32_t background, VersionFn&& textureVersion)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const void* p, size_t n)
    {
        const unsigned char* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    };
    const int header[4] = { width, height, samples, static_cast<int>(background) };
    mix(header, sizeof header);
    mix(&data.displayWidth, sizeof data.displayWidth);
    mix(&data.displayHeight, sizeof data.displayHeight);
    for (const ig::retained::DrawPass& pass: data.passes)
    {
        if (!pass.list) continue;
        mix(&pass.camera, sizeof pass.camera);
        const auto& v = pass.list->vertices();
        const auto& ix = pass.list->indices();
        mix(v.data(), v.size() * sizeof v[0]);
        mix(ix.data(), ix.size() * sizeof ix[0]);
        for (const ig::retained::DrawCmd& cmd: pass.list->commands())
        {
            mix(&cmd.texture.value, sizeof cmd.texture.value);
            if (cmd.texture)
            {
                const uint32_t version = textureVersion(cmd.texture);
                mix(&version, sizeof version);
            }
            mix(&cmd.clip, sizeof cmd.clip);
            mix(&cmd.roundClipCount, sizeof cmd.roundClipCount);
            for (int i = 0; i < cmd.roundClipCount && i < ig::retained::DrawCmd::MaxRoundClips; ++i)
                mix(&cmd.roundClips[i], sizeof cmd.roundClips[i]);
            mix(&cmd.indexOffset, sizeof cmd.indexOffset);
            mix(&cmd.indexCount, sizeof cmd.indexCount);
        }
    }
    return h;
}

} // namespace zen
} // namespace ig

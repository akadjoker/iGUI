#include "SdlWidgetBridge.hpp"

#include <algorithm>
#include <cmath>

ig::retained::TextureHandle SdlWidgetBridge::createTexture(int width, int height,
                                                     const unsigned char *rgba)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) return {};
    if (SDL_UpdateTexture(texture, nullptr, rgba, width * 4) != 0 ||
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0)
    {
        SDL_DestroyTexture(texture);
        return {};
    }
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    return {reinterpret_cast<uintptr_t>(texture)};
}

void SdlWidgetBridge::destroyTexture(ig::retained::TextureHandle handle)
{
    if (handle)
        SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(handle.value));
}

bool SdlWidgetBridge::render(ig::retained::DrawData &data)
{
    data.stats.reset();
    for (const ig::retained::DrawPass &pass : data.passes)
    {
        if (!pass.list) continue;
        const auto &sourceVertices = pass.list->vertices();
        const auto &sourceIndices = pass.list->indices();
        vertices_.resize(sourceVertices.size());
        indices_.resize(sourceIndices.size());

        const float cosine = std::cos(pass.camera.angle) * pass.camera.scale;
        const float sine = std::sin(pass.camera.angle) * pass.camera.scale;
        const float pivotX = pass.camera.pivotX * data.displayWidth;
        const float pivotY = pass.camera.pivotY * data.displayHeight;
        const float offsetX = pass.camera.x - pivotX;
        const float offsetY = pass.camera.y - pivotY;
        const float translateX = cosine * offsetX - sine * offsetY + pivotX;
        const float translateY = sine * offsetX + cosine * offsetY + pivotY;

        for (size_t i = 0; i < sourceVertices.size(); ++i)
        {
            const ig::retained::DrawVertex &source = sourceVertices[i];
            SDL_Vertex &target = vertices_[i];
            target.position = {cosine * source.x - sine * source.y + translateX,
                               sine * source.x + cosine * source.y + translateY};
            target.tex_coord = {source.u, source.v};
            target.color = {source.color.r, source.color.g, source.color.b, source.color.a};
        }
        for (size_t i = 0; i < sourceIndices.size(); ++i)
            indices_[i] = static_cast<int>(sourceIndices[i]);

        for (const ig::retained::DrawCmd &command : pass.list->commands())
        {
            SDL_Rect clip;
            if (pass.camera.angle == 0.0f)
            {
                float x0 = cosine * command.clip.x + translateX;
                float y0 = cosine * command.clip.y + translateY;
                float x1 = cosine * (command.clip.x + command.clip.w) + translateX;
                float y1 = cosine * (command.clip.y + command.clip.h) + translateY;
                if (x0 > x1) std::swap(x0, x1);
                if (y0 > y1) std::swap(y0, y1);
                clip = {static_cast<int>(std::floor(x0)), static_cast<int>(std::floor(y0)),
                        static_cast<int>(std::ceil(x1 - x0)), static_cast<int>(std::ceil(y1 - y0))};
            }
            else
                clip = {0, 0, static_cast<int>(data.displayWidth), static_cast<int>(data.displayHeight)};
            SDL_RenderSetClipRect(renderer_, &clip);
            SDL_Texture *texture = command.texture
                ? reinterpret_cast<SDL_Texture *>(command.texture.value) : nullptr;
            if (command.indexOffset + command.indexCount > indices_.size() ||
                SDL_RenderGeometry(renderer_, texture, vertices_.data(),
                                   static_cast<int>(vertices_.size()),
                                   indices_.data() + command.indexOffset,
                                   static_cast<int>(command.indexCount)) != 0)
                return false;
            ++data.stats.drawCalls;
            data.stats.triangles += static_cast<int>(command.indexCount / 3u);
        }
        data.stats.vertices += static_cast<int>(sourceVertices.size());
        data.stats.clipChanges += pass.list->pushClipCount();
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    return true;
}

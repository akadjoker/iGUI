#pragma once

#include <stdint.h>

#include <igui/Backend.hpp>
#include <igui/DrawData.hpp>
#include <igui/FontAtlas.hpp>
#include <igui/Math.hpp>
#include <igui/Types.hpp>

namespace ig
{
namespace opengl
{

// Backend OpenGL 3.3 core para o DrawData do modo imediato.
//
// Não traz carregador de GL: a aplicação passa uma função que resolve nomes
// (SDL_GL_GetProcAddress, glfwGetProcAddress, ...) e o backend guarda os
// ponteiros que usa, por isso não há dependência de glad nem ligação a libGL.
// O contexto tem de estar current em todas as chamadas.
//
// Um comando de geometria é um draw call, com o clip do comando no scissor. O
// atlas de fontes é a textura de tudo: a geometria sólida amostra o texel branco
// dele (ver TextProvider::SolidStyle), pelo que uma janela inteira sai em poucos
// comandos em vez de um por troca de textura.
//
// Também desenha para alvos fora do ecrã (TargetCount deles, um por painel):
// beginTarget() liga um buffer do tamanho indicado, render() desenha lá,
// endTarget() copia-o para a janela em (x, y, width, height). É assim que duas
// UIs ficam lado a lado sem que cada uma tenha de saber onde está.
class Backend : public ig::Backend
{
public:
    Backend();
    ~Backend() override;

    // Resolve as funções e prepara shader, buffers e a textura branca de
    // recurso. O contexto tem de estar current.
    bool init(void *(*procAddress)(const char *name));
    void shutdown();
    bool ready() const { return program_ != 0u; }

    // Fonte do texto e textura da geometria sólida. O conteúdo do atlas é
    // enviado para a GPU uma vez; chamar outra vez volta a enviá-lo.
    void setFontAtlas(ig::FontAtlas *atlas);

    // Desenhar por cima do próprio framebuffer, sem beginTarget(): só precisa
    // disto para o tamanho da janela.
    void setWindowSize(int width, int height);
    // Limpa o alvo ligado nesse momento (a janela ou o target de beginTarget()).
    void clear(const ig::Color &color);

    // Dois alvos fora do ecrã, um por painel: beginTarget() liga o alvo, aponta
    // o viewport ao tamanho pedido e é nele que render() desenha; endTarget()
    // copia-o para a janela em (x, y, width, height). Assim duas UIs ficam lado a
    // lado sem nenhuma saber onde está, e sem a aplicação tocar em GL para lá do
    // contexto.
    //
    // A cópia é um quad texturizado, não um glBlitFramebuffer: o blit existe em
    // teoria em GL 3.0+, mas em drivers reais há casos em que não copia nada
    // (aconteceu com a Mesa 25.2 neste adaptador), e um quad funciona em qualquer
    // sítio onde o shader funcione. A textura tem de ter o mesmo tamanho da
    // região copiada.
    static const int TargetCount = 2;
    bool beginTarget(int target, int width, int height);
    void endTarget(int target, int x, int y, int width, int height);

    ig::TextMetrics measureText(ig::FontId font, ig::StringView text,
                                float logicalSize, float dpiScale) override;
    bool render(const ig::DrawData &data) override;

    // Texturas de imagem, para as widgets que recebem um TextureId.
    ig::TextureId createTexture(int width, int height, const unsigned char *rgba);
    bool updateTexture(ig::TextureId texture, int width, int height, const unsigned char *rgba);
    void destroyTexture(ig::TextureId texture);

    // Lê de volta como RGBA, de cima para baixo, do alvo ligado nesse momento.
    // Para guardar uma imagem do resultado. A memória é do chamador.
    bool readPixels(int x, int y, int width, int height, unsigned char *rgba);

    const char *renderer() const;
    int versionMajor() const { return major_; }
    int versionMinor() const { return minor_; }

    // Tempo de GPU dos draw calls, com queries GL_TIME_ELAPSED: gpuTimerBegin()
    // e gpuTimerEnd() à volta do render(), e gpuSeconds() devolve a última medição
    // pronta - 0 enquanto não houver nenhuma. Cada slot tem o seu par de queries
    // em ping-pong (um slot por painel), por isso ler nunca espera pela GPU e
    // duas medições no mesmo frame não se sobrepõem.
    bool gpuTimerSupported() const { return query_[0][0] != 0u; }
    void gpuTimerBegin(int slot);
    void gpuTimerEnd(int slot);
    double gpuSeconds(int slot);

    // Últimos totais de um render(), para o relatório.
    int drawCalls() const { return drawCalls_; }
    int uploadedVertices() const { return uploadedVertices_; }

private:
    Backend(const Backend &);
    Backend &operator=(const Backend &);

    bool uploadAtlas();
    bool ensureTarget(int target, int width, int height);
    void bindTarget(int target);
    void copyTargetToWindow(int target, int x, int y, int width, int height);
    // Tamanho do alvo ligado nesse momento: o target de beginTarget() ou a
    // janela.
    int currentWidth() const
    {
        return currentTarget_ >= 0 ? targetWidth_[currentTarget_] : windowWidth_;
    }
    int currentHeight() const
    {
        return currentTarget_ >= 0 ? targetHeight_[currentTarget_] : windowHeight_;
    }
    void drawCommand(uint32_t firstIndex, uint32_t indexCount, const ig::Rect &clip,
                     ig::TextureId texture, float scaleX, float scaleY);

    struct Impl;
    Impl *impl_;
    uint32_t program_;
    uint32_t vao_;
    uint32_t vbo_;
    uint32_t ibo_;
    uint32_t textVao_;
    uint32_t textVbo_;
    uint32_t textIbo_;
    uint32_t atlasTexture_;
    uint32_t whiteTexture_;
    uint32_t query_[TargetCount][2];
    double lastGpuSeconds_[TargetCount];
    int querySlot_[TargetCount];
    uint32_t quadVao_;
    uint32_t quadVbo_;
    uint32_t quadIbo_;
    uint32_t targetFramebuffer_[TargetCount];
    uint32_t targetTexture_[TargetCount];
    int targetWidth_[TargetCount];
    int targetHeight_[TargetCount];
    int currentTarget_;
    int major_;
    int minor_;
    int drawCalls_;
    int uploadedVertices_;
    int windowWidth_;
    int windowHeight_;
    ig::FontAtlas *atlas_;
    bool atlasUploaded_;
    // O atlas em RGBA, composto a partir da máscara de 8 bits (ver uploadAtlas).
    ct::Vector<uint8_t> rgba_;
    ig::DrawList textScratch_;
};

} // namespace opengl
} // namespace ig

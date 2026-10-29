// Guarda o framebuffer numa imagem. A implementação de stb_image_write fica aqui
// (e não no stb_impl.cpp, que pertence ao módulo retido) para que o exemplo não
// dependa desse módulo.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <stdio.h>

namespace compare
{

bool writePng(const char* path, int width, int height, const unsigned char* rgba)
{
    if (!path || width <= 0 || height <= 0 || !rgba) return false;
    // O backend lê os pixels de cima para baixo, que é a ordem que o
    // stb_image_write espera.
    if (stbi_write_png(path, width, height, 4, rgba, width * 4) == 0)
    {
        fprintf(stderr, "nao consegui escrever %s\n", path);
        return false;
    }
    printf("imagem guardada: %s (%dx%d)\n", path, width, height);
    return true;
}

} // namespace compare

# Callback de desenho nativo (`DrawList::addNativeCallback`, `NativeView`)

Pedido da equipa do zenMCAP e do lucent-atlas (`docs/specs/draw-callback.md`): os graficos 2D e as sobreposicoes de mapa passam a desenhar com chamadas GL
suas, **na ordem do desenho** e dentro do rectangulo de um widget, sem passar por uma FBO e por `Viewport3D::setTexture`. A FBO continua a ser o caminho da cena 3D.

## 1. O que e

Um comando da lista de desenho que leva uma funcao e um ponteiro do utilizador. O renderizador OpenGL do adaptador `zen` chama a funcao no sitio do comando
(depois do que se desenhou antes, antes do que se desenha depois), com o recorte do widget ja posto como scissor. Os outros renderizadores (o rasteriador por
software, os testes sem renderizador, o SDL dos exemplos) **saltam** o comando: nao desenham nada e nao falham.

```cpp
using NativeDrawFn = void (*)(const NativeDrawInfo& info);
void DrawList::addNativeCallback(NativeDrawFn fn, void* user, const Rect& rect, bool restoreState = true);
bool NativeDrawAvailable();      // ig::retained::, ou GetIO().nativeBackend != 0
```

`rect` e o rectangulo do widget, nas unidades da lista (sob a transformacao em vigor). Nao se junta com os comandos vizinhos (a fusao de comandos da ronda 9 nunca atravessa um
callback) e nao deixa comando nenhum quando o recorte esta vazio ou o rectangulo cai fora dele. Para o resto da biblioteca e um comando como outro qualquer: `DrawCmd::isSpecial()`
e verdadeiro (`roundClipCount == -1`, `indexCount == 0`), e `DrawList::special(cmd)` da a funcao, o ponteiro e o rectangulo.

### 1.1 A pergunta antes de pintar

`NativeDrawAvailable()` (igual a `GetIO().nativeBackend != 0`) diz se o renderizador desta janela corre callbacks. E **verdadeira logo a seguir a `App::create`** do adaptador (antes de
qualquer pintura) com OpenGL, e falsa com o rasteriador por software e num teste sem renderizador (um `IO` novo tem `nativeBackend == 0`). Um widget escolhe com ela, antes de
pintar, entre o callback e uma textura ou um desenho do `PaintContext`. `GetIO().nativeSamples` e a contagem de amostras (MSAA) do alvo.

## 2. O que o callback recebe (`NativeDrawInfo`)

| campo | o que e |
|---|---|
| `user` | o ponteiro dado a `addNativeCallback` |
| `backend` | `NativeBackend::OpenGL33Core` (1) ou `OpenGLES3` (2; e a etiqueta do WebGL 2 tambem) |
| `framebufferWidth`, `framebufferHeight` | o tamanho do alvo em pixeis |
| `samples` | amostras do alvo (0 ou 1: sem MSAA); a do alvo fora do ecra e a que o driver deu |
| `pixelScaleX`, `pixelScaleY` | pixeis do alvo por unidade logica dos widgets |
| `rect` | o rectangulo do widget em pixeis do alvo, **origem no canto superior esquerdo, y para baixo** (nao cortado pelo recorte) |
| `clip` | o recorte em vigor, mesma convencao (e o scissor) |
| `rectGl[4]` | `rect` como o `glViewport` o quer: x, y, w, h inteiros (arredondado para fora), **origem no canto inferior esquerdo** |
| `scissorGl[4]` | a caixa que o `glScissor` recebeu, origem no canto inferior esquerdo |
| `projection[16]` | matriz 4x4 por colunas: unidades logicas (as coordenadas dos widgets) para o espaco de recorte, **com a camera da passagem**; e a que o vertex shader do iGUI aplica |
| `pixelProjection[16]` | matriz 4x4 por colunas: pixeis do alvo (origem em cima a esquerda) para o espaco de recorte |
| `restoreState` | o que se pediu |

## 3. O estado GL

**Na entrada** (sempre o mesmo, para a funcao nao ter de saber o que o iGUI usava):

- alvo ligado (a FBO fora do ecra, ou o buffer da janela) e `glViewport(0, 0, framebufferWidth, framebufferHeight)`;
- `GL_SCISSOR_TEST` ligado com a caixa `scissorGl`;
- `GL_BLEND` ligado, `glBlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA)`, equacao `FUNC_ADD`;
- `GL_DEPTH_TEST`, `GL_CULL_FACE`, `GL_STENCIL_TEST` desligados, todos os canais escritos;
- **nada do iGUI ligado**: programa 0, um vertex array vazio **feito de novo para cada chamada** (o perfil core exige um; o que a funcao lhe fizer nao chega ao do iGUI), sem
  `GL_ARRAY_BUFFER`, unidade de textura 0 activa sem textura 2D.

**Com `restoreState` ligado** (por omissao) o iGUI poe de volta, depois da chamada, tudo o que usa, qualquer que seja o estado em que a funcao a deixou:

1. o alvo (`glBindFramebuffer(GL_FRAMEBUFFER, ...)`) e `glViewport(0, 0, largura, altura)`;
2. `GL_SCISSOR_TEST` ligado e `glScissor` com a caixa do comando;
3. `GL_BLEND` ligado, `glBlendEquation(GL_FUNC_ADD)`, `glBlendFuncSeparate(...)` como acima;
4. `GL_DEPTH_TEST`, `GL_CULL_FACE`, `GL_STENCIL_TEST`, `GL_SAMPLE_ALPHA_TO_COVERAGE`, `GL_POLYGON_OFFSET_FILL` desligados; `glColorMask(1, 1, 1, 1)`;
5. `glActiveTexture(GL_TEXTURE0)`;
6. o programa, o vertex array e o `GL_ARRAY_BUFFER` do iGUI (o buffer de indices e estado do vertex array); a textura de cada comando seguinte e ligada pelo proprio comando.

**Com `restoreState` desligado** o iGUI so volta a ligar o que a entrada limpou (a ultima linha: programa, vertex array, buffer de vertices; as texturas ja as liga cada comando). A funcao
tem de deixar **como os encontrou**: o alvo, o viewport, o scissor (ligado e a caixa), o blend (ligado, funcao e equacao), a profundidade, o stencil e o cull desligados, a mascara de cor,
a amostragem alpha-to-coverage, o desvio de poligono e a unidade de textura activa. Poupa as ~15 chamadas GL da lista (uns 10 microssegundos), nada mais.

**A funcao nunca deve** apagar nem modificar objectos GL do iGUI (nao os conhece: so por nomes que adivinhasse), nem chamar `glFinish`, nem mudar o contexto corrente, nem criar threads ou
partilhar o contexto. Corre na thread da frame com o contexto corrente.

## 4. A regra do parado

- A funcao so e chamada em frames que **se pintam**. Numa janela parada (`WidgetApp::needsFrame() == false`, o adaptador salta a pintura) nao e chamada: 0 chamadas em 100 frames paradas.
- Chama-la nao marca nada sujo, e nao pede frame. Um widget cujo conteudo nativo muda pede ele a frame: `WidgetApp::requestFrame()` (do widget, `NativeView::requestRepaint()`;
  de outra thread, `requestFrameFromAnyThread()`). O callback corre entao exactamente uma vez nessa frame.
- Quem tem o ciclo do adaptador com `setFrameCallback` ou `setAfterRenderCallback` continua a pintar todas as frames ate ligar `setIdleSkipWithCallbacks(true)` (ROUND9.md 2.3).

## 5. `NativeView`

O widget que traz o callback e o desenho de reserva. Pergunta ao renderizador **quando pinta**, por isso a aplicacao nao escolhe nada de antemao:

```cpp
auto* view = parent->createChild<NativeView>();
view->setNativeDraw(drawTriangle, &state);        // restoreState = true
view->setFallback([&](PaintContext& ctx, const Rect& b) { /* o mesmo com ctx.fillTriangle(...), ou uma textura */ });
view->isNative();                                 // callback e renderizador que o corre
view->requestRepaint();                           // o conteudo nativo mudou: pinta a proxima frame
```

Sem desenho de reserva, onde o callback nao corre o widget nao desenha nada. O recorte (um `ScrollView`, um painel) ja esta aplicado ao scissor.

## 6. Exemplo completo (o do teste `tests/test_zen_native_callback_gl.cpp`, resumido)

```cpp
#include <igui_zen/ZenApp.hpp>
#include <glad/gl.h>                      // as funcoes que o GlRaster carregou; no web: <GLES3/gl3.h>
#include <igui/widgets/ViewWidgets.hpp>

using namespace ig::retained;

struct Triangle { unsigned program = 0, vao = 0, vbo = 0; int colour = -1; };

static unsigned compile(unsigned type, const char* body, int backend)
{
    // o mesmo GLSL serve os dois perfis: so a primeira linha muda
    const char* src[2] = { backend == (int)NativeBackend::OpenGLES3 ? "#version 300 es\n" : "#version 330 core\n", body };
    unsigned s = glCreateShader(type);
    glShaderSource(s, 2, src, nullptr);
    glCompileShader(s);
    return s;
}

static void drawTriangle(const NativeDrawInfo& info)
{
    Triangle& t = *static_cast<Triangle*>(info.user);
    if (!t.program)                                          // a primeira vez: os objectos GL, com o contexto corrente
    {
        unsigned vs = compile(GL_VERTEX_SHADER, "layout(location=0) in vec2 p; void main(){ gl_Position = vec4(p, 0.0, 1.0); }\n", info.backend);
        unsigned fs = compile(GL_FRAGMENT_SHADER, "precision mediump float; uniform vec4 c; out vec4 o; void main(){ o = c; }\n", info.backend);
        t.program = glCreateProgram();
        glAttachShader(t.program, vs); glAttachShader(t.program, fs); glLinkProgram(t.program);
        glDeleteShader(vs); glDeleteShader(fs);
        t.colour = glGetUniformLocation(t.program, "c");
        glGenVertexArrays(1, &t.vao); glBindVertexArray(t.vao);
        glGenBuffers(1, &t.vbo);      glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    // o desenho cai no rectangulo do widget: o viewport e o rectangulo; o scissor (o recorte) ja la esta
    glViewport(info.rectGl[0], info.rectGl[1], info.rectGl[2], info.rectGl[3]);
    const float v[6] = { -1, -1, 1, -1, -1, 1 };            // a metade inferior esquerda
    glUseProgram(t.program);
    glUniform4f(t.colour, 1.0f, 0.0f, 0.0f, 1.0f);
    glBindVertexArray(t.vao);
    glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // restoreState e verdadeiro: o iGUI poe o seu estado de volta (viewport, programa, vertex array...)
}

// na aplicacao
Triangle state;
auto* view = root->createChild<NativeView>();
view->setRect({ 40, 30, 80, 60 });
view->setNativeDraw(drawTriangle, &state);
view->setFallback([](PaintContext& ctx, const Rect& b)       // software, testes
        { ctx.fill.SetColor(Color(255, 0, 0, 255)); ctx.fillTriangle(b.x, b.y + b.h, b.x + b.w, b.y + b.h, b.x, b.y); });
// quando o triangulo muda: view->requestRepaint();
```

Os objectos GL da funcao (`state`) pertencem a ela: criam-se na primeira chamada e libertam-se quando a aplicacao acaba, com o contexto corrente.

### 6.1 Web (WebGL 2) e GLES 3

O renderizador do adaptador compila para GLES 3.0 no web (`NativeBackend::OpenGLES3`; e a etiqueta do WebGL 2), por isso as chamadas da funcao tem de ser **as do ES 3.0** (a mesma funcao corre depois no
desktop, onde um contexto 3.3 core as aceita todas). Nao usar o que so o desktop tem: `glPolygonMode` (e os modos de linha e ponto de preenchimento), `glDrawBuffer`, `glMapBuffer` (so ha `glMapBufferRange`),
`glLogicOp`, texturas 1D, `GL_CLAMP_TO_BORDER` e `GL_TEXTURE_BORDER_COLOR`, `GL_PRIMITIVE_RESTART` (so o `GL_PRIMITIVE_RESTART_FIXED_INDEX`), `GL_POINT_SIZE` por chamada (o tamanho do ponto sai do shader com
`gl_PointSize`), shaders de geometria e de tesselacao, `GL_QUADS` e `GL_LINE_LOOP` de funcoes antigas, e formatos de textura que o ES nao tem (`GL_LUMINANCE`: usar `GL_R8` com `texture swizzle`). O GLSL leva
`#version 300 es` e uma linha `precision` no fragment shader (o exemplo escolhe a primeira linha pelo `info.backend`). A contagem de amostras vem em `info.samples`: no web depende do contexto que o navegador deu.

## 7. Limites

- Uma camera da passagem que **roda** (`Camera2D::angle != 0`) nao corre callbacks (a escala e a translacao sim; `projection` leva-as). Uma transformacao da lista com rotacao da ao callback a caixa que contem o rectangulo.
- Os recortes redondos nao se aplicam ao callback (so o scissor, a caixa do recorte).
- Mais de uma janela: os callbacks correm no contexto unico do adaptador, com o alvo da janela que se desenha.
- O comando custa uma entrada em `DrawList::specials()` e outra em `commands()`; uma janela que nao usa callbacks nao paga nada: o mesmo numero de comandos, de chamadas GL e de instrucoes do lado do GUI (medido, ver ROUND10.md).

## 8. Testes

- `tests/test_native_callback.cpp` (`igui_native_callback`, sem GL): disponibilidade, o desenho de reserva, o lugar na lista, a fusao que nao atravessa o callback, recortes vazios, transformacao, o rasteriador que salta,
  o parado.
- `tests/test_zen_native_callback_gl.cpp` (`igui_zen_native_callback_gl`, no Xvfb privado com llvmpipe): o parado numa janela real (0 chamadas em 100 frames, 1 por `requestRepaint`), o que a funcao recebe, um triangulo
  conhecido dentro do rectangulo e nada fora, o recorte de um `ScrollView`, a ordem entre dois widgets que se sobrepoem, o estado GL (uma funcao que estraga programa, vertex array, buffers, texturas, blend, scissor, viewport,
  profundidade, cull, stencil, mascara e FBO com `restoreState` ligado, e uma que repoe o que tocou com ele desligado: os widgets desenhados depois saem pixel a pixel como sem callback; **controlo negativo**: sem o restauro
  do iGUI o teste falha com 8819 pixeis diferentes), e as amostras.

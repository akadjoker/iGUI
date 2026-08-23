# Plano de normalização do iGUI

## 1. Objetivo

Transformar o protótipo atual numa biblioteca de GUI immediate-mode pequena, autónoma e independente do motor gráfico e da biblioteca de janelas.

O núcleo do iGUI deve:

- receber eventos traduzidos pela aplicação;
- manter apenas o estado necessário de janelas e controlos;
- produzir uma lista de comandos de desenho neutra;
- nunca incluir nem chamar diretamente `RenderBatch`, `Font`, `Input`, `Device`, OpenGL, SDL, GLFW ou outro motor;
- compilar e ser testável sem criar uma janela ou uma GPU;
- não conter qualquer uso de `std::` no código próprio do core (`include/igui` e `src`);
- usar as estruturas e algoritmos de [`ct`](https://github.com/akadjoker/containers) em vez dos equivalentes da STL.

A aplicação será responsável por recolher eventos, enviá-los ao iGUI e mandar o backend executar o resultado do frame:

```text
SO/SDL/GLFW/etc. -> ig::Event -> ig::Context -> ig::DrawData -> ig::Backend -> GPU/Canvas/etc.
                                      |
                                      +-> resultados dos widgets para a aplicação
```

## 2. Estado atual

O projeto contém apenas:

- `GUI.hpp` — API, tema e todo o estado interno;
- `GUI.cpp` — interação, estado e desenho de todos os widgets;
- `tmp/wiGUI.h` e `tmp/wiGUI.cpp` — código de referência dependente do Wicked Engine, não compilável isoladamente.

Não existe ainda `CMakeLists.txt`, teste, exemplo autónomo, namespace próprio nem definição local das dependências usadas por `GUI.cpp`.

### Dependências que faltam

O código principal depende de tipos ou serviços não presentes neste repositório:

- `Config.hpp`: `u8`, `u32`;
- `Math.hpp`: `Vec2`, `Vec3`, `FloatRect`, `Clamp`;
- `Color.hpp`: `Color`;
- `Batch.hpp`: `RenderBatch` e primitivas de desenho;
- uma classe `Font` para medir e desenhar texto;
- `Input.hpp`: rato, teclado e entrada de caracteres;
- `Device::Instance().GetFrameTime()`;
- `pch.h`, que esconde includes realmente necessários.

`Shader.hpp` e `Texture.hpp` são incluídos por `GUI.cpp`, mas não são usados diretamente e devem desaparecer do núcleo.

### Problemas estruturais a resolver durante a migração

- Widgets chamam renderização e input globais diretamente.
- Há dois sistemas de identidade/estado sobrepostos: IDs de texto (`m_activeID`) e IDs numéricos sequenciais (`Control::id`).
- IDs sequenciais mudam quando um widget condicional aparece ou desaparece, podendo transferir estado para o controlo errado.
- O estado `static` dentro de `ColorPicker`, `ListBox` e `Dropdown` é partilhado entre instâncias de `GUI`, não é libertado e pode colidir.
- A ordem Z é usada no hit-test, mas o desenho continua a acontecer na ordem das chamadas da aplicação.
- Não existe clipping/scissor; conteúdo e dropdowns podem desenhar fora da janela.
- `DrawRectOutline()` ignora o argumento `thickness`.
- Coordenadas `float` são convertidas para `int` antes de desenhar.
- `canResize`, `isResizing`, scroll de janela e parte do estado hot/active existem, mas estão incompletos ou não são usados.
- `TextInput` só trata um carácter ASCII por frame e depende de um relógio global.
- `IsFocused()` significa atualmente “rato sobre uma janela”, não foco real.
- Os quatro `static` de drag/open e os mapas auxiliares nunca fazem garbage collection.
- Parâmetros e ponteiros de utilizador não são validados de forma consistente.
- `tmp/wiGUI.*` traz muitas dependências de outro motor. Deve continuar fora do build e ser usado apenas como referência, com origem/licença documentada antes de aproveitar código.

## 3. Decisões de arquitetura

### 3.1 Preservar immediate mode

A forma pública atual (`BeginWindow()`, `Button()`, `SliderFloat()`, etc.) será preservada como conceito. Não é necessário criar uma árvore persistente de classes para cada widget.

Durante a transição pode existir uma fachada de compatibilidade `GUI`, mas a implementação nova ficará em `namespace ig`, na classe `Context`.

### 3.2 Nome e namespace

O nome do projeto continua a ser **iGUI**. O namespace público será **`ig`**.

- `ig` tem apenas dois caracteres e deriva diretamente de iGUI;
- evita repetir nomes como `igui::GuiContext`;
- é menos provável colidir com código da aplicação do que o namespace genérico `ui`;
- produz nomes curtos e legíveis: `ig::Context`, `ig::Backend`, `ig::Event`, `ig::Theme`.

O target CMake e a pasta de includes podem continuar descritivos (`igui::igui` e `include/igui`). Namespace C++, nome do target e caminho de include não precisam ser iguais.

### 3.3 Eventos enviados pela aplicação

O iGUI deixa de fazer polling a `Input`. A aplicação converte os eventos da sua plataforma e chama `pushEvent()` antes de `beginFrame()`.

Eventos mínimos:

- `PointerMove`;
- `PointerDown` e `PointerUp`, com botão;
- `PointerWheel` horizontal/vertical;
- `KeyDown` e `KeyUp`, com modificadores;
- `TextInput`, em UTF-8;
- `FocusLost`;
- `ViewportChanged`/alteração de DPI.

`FocusLost` deve cancelar capturas e drags para não deixar controlos presos. `TextInput` é separado de `KeyDown`, pois texto e teclas físicas são conceitos diferentes.

`FrameInfo` transporta `deltaSeconds`, tamanho do viewport e `dpiScale`; assim desaparece a dependência de `Device`.

### 3.4 Lista de desenho neutra

Os widgets não chamam métodos virtuais por primitiva. Eles acrescentam comandos a um `DrawList`. No fim do frame, a aplicação recebe `DrawData` e entrega-o ao backend.

O core oferece helpers para retângulos, contornos, linhas, polylines, círculos e retângulos arredondados, mas converte todas essas formas para geometria antes de entregar o frame. O backend só precisa compreender dois comandos finais:

- `GeometryCommand`: intervalo de vértices/índices coloridos e clip rect já resolvido;
- `TextCommand`: texto UTF-8, `FontId`, tamanho, posição, cor e clip rect já resolvido.

`DrawData` contém buffers partilhados de vértices, índices e bytes de texto, mais uma sequência ordenada de `DrawCommand`. `DrawCommand` será uma união tagged trivial; não precisa de alocação, RTTI ou chamada virtual por primitiva.

Formato inicial proposto:

```cpp
namespace ig {

struct TextureId { uint64_t value; }; // zero = geometria sem textura

struct DrawVertex {
    Vec2 position;
    Vec2 uv;
    Color color;
};

using DrawIndex = uint32_t;

struct GeometryCommand {
    Rect clip;
    TextureId texture;
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
};

struct TextCommand {
    Rect clip;
    Vec2 position;
    FontId font;
    float logicalSize;
    Color color;
    uint32_t textOffset;
    uint32_t textSize;
};

enum class DrawCommandType : uint8_t { Geometry, Text };

struct DrawData {
    Vec2 displaySize;
    float dpiScale;
    ct::Span<const DrawVertex> vertices;
    ct::Span<const DrawIndex> indices;
    ct::Span<const char> textBytes;
    ct::Span<const DrawCommand> commands;
};

} // namespace ig
```

Os payloads concretos de `DrawCommand` ficam numa união tagged composta apenas por campos triviais. `TextCommand` usa offset+tamanho para os bytes pertencentes a `DrawData`, evitando ponteiros internos que possam ficar inválidos quando o buffer cresce.

`TextureId` fica previsto desde o início para permitir imagens no futuro sem alterar o formato da geometria. O handle é opaco; a aplicação/backend cria, resolve e destrói a textura. O core apenas o transporta. UVs são normalizados; quando o handle vale zero, o backend ignora os UVs.

Esta escolha mantém a tessellation e a aparência dos widgets iguais em todos os backends. Também permite desenhar o gradiente do `ColorPicker` com cor por vértice sem acrescentar uma operação especial ao backend.

As listas devem ser agrupadas por janela e compostas por `zOrder` em `endFrame()`. Isto resolve a diferença atual entre a janela que recebe input e a janela que aparece por cima.

Texto, pontos e vértices referidos pelos comandos pertencem a `DrawData` e permanecem válidos até ao `beginFrame()` seguinte.

### 3.5 Backend virtual mínimo

O contrato proposto é semelhante a:

```cpp
namespace ig {

struct FontId {
    uint32_t value;
};

struct TextMetrics {
    float width;
    float height;
    float ascent;
    float descent;
};

class Backend {
public:
    virtual ~Backend() {}

    virtual TextMetrics measureText(FontId font,
                                    ct::StringView utf8,
                                    float logicalSize,
                                    float dpiScale) = 0;

    virtual bool render(const DrawData& data) = 0;
};

} // namespace ig
```

`Context` guarda uma referência não proprietária ao `Backend`; por isso o backend deve viver mais tempo do que o contexto. Fonts, texturas, device, render target, command buffer e outros recursos continuam a pertencer ao backend concreto/aplicação.

O núcleo consulta `measureText()` durante a construção dos widgets, para que hitboxes, alinhamento e desenho usem exatamente as mesmas métricas. Não inicia nem termina frames gráficos e não chama `render()`. Essa chamada pertence ao programa:

```cpp
ig::Context ui(backend);

while (platform.pollEvent(nativeEvent)) {
    ig::Event event;
    if (adapter.translate(nativeEvent, event)) {
        ui.pushEvent(event);
    }
}

ui.beginFrame(frameInfo);

if (ui.beginWindow("Ferramentas", {20, 20, 320, 400}, &open)) {
    if (ui.button("Guardar", {8, 8, 100, 24})) {
        save();
    }
    ui.endWindow();
}

const ig::DrawData& drawData = ui.endFrame();
if (!backend.render(drawData)) {
    reportRenderFailure();
}
```

#### Convenções obrigatórias do backend

- coordenadas em pixels lógicos, origem no canto superior esquerdo, X para a direita e Y para baixo;
- `dpiScale` converte pixels lógicos em pixels do framebuffer;
- cores RGBA8 não pré-multiplicadas; blend normal source-alpha;
- clip rect semiaberto (`min` incluído, `max` excluído), já intersectado pelo core;
- geometria composta por triângulos indexados e executada pela ordem dos comandos;
- texto UTF-8; `FontId` é um handle opaco criado/gerido pela aplicação ou backend;
- `FontId{0}` é inválido e deve ser detetado em Debug;
- `logicalSize` e as métricas devolvidas usam coordenadas lógicas;
- o backend não guarda referências a `DrawData` depois de `render()`;
- `DrawData` é imutável e válido apenas até ao `beginFrame()` seguinte;
- a primeira versão é single-thread: `measureText()`, construção do frame e `render()` acontecem na mesma thread.

Se um renderer precisar de um objeto variável por frame, como command list, encoder ou framebuffer, isso é configurado na classe concreta antes de `render()`. Esse tipo específico não atravessa `ig::Backend` nem os headers do core.

#### O que não pertence ao backend

- polling ou tradução de eventos;
- relógio/delta time;
- criação da janela do sistema;
- estado de widgets, foco ou hit-test;
- tema e layout;
- posse do `Context`;
- headers ou tipos concretos de OpenGL, Vulkan, Direct3D, SDL, GLFW ou do motor do utilizador.

O adaptador de plataforma pode viver junto de um backend concreto, mas continua separado da interface `ig::Backend`. Se no futuro medir texto e renderizar precisarem de objetos diferentes, o contrato pode ser dividido sem alterar os widgets.

### 3.6 IDs e estado

Criar um único tipo `WidgetId` de 64 bits, calculado a partir de:

- ID da janela;
- stack de IDs (`pushId()`/`popId()`);
- identificador do widget.

Labels devem aceitar a convenção `Texto visível##id_estável`. O estado deixa de depender da ordem das chamadas.

Cada `ControlState` inclui `lastSeenFrame`. Um garbage collector incremental remove estado de widgets não vistos depois de um limite configurável. Estado específico (`TextEditState`, `ListBoxState`, `ColorPickerState`, etc.) fica dentro do contexto, nunca em variáveis `static` da função.

### 3.7 Clipping, captura e camadas

- Cada janela abre um clip rect para a área de conteúdo.
- Title bar e moldura usam o clip do viewport.
- Dropdowns e tooltips são enviados para uma camada overlay, depois das janelas.
- Só o widget capturado recebe drag até ao `PointerUp` ou `FocusLost`.
- O hit-test percorre janelas do topo para baixo e respeita clipping.
- O scroll só é consumido pelo controlo/janela elegível mais acima; caso contrário pode propagar ao pai.

### 3.8 Aplicação de prova SDL2 — fora da biblioteca

SDL2 será apenas a aplicação consumidora usada para provar o primeiro corte vertical. **Não faz parte da biblioteca iGUI, da sua API pública nem do target `igui`.** Assim que `Context`, eventos, `DrawData`, janela, `Label`, `Button` e `Checkbox` existirem, um exemplo externo consegue abrir uma janela SDL2 e interagir com GUI real.

Componentes locais ao exemplo:

- `demo::SdlEventAdapter`: traduz `SDL_Event` para `ig::Event`;
- `demo::SdlBackend`: implementa `ig::Backend` sobre um `SDL_Renderer*` recebido;
- `igui_sdl2_demo`: cria `SDL_Window`, `SDL_Renderer`, fonte e executa o loop completo.

Mapeamento do backend:

| iGUI | SDL2 |
|---|---|
| `GeometryCommand` | `SDL_RenderGeometryRaw()` |
| clip rect resolvido | `SDL_RenderSetClipRect()` |
| escala DPI | razão entre `SDL_GetRendererOutputSize()` e `SDL_GetWindowSize()`, aplicada ao renderer |
| `measureText()` | `TTF_SizeUTF8()` |
| `TextCommand` | textura criada por SDL_ttf e desenhada por `SDL_RenderCopyF()` |
| `TextureId` | registry interno de `SDL_Texture*` |
| pointer/teclado/wheel | `SDL_MOUSE*`, `SDL_KEY*` e `SDL_MOUSEWHEEL` |
| texto UTF-8 | `SDL_TEXTINPUT` |
| cancelamento de captura | `SDL_WINDOWEVENT_FOCUS_LOST` |

Regras do adaptador SDL2:

- exigir SDL **2.0.18 ou superior**, porque `SDL_RenderGeometryRaw()` foi introduzido nessa versão;
- SDL_ttf é uma dependência opcional e separada do SDL2;
- copiar o slice de `TextCommand` para uma `ct::String` terminada em zero antes de chamar SDL_ttf;
- começar com cache de texturas por `FontId + tamanho + texto + cor`; nunca rasterizar o mesmo label em todos os frames;
- invalidar a cache quando uma fonte for destruída ou a escala DPI mudar;
- usar `SDL_StartTextInput()`/`SDL_StopTextInput()` de acordo com `Context::wantsTextInput()`;
- não chamar `SDL_RenderClear()` nem `SDL_RenderPresent()` dentro do backend; o frame continua controlado pela aplicação;
- guardar e restaurar clip/scale/viewport do `SDL_Renderer` para não estragar o desenho da aplicação;
- devolver `false` de `render()` se `SDL_RenderGeometryRaw()` não for suportado pelo renderer selecionado;
- manter todos os includes e tipos SDL dentro de `examples/sdl2_demo`.

O exemplo SDL2 também deve usar `ct` em vez de contentores STL, embora o teste obrigatório `igui_no_std` continue focado no core.

Isolamento obrigatório no CMake:

```cmake
option(IGUI_BUILD_SDL2_DEMO "Build SDL2 integration example" OFF)

if(IGUI_BUILD_SDL2_DEMO)
    add_subdirectory(examples/sdl2_demo)
endif()
```

- `igui` liga apenas a `ct`;
- `find_package(SDL2)` e `find_package(SDL2_ttf)` só existem no CMake do exemplo;
- instalar/exportar `igui::igui` não instala o adaptador SDL;
- o core e os testes headless compilam numa máquina sem SDL;
- deve ser possível mover `examples/sdl2_demo` para outro repositório sem alterar uma linha do core.

## 4. Utilitários internos autónomos

Criar utilitários pequenos, sem dependências de motor:

| Ficheiro | Responsabilidade |
|---|---|
| `Types.hpp` | inteiros de tamanho fixo, `WidgetId`, `FontId`, asserts/configuração pública mínima |
| `Math.hpp` | `Vec2`, `Vec3`, `Rect`, `clamp`, `lerp`, hit-test e interseção de retângulos |
| `Color.hpp` | RGBA8, conversão segura e interpolação |
| `Hash.hpp` | hash de 64 bits, combinação de IDs e deteção opcional de colisões em debug |
| `Utf8.hpp/.cpp` | avanço/recuo por codepoint, inserção, remoção e validação mínima para `TextInput` |
| `Memory.hpp` | wrappers locais de copy/move/set de memória via headers C globais |
| `Format.hpp` | formatação de inteiros/floats baseada em `ct::String` |
| `Assert.hpp` | política de assert/falha sem depender de exceções |

Os tipos públicos não devem expor `std::string`, `std::vector`, `std::function` ou tipos concretos de um backend. Usar valores simples, `ct::StringView`, `ct::Span` e handles opacos.

A regra do projeto será mais estrita: **não pode existir `std::` em nenhum ficheiro de `include/igui` ou `src`**. Para isso:

- usar os tipos globais de `<stdint.h>`/`<stddef.h>` (`uint32_t`, `size_t`, etc.);
- usar `ct::sort` em vez de algoritmos STL;
- usar `ct::Array` para arrays fixos;
- usar `ct::String::number()`/`append_number()` para formatar números dos widgets;
- concentrar `memcpy`, `memmove`, `memset` e operações matemáticas necessárias em wrappers internos pequenos sobre `<string.h>` e `<math.h>`, sem namespace `std`;
- evitar templates próprios que obriguem o iGUI a importar type traits da STL.

A implementação da dependência `ct` usa alguns componentes de baixo nível da biblioteca C++ internamente. Isso fica encapsulado em `third_party/containers` e não conta como uso de `std::` pelo código do iGUI. Se a intenção futura for também remover `std::` de dentro de `ct`, isso deve ser tratado no repositório `containers`, como trabalho separado.

## 5. Integração dos contentores `ct`

Adicionar `containers` como dependência CMake fixada a um commit conhecido. Para builds reproduzíveis, preferir submodule em `third_party/containers` ou `FetchContent` com `GIT_TAG` igual a um commit, nunca seguir `main` implicitamente.

O commit observado durante este levantamento foi `fa22048488b4da6d1e370ad739cb8119c0562df6`. Antes de distribuir iGUI, adicionar/confirmar um ficheiro de licença no repositório `containers`, pois atualmente não foi encontrado um.

Mapeamento proposto:

| Uso atual/novo | Contentor `ct` | Nota |
|---|---|---|
| `std::string` | `ct::String` | nomes persistentes, labels copiadas e texto editável |
| string sem posse | `ct::StringView` | API de labels e texto de items |
| `std::vector<T>` | `ct::Vector<T>` | ordem Z, comandos, vértices e clips |
| fila de eventos | `ct::Deque<Event>` | FIFO eficiente, permite drenar todos os eventos no início do frame |
| arrays fixos | `ct::Array<T, N>` | pontos temporários e tabelas de tamanho conhecido |
| arrays recebidos pela API | `ct::Span<const T>` | substitui ponteiro + count sempre que possível |
| `std::unordered_map<WidgetId, State>` | `ct::HashMap<WidgetId, State>` | nunca guardar ponteiros/referências através de insert/rehash |
| callbacks opcionais | `ct::Function` | apenas onde retornar um valor imediato não for suficiente |
| payloads de eventos/comandos | união tagged trivial | conjunto fechado, sem RTTI/alocação e layout simples para o backend |
| stack de IDs/clips | `ct::Vector` ou `ct::Stack` | reservar capacidade no arranque do contexto |
| janelas persistentes | `ct::SlotMap<WindowState>` | referências externas e ordem Z usam `Handle<WindowState>` |
| lookup de janela | `ct::HashMap<WidgetId, WindowHandle>` | separa lookup da posse |
| dados temporários de frame | `ct::Vector` primeiro; `ct::Arena` depois de medir | evitar otimização prematura durante a migração |
| ordenação Z | `ct::sort` | elimina `std::sort` do código atual |

Não fazer substituição textual de `unordered_map` por `HashMap`. O `ct::HashMap` usa open addressing e um `rehash` move os valores. O código atual guarda `WindowData*` dentro de `m_windowOrder`, o que criaria ponteiros pendurados. A solução é guardar handles e resolver o handle no momento de uso.

Também é necessário adaptar loops: o iterador de `ct::HashMap` expõe `entry.key` e `entry.value`, não `pair.first` e `pair.second`; `find()` devolve `V*`, não um iterador.

## 6. Estrutura de ficheiros alvo

```text
iGUI/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── plan.md
├── include/igui/
│   ├── Gui.hpp                 # API immediate-mode
│   ├── Backend.hpp             # ig::Backend e métricas de texto
│   ├── DrawData.hpp            # comandos neutros e spans de saída
│   ├── Events.hpp              # eventos, teclas, botões e FrameInfo
│   ├── Theme.hpp
│   ├── Types.hpp
│   ├── Math.hpp
│   └── Color.hpp
├── src/
│   ├── Gui.cpp                 # frame, janelas, foco e IDs
│   ├── Widgets.cpp             # widgets básicos
│   ├── TextEdit.cpp            # edição e UTF-8
│   ├── DrawList.cpp            # gravação/composição de comandos
│   └── Utf8.cpp
├── cmake/
│   └── CheckNoStd.cmake        # falha se include/igui ou src contiverem std::
├── tests/
│   ├── TestBackend.hpp         # métricas determinísticas e captura de DrawData
│   ├── test_events.cpp
│   ├── test_ids.cpp
│   ├── test_windows.cpp
│   ├── test_widgets.cpp
│   ├── test_text.cpp
│   └── test_draw_order.cpp
├── examples/
│   └── sdl2_demo/              # consumidor externo; não pertence ao target igui
│       ├── CMakeLists.txt
│       ├── SdlBackend.hpp
│       ├── SdlBackend.cpp
│       ├── SdlEvents.hpp
│       ├── SdlEvents.cpp
│       └── main.cpp
└── third_party/
    └── containers/             # se for adotado o submodule
```

`GUI.hpp` pode permanecer temporariamente como fachada/deprecation header. `GUI.cpp` será desmontado por partes e removido quando a paridade estiver confirmada.

`tmp/` não entra em nenhum target CMake. Depois de extrair apenas ideias e documentar a proveniência, deve ser movido para documentação externa ou removido numa decisão separada.

## 7. API pública inicial

Definir primeiro uma API pequena e estável:

```cpp
namespace ig {

class Context {
public:
    explicit Context(Backend& backend);

    void pushEvent(const Event& event);
    void beginFrame(const FrameInfo& frame);
    const DrawData& endFrame();

    bool beginWindow(StringView title, Rect initialBounds, bool* open = nullptr,
                     WindowFlags flags = WindowFlags::Default);
    void endWindow();

    void pushId(uint64_t id);
    void pushId(StringView id);
    void popId();

    bool button(StringView label, Rect bounds);
    bool checkbox(StringView label, bool& value, Rect bounds);
    bool sliderFloat(StringView label, float& value, float min, float max, Rect bounds);
    bool textInput(StringView label, TextBuffer buffer, Rect bounds);

    void setTheme(const Theme& theme);
    const Theme& theme() const;
    bool wantsPointer() const;
    bool wantsKeyboard() const;
    bool wantsTextInput() const;
};

} // namespace ig
```

As assinaturas finais devem evitar combinações inválidas. Referências substituem ponteiros obrigatórios. `TextBuffer` transporta ponteiro, tamanho usado e capacidade sem chamar `strlen()` cegamente. Listas usam `Span<const StringView>`.

Manter retornos imediatos (`true` quando houve alteração/click) como caminho principal. Callbacks são opcionais; não é necessário transformar uma biblioteca immediate-mode num sistema retained-mode.

## 8. Fases de execução

### Fase 0 — Baseline e regras

Entregáveis:

- criar o build CMake da implementação antiga apenas para expor erros reais;
- documentar C++14 como mínimo inicial, alinhado com `ct`;
- adicionar warnings altos e builds Debug/Release;
- registar comportamento esperado de cada widget atual;
- decidir e documentar a licença do iGUI e do código em `tmp/`.

Critério de saída: existe uma lista explícita de erros/dependências, e nenhum include implícito depende de `pch.h`.

### Fase 1 — Core autónomo e contentores

Entregáveis:

- criar `Types`, `Math`, `Color`, `Hash` e `Assert`;
- integrar `ct` por commit fixo;
- criar namespace público `ig`;
- substituir contentores STL segundo a tabela acima;
- trocar a fila de eventos por `ct::Deque`, arrays fixos por `ct::Array` e ordenação por `ct::sort`;
- adicionar o target/teste `igui_no_std`, que procura `std::` apenas no código próprio do core;
- implementar IDs estáveis e stack de IDs;
- criar storage de janelas com handles, sem ponteiros persistentes.

Critério de saída: os headers públicos compilam sozinhos, não incluem tipos do motor antigo e `igui_no_std` confirma zero ocorrências de `std::` em `include/igui` e `src`.

### Fase 2 — Eventos e máquina de interação

Entregáveis:

- `ct::Deque<Event>` e `InputState` internos;
- captura de pointer, hot, active, focus e navegação básica;
- `FocusLost`, wheel, modificadores e múltiplos eventos de texto por frame;
- `FrameInfo::deltaSeconds` para cursor e spinner;
- garbage collection de `ControlState`.

Critério de saída: sequências sintéticas de eventos conseguem clicar, arrastar, focar, escrever e cancelar sem qualquer janela real.

### Fase 3 — DrawData e backend virtual

Entregáveis:

- `DrawList`, `DrawCommand`, clips, layers e ownership de texto/vértices;
- `ig::Backend` e `TestBackend` headless;
- união tagged trivial para `GeometryCommand`/`TextCommand` e handles opacos de font/textura;
- `demo::SdlBackend`, tradutor de eventos e demo visual, todos fora do target `igui`;
- composição das janelas por ordem Z;
- overlay para dropdown;
- precisão `float` preservada até ao backend;
- espessura de contorno respeitada.

Critério de saída: testes headless validam comandos, clips, ordem, DPI, métricas de texto e duração de vida de `DrawData`; a demo SDL2 apresenta uma janela, recebe eventos e desenha pelo menos `Label`, `Button` e `Checkbox`.

### Fase 4 — Migrar widgets por grupos

Ordem recomendada:

1. `Label`, `LabelColored`, `Text`, `Separator`, `SeparatorText`, `ProgressBar`;
2. `Button`, `Checkbox`, `RadioButton`, `ToggleSwitch`;
3. sliders horizontais/verticais e `DragFloat`;
4. `TextInput` com UTF-8 e cursor;
5. `ListBox` e `Dropdown`, incluindo wheel, clip e overlay;
6. `ColorPicker` com gradiente por vértices;
7. `Spinner` usando `deltaSeconds`;
8. janela: mover, minimizar, fechar, resize e scroll.

Cada migração inclui teste de input e snapshot estrutural dos comandos de desenho. Só apagar a versão antiga do widget depois de atingir paridade definida; não é obrigatório preservar bugs visuais.

Critério de saída: todos os widgets do header antigo têm implementação nova ou estão marcados explicitamente como removidos/deprecated.

### Fase 5 — Compatibilidade e validação de backends

Entregáveis:

- fachada temporária para facilitar a troca de `GUI` por `ig::Context`;
- adaptador opcional do `RenderBatch` antigo, num target separado e desligado por defeito, caso essas classes voltem a estar disponíveis;
- consolidar o exemplo SDL2 como integração externa documentada;
- preparar um segundo backend apenas quando for necessário validar a neutralidade da API.

O backend de exemplo não define a API do core. Se revelar uma necessidade nova, adicionar uma capacidade neutra a `DrawData`, não tipos do backend ao iGUI.

Critério de saída: a mesma demo funciona com `TestBackend` e com pelo menos um backend visual sem alterar o código dos widgets.

### Fase 6 — Robustez e distribuição

Entregáveis:

- testes com ASan/UBSan;
- testes de IDs duplicados, eventos fora de ordem, `FocusLost`, viewport vazio, listas vazias e buffers cheios;
- limites configuráveis para comandos, texto e vértices por frame;
- medição de alocações depois do warm-up;
- documentação de integração de backend e eventos;
- instalação CMake/export target `igui::igui`;
- CI para GCC, Clang e, se necessário, MSVC.
- verificação `igui_no_std` obrigatória em CI, excluindo apenas `third_party`.

Critério de saída: zero dependências gráficas no target `igui`, zero `std::` no código próprio, zero alocações inesperadas no frame estável e testes limpos com sanitizers.

## 9. Testes de aceitação essenciais

- Duas instâncias de `ig::Context` não partilham estado.
- Inserir/remover uma janela não invalida os handles guardados na ordem Z.
- Um widget condicional não transfere foco/estado ao widget seguinte.
- Clique começa apenas no topo e só confirma no controlo correto.
- Drag continua fora dos limites até ao release; `FocusLost` cancela-o.
- A janela focada é também desenhada no topo.
- Conteúdo é cortado na área da janela; dropdown usa overlay.
- Wheel afeta apenas o alvo elegível.
- `TextInput` aceita vários codepoints UTF-8 no mesmo frame e não corta um codepoint a meio.
- Medir texto e renderizar usam o mesmo `FontId`, tamanho e escala DPI.
- Um backend vazio/headless permite executar toda a lógica.
- Criar frames sem widgets não deixa estado ativo nem comandos antigos.

## 10. Limites da primeira versão

Para manter o trabalho controlado, deixar fora do primeiro milestone:

- árvore retained-mode de widgets;
- animações complexas;
- docking;
- acessibilidade nativa;
- IME/composição avançada (mas a API de eventos não deve impedir a adição futura);
- multi-viewport/múltiplas janelas do sistema;
- imagens/texturas, salvo se forem necessárias ao backend de exemplo.

## 11. Primeira entrega implementável

O primeiro milestone deve produzir um núcleo pequeno que já prove a arquitetura:

1. build CMake + dependência `ct` fixada;
2. tipos internos (`Vec2`, `Rect`, `Color`, IDs);
3. eventos de pointer e `FrameInfo`;
4. `DrawData` com rect, line, text e clip;
5. `ig::Backend` + `TestBackend`;
6. `ig::Context` com uma janela, `Label`, `Button` e `Checkbox`;
7. `demo::SdlBackend` e tradução de eventos SDL2, fora da biblioteca;
8. demo SDL2 interativa;
9. testes de click, foco, clipping, ordem Z e independência entre contextos.

Só depois deste corte vertical se devem migrar os widgets mais complexos. Assim a arquitetura de eventos/backend/estado é validada antes de transportar as cerca de duas mil linhas atuais.

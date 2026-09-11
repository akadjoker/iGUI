# Plano: separar immediate mode e retained mode na build

Objectivo: poder escolher no CMake (`-DIGUI_BUILD_IMMEDIATE=...`,
`-DIGUI_BUILD_RETAINED=...`) compilar só o que se precisa, sem arrastar o
outro modo.

**Estado: implementado e verificado.** As 4 combinações da tabela na secção
5 foram testadas de facto (build + ctest) e correspondem ao previsto. Duas
coisas que este plano não tinha apanhado, descobertas ao implementar:

- `tests/test_core.cpp` usa `ig::Context` directamente além da API legacy —
  só ligava antes porque `igui_legacy` arrastava `igui` inteiro por baixo.
  Agora liga explicitamente a `igui_legacy` e `igui`, e o executável só é
  criado quando `IGUI_BUILD_LEGACY AND IGUI_BUILD_IMMEDIATE`.
- `igui_font` estava `PUBLIC igui` sem precisar (só usa `Backend.hpp`/
  `ct::Vector`) — corrigido para `PUBLIC igui_core`. Consequência: os
  consumidores que chamam `ig::Context` directamente mas só ligavam a
  `igui_sdl2`/`igui_font` (arrastando `igui` por baixo) deixaram de
  compilar — `igui_sdl2_demo` e `tests/test_sdl2.cpp` agora ligam a `igui`
  explicitamente, e ambos passam a exigir `IGUI_BUILD_IMMEDIATE`.

Um teste (`igui_tests` / `test_selectable`, assert em
`data.vertices[8].color.r`) falha na combinação ON/ON/ON — confirmado que
já falhava exactamente da mesma forma no código original antes deste
split (não é uma regressão desta mudança).

---

## Situação actual (verificado no código, não assumido)

Confirmado por grep cruzado entre os módulos:

- `src/Gui.cpp` / `include/igui/Gui.hpp` (`ig::Context`, o immediate mode:
  botões, sliders, tabelas, docking, tree, gizmos, etc.) — **zero
  referências** a partir do retained (`src/widgets/*`) ou do legacy
  (`src/iGUI.cpp`). Confirmado.
- `src/widgets/*.cpp` (retained toolkit: `Retained.cpp`, `Widget.cpp`,
  `WidgetApp.cpp`, etc.) e `src/iGUI.cpp` (legacy) **não usam `ig::Context`
  nem nada de `Gui.hpp`.**
- O que ambos **realmente usam** é a camada de baixo nível partilhada:
  `ig::DrawList` / `ig::DrawData` / `ig::Backend` (`src/DrawList.cpp`,
  `include/igui/DrawData.hpp`, `include/igui/Backend.hpp`) — confirmado por
  grep em `src/iGUI.cpp` (`ig::DrawList m_drawList`, `ig::DrawData`) e nos
  headers/cpp do retained.

Ou seja: **não há dependência lógica entre os dois modos** — a única coisa
partilhada é o "motor de desenho" (DrawList/Backend), que já era suposto ser
partilhado.

### Onde está o acoplamento (é só no CMakeLists.txt)

```
add_library(igui
    src/DrawList.cpp   <- partilhado (precisa de todos)
    src/FileDialog.cpp <- só immediate mode
    src/Gui.cpp         <- só immediate mode (Context, ~5000 linhas)
)
```

Este único target `igui` mistura o motor de desenho partilhado com a API
immediate-mode. Depois:

- `igui_legacy` (`src/iGUI.cpp`) faz `target_link_libraries(... PUBLIC igui)`
  — só para herdar `ct` e o DrawList, mas arrasta `Gui.cpp` +
  `FileDialog.cpp` inteiros para o link.
- `igui_widgets` (retained) faz o mesmo `target_link_libraries(... PUBLIC
  igui)` — mesmo problema, e ainda por cima está **aninhado dentro do bloco
  `if(IGUI_BUILD_SDL2_DEMO)`** no CMakeLists.txt (linha ~197), ou seja hoje
  nem sequer existe uma opção independente para construir só o retained.

Resultado prático: hoje é impossível compilar "só retained" ou "só
immediate" — construir qualquer um dos dois obriga a compilar o `Context`
inteiro (`Gui.cpp`, ~5000 linhas) porque está no mesmo target que o
DrawList.

---

## Plano de separação

### 1. Extrair o motor partilhado para um target próprio

Novo target `igui_core` (nome a confirmar), só com `src/DrawList.cpp`
(+ headers `Backend.hpp`, `DrawData.hpp`, `Color.hpp`, `Math.hpp`,
`Theme.hpp` — já são header-only/triviais). Liga só a `ct`.

### 2. `igui` (immediate mode) passa a depender de `igui_core`

```
add_library(igui src/Gui.cpp src/FileDialog.cpp)
target_link_libraries(igui PUBLIC igui_core)
```

### 3. `igui_legacy` e `igui_widgets` passam a depender só de `igui_core`

Deixam de arrastar `Gui.cpp`/`FileDialog.cpp`:

```
target_link_libraries(igui_legacy  PUBLIC igui_core)   # em vez de igui
target_link_libraries(igui_widgets PUBLIC igui_core)   # em vez de igui
```

### 4. Novas opções de build, independentes

```
option(IGUI_BUILD_IMMEDIATE "Build the immediate-mode Context API" ON)
option(IGUI_BUILD_RETAINED  "Build the retained widget toolkit"    ON)
option(IGUI_BUILD_LEGACY    "Build the old retained GUI compat layer" ON)
```

- `igui_widgets` sai de dentro do `if(IGUI_BUILD_SDL2_DEMO)` e passa a ter o
  seu próprio `if(IGUI_BUILD_RETAINED)` — deixa também de depender
  implicitamente de `IGUI_BUILD_SDL2_DEMO` estar ligado.
- `igui` (Context) passa a estar dentro de `if(IGUI_BUILD_IMMEDIATE)`.
- Cada demo/exemplo/teste passa a só se registar se a opção de que depende
  estiver ligada (ex.: `igui_widget_gallery_tests` precisa de
  `IGUI_BUILD_IMMEDIATE`; `igui_original_widgets_tests` precisa de
  `IGUI_BUILD_RETAINED`).

### 5. Consequência: combinações possíveis

| IMMEDIATE | RETAINED | Resultado |
|---|---|---|
| ON | ON | tudo (default actual) |
| ON | OFF | só `Context` (immediate) + DrawList — não compila `src/widgets/*` (~30 ficheiros, stb, PCH C++17) |
| OFF | ON | só o toolkit retained + DrawList — não compila `Gui.cpp`/`FileDialog.cpp` (~6000 linhas) |
| OFF | OFF | só `igui_core` (motor de desenho), útil para quem só quer o backend/DrawList |

---

## Riscos / pontos a confirmar antes de implementar

- `src/widgets/FileDialog.cpp` (retained) e `src/FileDialog.cpp` (immediate)
  têm o mesmo nome de ficheiro em pastas diferentes — confirmar que não há
  colisão de nome de objecto/target ao separar (já funciona hoje, mas
  convém re-confirmar depois do split).
- `igui_widgets_stb` (STB para o retained) só é criado dentro do bloco
  `IGUI_BUILD_SDL2_DEMO` hoje — tem de ser promovido para fora, condicional
  só a `IGUI_BUILD_RETAINED`.
- Testes (`tests/test_widgets.cpp` usa `igui`, `tests/test_core.cpp` e
  `tests/test_original_widgets.cpp` usam `igui_legacy`/`igui_widgets`) têm
  de passar a ser condicionais às novas opções, para `IGUI_BUILD_TESTS=ON`
  não falhar quando um dos modos está desligado.
- Confirmar que nenhum exemplo (`examples/*`) mistura os dois modos no
  mesmo binário (ex.: usa `Context` e o toolkit retained ao mesmo tempo) —
  se algum misturar, esse exemplo passa a exigir as duas opções ligadas.

## Não incluído neste plano

Isto separa a **build**, não resolve a duplicação de código já identificada
no [CODE_REVIEW.md](CODE_REVIEW.md) (P3: `DockSlot` vs `DockSide`,
comparação case-insensitive triplicada, `humanSize`/`humanDate`
reimplementados, etc.) — essa continua a ser feita à parte, unificando
utilitários partilhados (idealmente dentro do futuro `igui_core`).

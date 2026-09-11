# Code review — branch `port/ct-containers` vs `main`

Review automático (nível high, 8 agentes em paralelo) ao diff `main...HEAD`
(119 ficheiros, ~12.8k linhas). 8/8 agentes concluídos.

Prioridades: **P0** crash/corrupção de dados · **P1** bug funcional visível ·
**P2** desperdício de CPU/alocações · **P3** duplicação/limpeza estrutural.

---

## P0 — crashes / corrupção de memória

- [ ] **`src/Gui.cpp:4604` (`Context::beginTable(StringView, Span<const float>, float)`)
  — use-after-free confirmado.** `table_.columnWeights = &columnWeights[0];`
  guarda um ponteiro cru para o buffer do `Span` recebido, dentro de `table_`
  (membro de `Context`, `include/igui/Gui.hpp:624`, que sobrevive à chamada).
  Esse ponteiro só é desreferenciado mais tarde, em chamadas separadas
  (`tableColumnWidth`/`tableColumnX`, linha 4617-4626, invocadas a partir de
  `tableNextColumn()`). Como `Span` aceita qualquer temporário com
  `.data()`/`.size()`, `ctx.beginTable("t", ct::Vector<float>{1.f,2.f,1.f},
  w);` seguido de `ctx.tableNextColumn();` lê memória já libertada — corrompe
  o layout das colunas ou crasha sob ASan. **Confirmado por inspecção direta
  do código** (verificado: `columnWeights` é `const float*` em `Table`, não
  cópia).
- [ ] **`src/Gui.cpp:4963-4981` (`consumeEvents`, `EventType::FocusLost`)** —
  o handler de perda de foco reinicia `activeWidget_`, `focusedWidget_`,
  `draggingWindow_`, etc., mas nunca reinicia `dragDrop_` (estado usado por
  `treeItem`/`beginDragSource`/`acceptDragDropTarget`). `dragDrop_` só é
  limpo em `endFrame()` e só quando `pointer_.released[left]` é verdadeiro.
  Se a janela perder o foco (alt-tab) a meio de um drag, sem `mouse-up`
  entregue, `dragDrop_.active`/`armed` ficam presos para sempre: o tooltip
  de preview continua a desenhar-se na posição antiga do rato, e o próximo
  clique normal num drop target qualquer completa um "drop fantasma" com o
  payload antigo.
- [ ] **`src/Gui.cpp:941-991` (`Context::treeItem` — overload simples)** — a
  versão documentada como não-arrastável, `treeItem(label, expanded, style,
  bounds)`, delega na overload arrastável com `drop = nullptr`, mas o bloco
  que arma o drag (`pressedOnRow`, linhas 966-991) não está condicionado a
  `drop != nullptr`. Mesmo esta API "sem drag/drop" arma o único slot global
  `dragDrop_` ao pressionar o rato (payload `"TREEITEM"`, `source =
  makeWidgetId(label)`), mostra um preview de drag ao arrastar, e — se
  existir noutro sítio um `treeItem`/`acceptDragDropTarget` a aceitar o
  mesmo tipo de payload — pode entregar um `source` inválido (hash da label,
  não um id real de nó) à lógica de reparent da app.

## P1 — bugs funcionais

- [ ] **`src/widgets/WidgetSerializer.cpp:260`** — `j["tags"].items()` é chamado
  sem verificar `is_array()` antes (ao contrário de `deserializeChildren`, que
  verifica). `ct::Json::items()` chama `detail::fatal()` → `std::abort()` se o
  valor não for array. Um `"tags": "x"` num JSON de layout guardado (à mão ou
  corrompido) derruba a aplicação inteira ao carregar.
- [ ] **`src/widgets/WidgetSerializer.cpp:238-241`** — mesmo problema para
  `"rect"`: `r[0]..r[3]` lidos via `at()`/`operator[]` sem confirmar que `r` é
  array com 4 elementos. Um `"rect"` com menos elementos ou tipo errado aborta
  o processo (o código antigo com nlohmann pelo menos lançava excepção
  apanhável).
- [ ] **`include/igui/Gui.hpp` (overloads de `comboBox`/`sliderFloat`/
  `inputText`/`progressBar`)** — o `width` por omissão mudou de `180.0f` para
  `0.0f` (agora significa "preencher largura disponível" via
  `availableWidth()` em `src/Gui.cpp`). Confirmado **independentemente por 2
  agentes**. `examples/sdl2_demo/main.cpp` não foi tocado por este branch e
  chama estes 4 widgets sem `width` explícito — passam a esticar para toda a
  largura da janela (~504px) enquanto o `selectable` ao lado fica fixo em
  180px, partindo visualmente o layout da demo sem nenhum erro de compilação.
- [ ] **`src/FileDialog.cpp:498-500`** — `scrollOffset` do scroll da lista de
  ficheiros só é limitado em baixo (`>= 0`), nunca em cima. Em pastas com
  poucos ficheiros, continuar a rodar a roda do rato faz `scrollOffset`
  crescer além do conteúdo total; todas as linhas passam a falhar o teste
  `item.bottom() <= entriesArea.y` e a lista fica vazia até se fazer scroll
  manual para trás.
- [ ] **`src/FileDialog.cpp:399-402`** — Backspace no campo "nova pasta"
  apaga sempre 1 byte, não um code point UTF-8 completo (ao contrário da
  inserção, que avança por `event.textLength`). Escrever "café" e dar
  Backspace uma vez deixa uma sequência UTF-8 inválida/truncada, que é depois
  passada a `provider.createDirectory(...)`.
- [ ] **`src/FileDialog.cpp:527-531`** — hit-test da vista de ícones não
  limita `column` ao número de colunas calculado. Quando
  `entriesArea.width` não é múltiplo exacto da célula (92px), clicar na
  faixa morta à direita calcula `column == columns`, seleccionando o
  primeiro item da linha seguinte em vez de nada.
- [ ] **`examples/sdl2_original_widgets/stages/stage_controls.cpp:75`** — o
  rename mecânico `BuGUI` → `ig::retained` foi aplicado dentro de uma string
  literal visível na UI (lista de tags de demo), trocando o texto mostrado
  `"BuGUI"` por `"ig::retained"`. É texto de utilizador, não deve seguir o
  rename automático de identificadores.

## P2 — desperdício de trabalho (CPU/alocações por frame)

- [ ] **`src/FileDialog.cpp:506-518` e `591-601`** — `visibleEntries` é
  reconstruído do zero (vector novo + filtro O(n)) em **todos os frames**
  em que o diálogo está aberto, mesmo sem alterações a `entries`/`filter`/
  `showHidden`. Guardar a lista filtrada em `FileDialogState` com uma dirty
  flag evita o trabalho repetido.
- [ ] **`src/DrawList.cpp:220-276` (`addCircleFilled`)** — o novo anel de
  anti-aliasing triplica o nº de índices e duplica vértices por círculo,
  com `cosf`/`sinf` recalculados por vértice, por frame, nos ~15 pontos de
  chamada (radio buttons, knobs, color wheel, gizmo handles). Vale a pena
  uma tabela de seno/cosseno pré-calculada, ou aplicar a franja de AA só
  acima de um raio mínimo.
- [ ] **`adapters/raylib/RaylibBackend.cpp:243-277` (`renderGeometry`)** —
  duas passagens completas sobre todos os índices por comando de desenho,
  por frame: uma só para validar bounds, outra para emitir os triângulos.
  Fundir num único loop (ou confiar no invariante já garantido pelo
  `DrawList`) poupa metade do trabalho.
- [ ] **`examples/raylib_dock_demo/main.cpp:26-59`** — `entries.push_back`
  em loop sobre `files.count` sem `reserve(files.count)` (menor, só corre
  ao navegar, não por frame).

## P3 — duplicação e simplificação

**Reimplementações que já existem noutro sítio:**
- `colorFromHsv`/`colorToHsv` (`src/Gui.cpp:255,283`) reimplementam
  `Color::FromHSV`/`Color::ToHSV` (`include/igui/Color.hpp:67-114`) em vez
  de os chamar.
- `fileDialogSize` (`src/FileDialog.cpp:64-76`) reimplementa
  `FileSystem::humanSize` (`src/widgets/FileSystem.cpp:106-118`), thresholds
  e formatos idênticos.
- `fileDialogTime` (`src/FileDialog.cpp:78-88`) reimplementa
  `FileSystem::humanDate` (`src/widgets/FileSystem.cpp:120-134`).
- Comparação case-insensitive ASCII reimplementada **pela terceira vez**:
  `sameAsciiIgnoreCase` (`src/FileDialog.cpp:14-27`) duplica `toLowerStr`
  (`src/widgets/FileDialog.cpp:27`) e `containsCI`
  (`src/widgets/ConsoleWidget.cpp:18`) — e este diff já introduz
  `include/igui/widgets/String.hpp`, que seria o sítio natural para uma só
  versão partilhada.

**Duplicação estrutural dentro do próprio diff:**
- Bloco de construção de `visibleEntries` (sentinelas + filtro) duplicado
  verbatim em `src/FileDialog.cpp:506-518` e outra vez em `591-607` (dentro
  do `refresh`).
- `selectedIndex` + `selectedPath` em `FileDialogState`
  (`include/igui/FileDialog.hpp`) — `selectedPath` é derivável de
  `selectedIndex`, mas é mantido sincronizado à mão em ~4 sítios
  (`src/FileDialog.cpp:550-551,566-567,604-605`).
- Cadeias if/else de hit-test para ~12 botões da toolbar, duplicadas quase
  verbatim entre o bloco de press (`src/FileDialog.cpp:296-330`) e o de
  release (`332-364`) — os breadcrumbs ao lado já usam tabela+loop para o
  mesmo tipo de teste.
- `WidgetId` construídos com literais ASCII-em-hex à mão (ex.:
  `combineIds(id, 0x4241434bull)` para `"BACK"`), repetidos em 14 sítios
  sem constantes nomeadas (`src/FileDialog.cpp:168,229-241,259,287`).
- `treeItem` (drag-reorder) faz a sua própria state machine de arm/threshold
  contra `dragDrop_` (`src/Gui.cpp:966-991,1033-1054`) em vez de usar a API
  genérica `beginDragSource`/`acceptDragDropTarget` já adicionada no mesmo
  diff (`src/Gui.cpp:3264,3301`), com um `WidgetId` mágico repetido
  (`0x545245454954454dull`).
- `collapsingHeader`, `treeNode` e `treeItem` calculam cada um o mesmo
  polígono da seta de expand/collapse (`src/Gui.cpp:862-899, 901-939,
  1008-1021`) em vez de partilhar um helper.
- `DockSlot` (immediate mode, `include/igui/Gui.hpp:130`) duplica o conceito
  de `DockSide` (retained mode, `include/igui/widgets/DockPanel.hpp:12`) com
  nomes/ordem diferentes e sem `Top`.

**Estrutural (menor confiança):**
- `src/Gui.cpp` cresceu de 939 para ~5460 linhas como um único `Context`
  monolítico, apesar de `FileDialog.cpp` mostrar que já há apetite para
  separar por feature.

---

## Notas de contexto (não são bugs)
- O rename `BuGUI`→`ig::retained` / `GUI`→`iGUI` (STL→`ct` containers) foi
  verificado como mecânico e consistente em toda a árvore — sem chamadas
  antigas soltas, sem `Color::WHITE` etc. esquecido a ser usado.
- `plan.md` removido é só um documento de planeamento em português, não é
  "load-bearing".

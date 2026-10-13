# Review: modo retido (`ig::retained`)

Feito a 2026-10-13 sobre o commit `9281675`. As linhas são as desse commit.

## O que foi lido

- **Por inteiro:** o núcleo (`Widget`, `WidgetApp`, `Retained`, `Signal`) e os
  widgets base (layout, scroll, input, texto, dados, diálogos, menus, views,
  `FileDialog`, `DirCache`).
- **Só com pesquisas dirigidas:** `CodeEditor`, `AudioWidgets`, `ChartWidgets`,
  `GizmoWidgets`, `BuImage`, `IconAtlas`, `AppWidgets`, `ToolBar` e
  `AutomotiveWidgets`.

## Como foram confirmados

Treze dos quinze foram reproduzidos com sondas ligadas à `libigui_widgets.a`, com
ASan onde o código é instrumentável. O 3 e o 13 são só de leitura (a sonda do 3
correu sem o ASan dizer nada). As sondas não estão no repo; cada problema que se
arranjar deve ganhar o seu teste em `tests/test_original_widgets.cpp`.

## Memória (use-after-free e fugas)

- [x] **1. O `DirCache` deixa ponteiros pendurados quando o buffer cresce** —
      [DirCache.cpp:58](../src/widgets/DirCache.cpp#L58) (`allocKids`).
      Entrega ponteiros para dentro de `kidsBuf_` e, quando o buffer cresce, faz
      `malloc` + `memcpy` + `free`: o `node->kids` de todos os nós já lidos fica a
      apontar para memória libertada.
      *Como se vê:* FileDialog aberto em `~` (40 entradas); entrar numa pasta com
      cerca de 500 ficheiros passa o `kidsBufUsed_` de 512 e o buffer é realocado;
      voltar a `~` pelo breadcrumb apanha o nó em cache e o `refreshDir()` lê
      `node->kids[i]` de memória libertada. ASan: heap-use-after-free ao ler
      `root->kids` depois de um `deepScan(/usr/share)`.
      *Arranjo:* guardar índices no buffer em vez de ponteiros, ou alocar os
      filhos em blocos que não se movem.

- [x] **2. `removeStage()` apaga uma stage que ainda está a ser pintada** —
      [WidgetApp.cpp:647](../src/widgets/WidgetApp.cpp#L647).
      Não limpa o `transOldRoot_`, que o `paint()` continua a desenhar enquanto a
      transição decorre (SlideLeft de 0,3 s por omissão). O
      `notifyWidgetRemoved()` também não conhece o `transOldRoot_`.
      *Como se vê:* `app.setStage("game"); app.removeStage("menu");` dá SEGV no
      `WidgetApp::paint` seguinte, com a configuração por omissão.

- [x] **3. `DataGrid::setModel()` e `TreeView::setModel()` nunca se desligam do
      modelo** — [DataWidgets.cpp:885](../src/widgets/DataWidgets.cpp#L885).
      Ligam aos sinais do modelo lambdas que capturam `this`; não há `~DataGrid`
      nem `SlotId`s guardados. *Só por leitura.*
      *Como se vê:* um modelo partilhado e uma DataGrid dentro de um FloatWindow
      que o utilizador fecha: no `model.addRow()` seguinte, o
      `rowsInserted.emit()` corre `rebuildSortOrder()` sobre a grid libertada.
      Chamar `setModel()` duas vezes acumula ligações, e a grid continua a reagir
      ao modelo antigo.

- [x] **4. Os popups "owned" nunca são destruídos** —
      [WidgetApp.cpp:224](../src/widgets/WidgetApp.cpp#L224).
      O `notifyWidgetRemoved(popupClosing_)` põe o `popupClosing_` a `nullptr`
      antes de ele ser copiado para `pendingPopupDelete_` (o mesmo no `showPopup`,
      linhas 969-973 e 981-985).
      *Como se vê:* cada abrir e fechar de uma ComboBox, DatePicker,
      AlertDialog/ConfirmDialog ou do popup de cor da PropertyGrid deixa o widget
      do popup por libertar. `showPopup(p, nullptr, true)` + `closePopup()` + 60
      frames de `update()`, e o destrutor de `p` nunca corre.

## Rato e teclado no sítio errado

- [ ] **5. O duplo clique do `FileDialog` é sempre verdade** —
      [FileDialog.cpp:887](../src/widgets/FileDialog.cpp#L887).
      Calcula `now = lastClickTime + deltaTime()`, por isso `now - lastClickTime`
      é sempre um frame (menos de 0,4 s), e guarda o estado em `static`s
      partilhados por todos os diálogos.
      *Como se vê:* um clique numa pasta e, dez segundos depois, outro clique na
      mesma linha: o diálogo entra na pasta; num ficheiro, emite `accepted` e
      fecha. Como o `lastIdx` sobrevive à navegação, um só clique na linha N da
      listagem seguinte também abre.
      *Arranjo:* usar o `e.clickCount`, que o `WidgetApp` já calcula.

- [ ] **6. A `ListWidget` rola no paint e o hit-test não sabe** —
      [ScrollWidgets.cpp:596](../src/widgets/ScrollWidgets.cpp#L596).
      Move a linha, pinta e repõe, sem redefinir o `scrollOffset()`.
      *Como se vê:* lista com um Button por linha, scroll de 3 linhas, clique no
      botão da linha do topo (a 3): quem recebe o `clicked` é o botão da linha 0.
      Um botão "apagar" atua sobre o item errado. O `connectRowPress` também
      captura o índice no `addRow`, que fica errado depois de um `removeRow()`.

- [ ] **7. A roda e a barra de scroll não consomem o evento** —
      [ScrollWidgets.cpp:260](../src/widgets/ScrollWidgets.cpp#L260).
      `ScrollView`/`ListBox`/`ListWidget::onMouseScroll` e
      `ScrollBar::onMousePress` nunca põem `e.consumed`.
      *Como se vê:* a roda sobre uma ScrollView dentro de outra rola as duas
      (dentro 30, fora 30). Um clique na calha da barra de uma ListBox sobe até ao
      `ListBox::onMousePress`, que seleciona a linha a essa altura e emite
      `selectionChanged(30)`.

- [ ] **8. Enter e Escape não fazem nada na `InputBox` nem no `FileDialog`** —
      [DialogWidgets.cpp:612](../src/widgets/DialogWidgets.cpp#L612).
      O `TextInput::onKeyPress` consome sempre o Return e o Escape, e nenhum dos
      dois diálogos liga o `submitted`. Além disso nada dá foco ao campo da
      InputBox nem ao Dialog modal: o `Dialog::show()` (linha 35) não chama
      `setFocused`, por isso o `Dialog::onKeyPress` nunca é alcançado.
      *Como se vê:* "Rename" abre uma InputBox e o que se escreve vai para o
      editor por trás; depois de clicar no campo, Enter e Escape dão
      `accepted=0 cancelled=0`. No FileDialog, Enter no campo do nome também não
      abre ([FileDialog.cpp:1000-1002](../src/widgets/FileDialog.cpp#L1000-L1002)).

- [ ] **9. Fechar um popup com um clique fora deixa o dono a pensar que está
      aberto** — [ComboBox.cpp:254](../src/widgets/ComboBox.cpp#L254).
      O estado vive no dono (`ComboBox::open_`, `DatePicker::open_`), mas é o
      `WidgetApp` que fecha sem avisar.
      *Como se vê:* abrir uma ComboBox, clicar fora, clicar outra vez na combo:
      nada abre, só ao segundo clique. O DatePicker
      ([InputWidgets.cpp:895](../src/widgets/InputWidgets.cpp#L895)) tem o mesmo.
      Na MenuBar é ao contrário: clicar no título do menu aberto fecha-o e a
      `MenuBar::onMousePress` volta a abri-lo.
      *Arranjo:* o `WidgetApp` avisar o dono, ou o dono perguntar
      `popup() == o meu`.

- [ ] **10. O relógio do `WidgetApp` trunca a cada frame** —
      [WidgetApp.cpp:114](../src/widgets/WidgetApp.cpp#L114).
      `elapsedMs_ += uint32_t(dt_ * 1000)`: acima de 1000 fps nunca avança, e a
      144 Hz atrasa cerca de 13%.
      *Como se vê:* sem vsync, com frames de 0,5 ms, 2 s de `update()` somam 0 ms.
      Dois cliques no mesmo sítio com qualquer intervalo passam a contar como
      duplo ou triplo: o TextEdit seleciona palavra ou linha, o VideoCanvas repõe
      a vista, e a DataGrid e a TreeGrid
      ([DataWidgets.cpp:611](../src/widgets/DataWidgets.cpp#L611) e
      [1475](../src/widgets/DataWidgets.cpp#L1475), esta com `static`s
      partilhados) entram em edição ao segundo clique.
      *Arranjo:* acumular em float ou double e converter no fim.

## Estado que não é respeitado

- [ ] **11. `setVisible(false)` não esconde 16 widgets** —
      [Widget.cpp:445](../src/widgets/Widget.cpp#L445) (`Widget::paint`).
      O `Widget::paint()` (e o do GridLayout, do Collapsible, etc.) pinta todos os
      filhos sem ver o `isVisible()`, e estes widgets não testam o `visible_` no
      paint deles: Canvas, ImageView, VideoCanvas, ConsoleWidget, DockPanel,
      ThumbnailGrid, PlotWidget, HistogramWidget, GradientEditor, VUMeter,
      SpectrumAnalyzer, WaveformView, PianoRoll, ADSRWidget, LinearKnob e
      FileDialog.
      *Como se vê:* `canvas->setVisible(false)` num BoxLayout: o layout salta-o,
      mas o `Canvas::paint` continua a desenhar por cima dos irmãos (20 vértices
      de um Canvas invisível). O hit-test já o ignora.
      *Arranjo:* o teste no ciclo de filhos do `Widget::paint`, não em cada
      widget.

- [ ] **12. `PageView::setPage()` só esconde a página anterior no fim de uma
      animação** — [ViewWidgets.cpp:619](../src/widgets/ViewWidgets.cpp#L619).
      Com `Transition::None`, ou a trocar a meio de um slide, a anterior fica
      visível para sempre.
      *Como se vê:* `pv->setPage(1, PageView::None)`: `page0 visible=1 page1
      visible=1`. Não é pintada, mas continua no hit-test, e ao voltar atrás os
      cliques caem em widgets que não se veem. Dois `setPage()` em menos de
      0,25 s prendem a primeira página da mesma maneira.

- [ ] **13. `setEnabled(false)` não tem efeito nos campos de texto** —
      [TextInputWidgets.cpp:251](../src/widgets/TextInputWidgets.cpp#L251).
      Nem o dispatch do `WidgetApp` nem os handlers de TextInput, TextEdit,
      CodeEditor e SpinBox olham para o `enabled_`, e o estado não desce aos
      filhos. *Só por leitura.*
      *Como se vê:* um campo desativado ganha foco com um clique, aceita texto,
      Backspace e colar, e é pintado como ativo; num SpinBox desativado os botões
      e a roda continuam a mudar o valor. `panel->setEnabled(false)` não desativa
      nenhum descendente. Só o Tab os salta.

## Texto

- [ ] **14. UTF-8 inválido aborta o processo** —
      [Utf8.hpp:32](../include/igui/widgets/Utf8.hpp#L32) (`utf8CharLen`).
      Confia no byte inicial, por isso o `utf8ByteOffset()` e o `utf8Length()`
      saltam para lá do fim em sequências truncadas ou em Latin-1, e devolvem
      offsets maiores do que o `size()`, que o `ct::String::insert/substr` tratam
      com `fatal()`.
      *Como se vê:* um nome de ficheiro em ISO-8859-1 como `"Bal\xE3o"` (habitual
      em discos antigos e no que o `FindFirstFileA` devolve no Windows) chega ao
      campo do FileDialog; escrever um carácter no fim aborta com "ct::String::
      insert: pos fora dos limites". O mesmo no `TextEdit::selectedText/splitLine`
      com o cursor no fim de uma linha de um `.bb` em CP1252.

- [ ] **15. O `TextEdit` não refaz o wrap depois de juntar linhas** —
      [TextInputWidgets.cpp:872](../src/widgets/TextInputWidgets.cpp#L872).
      O `mergeWithPrevLine()` e o `deleteSelection()` mudam o `lines_` sem pôr
      `wrapDirty_ = true`.
      *Como se vê:* texto `"abc\ndefgh\nijklmnop"`, cursor no início da linha 2,
      Backspace: a linha passa a "abcdefgh" mas só se vê "abc", e as de baixo
      aparecem cortadas, até à próxima inserção (`lines=2, wrapRows=3`). Cortar
      ou apagar uma seleção de várias linhas dá o mesmo.

## Ficou de fora da lista

Reproduzidos com sonda:

- [ ] [WidgetApp.cpp:450](../src/widgets/WidgetApp.cpp#L450) — depois de largar o
      botão, o hover é recalculado só contra o `root_`: depois de um clique num
      FloatWindow passa para o widget por baixo.
- [ ] [LayoutWidgets.cpp:329](../src/widgets/LayoutWidgets.cpp#L329) — o
      Collapsible fechado zera o rect dos filhos; os que usam o `sizeHint()` por
      omissão (Canvas, ImageView) voltam com altura 0 ao reabrir.
- [ ] [LayoutWidgets.cpp:87](../src/widgets/LayoutWidgets.cpp#L87) —
      `BorderLayout::setWidget(região, mesmoWidget)` apaga-o e volta a
      adicioná-lo (SEGV no `layout()`).
- [ ] [WidgetApp.cpp:156](../src/widgets/WidgetApp.cpp#L156) — o F1 (scancode 58)
      liga o overlay de debug em qualquer aplicação e engole a tecla.
- [ ] [DataWidgets.cpp:765](../src/widgets/DataWidgets.cpp#L765) — o Tab em edição
      na DataGrid nunca passa à célula seguinte, porque o `commitEdit()` já pôs
      `editRow_/editCol_` a -1.
- [ ] [LayoutWidgets.cpp:1103](../src/widgets/LayoutWidgets.cpp#L1103) —
      `TabLayout::removeTab()` de uma tab à esquerda da ativa muda a ativa sem
      emitir `currentChanged`.
- [ ] [TextInputWidgets.cpp:1303-1327](../src/widgets/TextInputWidgets.cpp#L1303-L1327)
      — o duplo e o triplo clique do TextEdit misturam índices de byte com
      colunas de codepoint ("olá mundo" seleciona "undo").

Só por leitura:

- [ ] DataGrid, TreeGrid, PropertyGrid e ConsoleWidget editam o buffer byte a
      byte: um Backspace sobre "ç" ou "ã" deixa UTF-8 inválido (deviam usar a
      lógica do TextInput).
- [ ] [Signal.hpp:64](../include/igui/widgets/Signal.hpp#L64) — o `emit()`
      percorre o `slots_` enquanto um handler pode ligar ou desligar slots.
- [ ] [WidgetApp.cpp:526-531](../src/widgets/WidgetApp.cpp#L526-L531) — o bubble
      das teclas não tem a guarda de `deletions_` que o do rato tem.
- [ ] [InputWidgets.cpp:480](../src/widgets/InputWidgets.cpp#L480) —
      `DatePicker::setDate()` não valida o mês, que indexa `d[month]` e
      `months[]`.
- [ ] [WidgetApp.cpp:914](../src/widgets/WidgetApp.cpp#L914) — o estado das
      janelas é reposto sem limitar ao ecrã atual.
- [ ] [FileDialog.cpp:426-441](../src/widgets/FileDialog.cpp#L426-L441) — os
      breadcrumbs no Windows assumem "C:" e geram caminhos "C:/:/…".
- [ ] [FileDialog.cpp:545](../src/widgets/FileDialog.cpp#L545) e 588 —
      `panelColor.r + 15` dá overflow de u8 com o `Theme::light()` (barras
      pretas).
- [ ] [Animation.cpp:303-331](../src/widgets/Animation.cpp#L303-L331) — o
      `fadeOut` e o `slideIn` guardam `Widget*` cru, sem cancelamento.
- [ ] O TextEdit sem word-wrap não tem scroll horizontal (o `maxContentW_` é
      calculado e nunca usado).
- [ ] [ConsoleWidget.cpp:50](../src/widgets/ConsoleWidget.cpp#L50) — as linhas
      duplicadas colapsadas não são contadas mas são subtraídas no trim.
- [ ] [WidgetApp.hpp:254](../include/igui/widgets/WidgetApp.hpp#L254) e 260 —
      `setStatusText` e `statusBarHeight` estão declaradas e nunca definidas, e o
      `statusBar_` é sempre nulo.
- [ ] Eficiência: o `fireEvent("move")` aloca strings e copia callbacks em cada
      movimento do rato, por cada widget da cadeia; o `TextEdit::rebuildWrap`
      volta a medir o documento inteiro a cada tecla.

## Por onde começar

1. Os quatro de memória (1 a 4): o 1 e o 2 rebentam em uso normal.
2. O 5, o 6 e o 8: fazem a aplicação agir sobre o item errado ou não responder.
3. O 11 no `Widget::paint`, que arranja os 16 widgets de uma vez.
4. O 14, que aborta com ficheiros reais em Windows.

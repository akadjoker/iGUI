# Ronda 7 (ramo igui-round7, a partir de igui-round6 43e1ae5)

Fecha duas coisas que a ronda 6 deixou escritas como por fazer: (1) o tratador que tira o proprio widget da arvore a meio do seu sinal
(`parent->removeChild(w)`), que a ronda 6 so tornou seguro no PianoRoll, ListBox, ListWidget, Slider, Button e Menu, passa a ser corrido
contra os outros widgets que emitem um sinal, um por um; (2) o `-fsanitize=float-cast-overflow` entra no estagio asan do `tools/check.sh` e
um relatorio de um sanitizador na saida de um teste que passou faz o teste falhar. A zen_plataform (a lista, o grupo e a janela da zen_ui)
fez-se noutro repositorio: ramo `zen-ui-clip`, ver a ultima seccao. Regras da ronda: sem `std::` nem cabecalhos da biblioteca standard em
codigo novo (a contagem de `std::` por ficheiro nao subiu em nenhum: AudioWidgets.cpp 68 -> 67), nada abrandar (as instrucoes por frame
sao iguais), o hash do desenho nao muda, tudo em scope com MemoryMax=4G, sem janela no ecra (Xvfb privado, HOME e XDG em scratch), sem push.

Como construir e correr (a partir da raiz): `IGUI_CONTAINERS_DIR=<ct 34169bc> IGUI_ZEN_PLATFORM_DIR=<zen_plataform db76a13> tools/check.sh --build <dir> --jobs 3 --base 43e1ae5`.

## Estado

| # | Item | Estado | Commits | Teste |
|---|------|--------|---------|-------|
| 1 | Um tratador que tira o proprio widget da arvore, nos outros widgets | feito para 54 widgets, com as excepcoes da secao 3 | e1344c8 .. 564f067 (nove) | `tests/test_retained_round7.cpp`, alvo `igui_retained_round7` (150 casos) |
| 2 | `float-cast-overflow` e relatorios de sanitizadores no `check.sh` | feito; a suite nao tinha mais nada | c952d90 | prova de scratch (a seccao 4) |

## 1. O metodo: o controlo e dois modos

A regra da ronda 6 (`WidgetApp::hasGone`): o tratador tira o widget com `parent->removeChild(w)`; o widget que emite o sinal nao toca em
nada seu nem emite mais nada depois dele. O que se corre, para cada sinal de cada widget (`tests/test_retained_round7.cpp`, o `Stage` da
ronda 6 passou para `tests/StageHarness.hpp` com mais gestos: clique, roda, tecla, texto e `act()`, uma chamada da API feita de dentro de uma
frame):

- o CONTROLO, nos dois modos: o tratador so conta. O sinal tem de vir (um caso cujo gesto nao faz vir o sinal nao prova nada: falha com
  "CONTROL"), e os sinais e os eventos globais que se lhe seguem (as "testemunhas", p.ex. o `cursorMoved` do `textChanged` do TextEdit, o
  evento "toggle" do CheckBox) tambem;
- o caso, nos dois modos, com o tratador da regra: DENTRO de uma frame (modo 0: os eventos vao pelo despachante do WidgetApp, ou a chamada da API
  sai de uma chamada diferida que o `update()` corre: o apagamento espera pelo fim da fase e a memoria ainda la esta) e FORA de uma (modo 1: o
  tratador ou o metodo do widget chamados a mao, como faz a aplicacao com `setValue()` ou um teste com `onMouseRelease()`: o `removeChild` apaga
  logo, e um widget que se toca depois do sinal le memoria libertada: o ASan para). O tratador correu uma vez, o widget saiu da arvore
  (`hasGone`) e nenhuma testemunha veio.

`tests/test_retained_round7.cpp --list` diz os casos, `--only=TEXTO` corre os que tem TEXTO no nome (sob o ASan o programa para no primeiro
widget que le o que libertou: um processo por caso diz quais).

### O que se viu falhar

Com a biblioteca da ronda 6 (43e1ae5) e os testes de agora (os mesmos ficheiros de teste, construcao `-fsanitize=address,undefined,float-cast-overflow`,
um processo por caso): **100 dos 150 casos falham** (todos com `heap-use-after-free`; um caso por widget e gesto, a lista na seccao 2), e com a
de agora nenhum. Os 50 que passam, passam ja com a biblioteca da ronda 6: sao o "ja servia" abaixo (o sinal e a ultima coisa do metodo, ou o
codigo ja perguntava `deletions()`).

Os commits (um por familia de widgets, cada um com os seus casos de teste e a lista do que se viu falhar na mensagem):

| Commit | O que |
|--------|-------|
| c952d90 | `check.sh`: `float-cast-overflow` e os relatorios de sanitizadores |
| e1344c8 | 1/n CheckBox, RadioButton, Switch, ProgressBar, ScrollBar, ComboBox, Collapsible, StackLayout, Splitter, TabLayout, SlidePanel, SideTabBar, ToolBar, DockPanel, TextEdit, TextInput; `WidgetApp::emitGone`; o `Stage` passa para `tests/StageHarness.hpp` |
| eac9abc | 2/n Knob, Fader, LinearKnob, ADSRWidget, WaveformView, PageView, Outliner, Viewport3D |
| a016280 | 3/n Range (SpinSlider), ThumbnailGrid, AssetBrowser, NodeEditor, Chart, Menu |
| a5ab837 | 4/n TreeView, PropertyGrid (linha bool), ColorPicker |
| b9a4c50 | 5/n DataGrid, TreeGrid |
| 581bce8 | 6/n Timeline (as entradas que o teste alcanca) |
| dedc470 | 7/n CodeEditor, com `WidgetApp::DeletionHold` |
| 0a9f5b6 | 8/n Chart e SideTabBar: os sinais de Widget (`moved`, `pressed`) |
| 564f067 | 9/n o callback de uma linha botao do PropertyGrid (o teste que faltava ao 4/n) |

## 2. Por widget

(F = visto falhar com o ASan na ronda 6 e corrigido; S = ja servia, visto com o controlo. Um sinal por linha de teste; entre parenteses o gesto.)

| Widget | F (falhava na ronda 6) | S (ja servia) |
|--------|-----------------------|---------------|
| CheckBox | toggled by a click | - |
| RadioButton | toggled by setSelected(false); toggled by a click, without a group | toggled by a click, in a group |
| RadioGroup | - | selectionChanged taking the selected button out |
| Switch | toggled by a click; toggled by setOn | - |
| SpinBox | - | onValueChanged by setValue; onValueChanged by a click on the plus button; onValueChanged by the Up key; onValueChanged by the wheel |
| ProgressBar | onValueChanged by setValue | - |
| DatePicker | - | onDateChanged by setDate |
| TimePicker | - | onTimeChanged by setTime |
| ScrollBar | scrolled by a click on the track; scrolled by dragging the thumb | - |
| ComboBox | selectionChanged by setSelectedIndex; selectionChanged by removing the selected item; selectionChanged by a click on its list | - |
| Collapsible | expandedChanged by setExpanded; expandedChanged by a click on the header; checkedChanged by a click on the check box | - |
| StackLayout | currentChanged by setCurrentIndex | - |
| Splitter | ratioChanged by setRatio; ratioChanged by dragging the handle | - |
| TabLayout | currentChanged by setCurrentIndex; currentChanged by a click on a tab; tabClosed by a click on the close button | currentChanged by removing the current tab |
| Carousel | - | pageChanged by next; pageChanged by setCurrentPage |
| SlidePanel | openChanged by open; openChanged by close | - |
| SideTabBar | buttonClicked by a click on a rail button; moved (the base signal) by moving over the bar | currentChanged by setCurrentIndex; currentChanged by removing the current page |
| ToolBar | buttonClicked by a click on a button; toggleChanged by a click on a toggle | - |
| DockPanel | panelActivated by showPanel; panelActivated by a click on another tab; panelClosed by closePanel; panelClosed by a middle click on a tab | tabMenuRequested by a right click on a tab |
| TextInput | textChanged by typing | textChanged by setText; textChanged by Backspace; textChanged by Delete; textChanged by cutting the selection; submitted by Return |
| TextEdit | textChanged by typing; textChanged by Return; textChanged by Backspace; textChanged by Delete; textChanged by cutting the selection | cursorMoved by the Left key; cursorMoved by setCursorPos |
| Knob | onChanged by setValue; onChanged by dragging; onChanged by the wheel | - |
| Fader | onChanged by setValue; onChanged by a press on the track and a drag | - |
| LinearKnob | onChanged by setValue; onChanged by a press on the track | - |
| ADSRWidget | onChanged by dragging the attack handle | - |
| WaveformView | onScrub by a press; onScrub by a drag (the second signal: the press scrubs too) | - |
| RadialGauge | - | onValueChanged by setValue |
| PowerBar | - | onValueChanged by setValue |
| DriveMode | - | onModeChanged by setMode |
| DigitalSpeed | - | onValueChanged by setValue |
| BatteryGauge | - | onPercentChanged by setPercent |
| FloatWindow | - | minimizedChanged by setMinimized; minimizedChanged by a click on the minimize button; closed by a click on the close button |
| PageView | pageChanged by setPage | - |
| VideoCanvas | - | viewChanged by setViewTransform |
| Breadcrumbs | - | itemClicked by a click on a segment |
| SearchBar | - | onTextChanged by typing; onTextChanged by Backspace; onSearch by Return |
| SplitButton | - | clicked by a click on the main button |
| Outliner | selectionChanged by a click on a row | visibilityChanged by a click on the eye; lockChanged by a click on the lock |
| RichText | - | linkClicked by a click on a link |
| Viewport3D | onOrbit by a drag; onPan by a right drag; onZoom by the wheel | onResize by the first paint |
| Gizmo2D | - | onDragStart by a press on the handle; onTranslate by a drag; onDragEnd by the release |
| Range | valueChanged by setValue; valueChanged by setMax below the value; changed by setStep; changed by setPage | - |
| ThumbnailGrid | selectionChanged by a click on an item | selectionChanged by setSelected; onItemClicked by a click on an item; onItemContextMenu by a right click on an item |
| AssetBrowser | onSelect by a click on an item; onOpen by a second click on the same item | - |
| NodeEditor | onNodeSelected by a click on a node | - |
| Chart | hoverChanged by moving onto a bar; pressed (the base signal) by a click on a bar; moved (the base signal) by moving over the chart | - |
| Menu | aboutToShow deleting the menu | - |
| TreeView | selectionChanged by setSelectedNode; selectionChanged by a click on a row; nodeExpanded by a click on the triangle; nodeCollapsed by a click on the triangle; nodeExpanded by the Right key; nodeCollapsed by the Left key; selectionChanged by the Down key | nodeActivated by a double click; selectionSetChanged by a ctrl click in a multi selection |
| PropertyGrid | propertyChanged by a click on a bool; onChange callback of a bool row taking the grid out; onClick callback of a button row taking the grid out | - |
| ColorPicker | colorChanged by a press on the value bar; colorChanged by a press on the alpha bar; colorChanged by a press on the wheel | colorCommitted by a drag |
| DataGrid | selectionChanged by a click on a row; columnSorted by a click on a header; rowCheckChanged by a click on a check box; rowCheckChanged by the Space key; cellEdited by typing into a cell and Return | selectionChanged by the Down key |
| TreeGrid | selectionChanged by a click on a row; selectionChanged by the Down key; columnSorted by a click on a header; nodeCollapsed by a click on the arrow; nodeExpanded by the Right key; cellEdited by typing into a cell and Return; nodeCollapsed by the Left key | - |
| Timeline | onTrackMuteChanged by setTrackMuted; onTrackSelected by setSelectedTrack; onPlayheadChanged by a click on the ruler; onPlayheadChanged by dragging along the ruler (the second signal); onClipSelected by a click on a clip; onClipSelected by a right click on a clip; onClipChanged by dragging a clip; onTrackLockChanged by a click on the lock button | - |
| CodeEditor | textChanged by typing; undoRedoChanged by typing; textChanged by undo; textChanged by Return | breakpointToggled by a click in the gutter |

## 3. O que NAO ficou seguro, ou nao se correu, e porque

Nao se afirma que cada sinal de cada widget esta seguro: so os que um teste correu (a tabela acima). O que fica, por familia, e porque:

- **CodeEditor**: nao se consegue com um `hasGone` a seguir a cada emissao: emite `textChanged`, `cursorMoved` e `undoRedoChanged` do meio de dezenas de
  sequencias (escrever, partir a linha, desfazer, indentar, comentar, completar, varios cursores) que continuam a ler membros. Em vez disso o
  `WidgetApp::DeletionHold` (secao 5): o editor segura o apagamento nos pontos de entrada `onTextInput`, `onKeyPress`, `onMousePress`, `undo` e `redo`. Fica
  seguro quanto a memoria (visto: escrever, Return, Ctrl+Z, `undoRedoChanged` e o clique na margem dos pontos de paragem) mas NAO para: o editor ainda
  pode emitir o que vinha a seguir (o `cursorMoved` do texto escrito) para um widget que ja saiu. Nao testado: `foldToggled` (pede um realcador de sintaxe;
  o sinal vem antes de `invalidateWrap()` e `markDirty()` em `foldAt`, `unfoldAt` e `toggleFoldAt`), e os comandos de edicao que a aplicacao chama directamente
  (`replaceRange`, a indentacao, ...) que nao seguram nada.
- **Timeline** (36 emissoes): so se mexeu no que um teste alcanca: `setTrackMuted`, `setSelectedTrack`, a regua (clique e arrasto), o botao de tranca, a escolha de um
  clipe (clique e botao direito) e o arrasto de um clipe so. O resto (as teclas, os keyframes, os envelopes, os redimensionamentos, o arrasto de um grupo de
  clipes, o Grab, o drop, a altura das pistas) tem o mesmo desenho (`emit(); markDirty();` ou um ciclo que continua a ler `dragGroup_`) e nao foi corrido: por mexer.
- **PropertyGrid**: so a linha bool (o callback da linha e o `propertyChanged`) e a linha botao. Por mexer, com o mesmo desenho: `spinChanged`, `spinStep`, `commitEdit`, o
  arrasto das linhas Range e Vec, a libertacao depois de um arrasto, e os popups do combo e da cor (`PropComboPopup_`, `ColorPickerPopup_`, que leem `grid_` depois do
  callback). O `ColorButton` so corre os callbacks do popup enquanto o botao existe (o WidgetApp larga o popup quando o dono sai: o comentario do codigo): nao se correu.
- **Dialogos** (`MessageBox::result`, `ChoiceDialog::chosen`, `InputBox::accepted` e `cancelled`, `FileDialog::accepted` e `cancelled`, os `setOnClose`): sao `FloatWindow`
  e o `WidgetApp::removeFloat` nunca apaga logo (poe na fila e o fim do ciclo apaga), por isso a janela que sai a meio do seu sinal nunca le memoria libertada (visto no
  `FloatWindow::minimizedChanged` e `closed`). Os sinais dos dialogos em si nao se correram (o FileDialog pede o sistema de ficheiros).
- **Lidos e nao corridos** (o sinal e a ultima coisa do metodo): `Gizmo3D` (cinco emissoes), `VideoCanvas` (`boxGrabbed`, `boxDragged`, `boxReleased`, `boxDoubleClicked`,
  `hoveredPixel`), `CurveEditor`, `GradientEditor` e `PlotWidget`, os sinais proprios do `SpinSlider` (`grabbed`, `ungrabbed`, `valueFocusEntered` e `valueFocusExited` ja
  perguntam `deletions()` a seguir), a `ConsoleWidget` (`onCommand`, `entryActivated`) e a `SymbolOutline` (`activated`).
- **Nao correm sobre uma arvore**: `WidgetApp::unhandledKey` e `fileDrop`, os sinais do `ItemModel` e as animacoes (`Animator`).
- **O buraco do `hasGone`**: compara ponteiros. Fora de uma frame, um tratador que tira o widget e cria outro que o alocador poe no mesmo endereco faz o `hasGone` dizer "nao saiu",
  e o widget continua a escrever num widget que e outro. Nao se viu; um contador de geracao por widget fechava-o.
- **Os widgets que chamam o tratador da base e continuam** (`Widget::onMouseMove`, `Widget::onMousePress`, que emitem `moved` e `pressed`): achados e corrigidos o Chart e a SideTabBar; o TreeView
  (`Widget::onMousePress` a abrir o seu) e o ThumbnailGrid (`Widget::onMouseMove` no fim) nao tiveram caso que fizesse vir o sinal.

## 4. `float-cast-overflow` e relatorios no `check.sh`

O grupo `undefined` do GCC nao leva o `float-cast-overflow` (um NaN ou um float grande demais passado a int): o estagio asan nunca o via (a ronda 6 achou
12 avisos a mao). E por defeito o `float-cast-overflow` RECUPERA no GCC (escreve o aviso e segue com saida 0), e o `ctest` sem `-V` nao mostra a saida
de um teste que passou: um aviso assim ficava invisivel. Duas coisas:

- o estagio asan compila com `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=undefined,float-cast-overflow`: o programa para
  no primeiro achado e o teste falha, com a linha e a pilha no log;
- a opcao de CMake `IGUI_FAIL_ON_SANITIZER_REPORT` (o `check.sh` liga-a no estagio asan) poe `FAIL_REGULAR_EXPRESSION` em todos os testes da pasta:
  `runtime error:`, `ERROR: AddressSanitizer`, `ERROR: LeakSanitizer` e `SUMMARY: ...Sanitizer`; um teste que sai com 0 mas tem um relatorio na saida falha.

Visto (programa de scratch, `<SP>/ig7/san`, um NaN passado a int): so com `undefined` passa em silencio (saida 0); com o `float-cast-overflow` e sem o
`-fno-sanitize-recover` escreve o aviso e sai com 0, e o `ctest` diz "Passed"; com a opcao ligada o `ctest` diz "Failed: Error regular expression found
in output"; com `-fno-sanitize-recover` sai com 1. A suite inteira (46 testes da ronda 6, 47 com o novo) nesta construcao: nada de novo apareceu: a ronda 6
ja tinha limpo os sitios que a suite alcanca, e os casos de agora (que nao passam NaN) tambem nao dispararam nada. Nao se corrigiu nada por isto.

## 5. Mudancas de comportamento

- O `markDirty()` (e `e.consumed`, e o `requestLayout()`) passa para ANTES do sinal em cerca de quarenta sitios: o estado ja mudou quando o tratador corre, e o tratador ve o widget
  ja marcado: nenhum efeito visivel.
- O `CheckBox` e o `ProgressBar` nao disparam o evento global `"toggle"` / `"change"` (`WidgetApp::on`) quando o tratador tirou o widget (antes disparavam-no, fora de uma frame com
  um ponteiro para memoria libertada, dentro de uma frame com um widget que ja nao esta na arvore).
- O `TabLayout::tabClosed` ainda sai antes de o separador se tirar; se o tratador tirou o proprio TabLayout nao ha separador a tirar.
- A `ComboBox::removeItem` do item escolhido emite `selectionChanged(-1)` DEPOIS de actualizar a largura do mais largo (antes, antes); o `ComboPopup_` guarda o dono, pergunta `hasGone` e so
  depois lhe fecha a lista (marca `e.consumed` antes).
- `AssetBrowser::onMousePress`: a decisao do duplo clique toma-se antes de emitir, e `onSelect` e `onOpen` recebem uma COPIA do item (o argumento era `items_[idx]`, do proprio navegador).
- `TextEdit` / `CodeEditor`: `TextEdit::notifyEdited()` (`textChanged` e depois `cursorMoved`, que nao vem se o primeiro tirou o editor); o `blinkTimer_` e o `e.consumed` do
  texto escrito passam para antes da insercao.
- `Range::setMin`, `setMax` e `setPage` perguntam `hasGone` depois do `setValue` e nao emitem `changed` se o `valueChanged` tirou o widget (`Range::valueSetGone`).
- `DataGrid::commitEdit` e `TreeGrid::commitEdit` limpam o estado da edicao ANTES de emitir `cellEdited`; o clique e a tecla Tab que as chamam e continuam perguntam `hasGone`.
- `Menu::exec`: `aboutToShow` pode apagar o menu (o destrutor poe uma flag que o `exec` le: um menu nao esta num pai, o dono apaga-o); nao ha `sizeHint()` nem `showPopup` depois.
- NOVO, `WidgetApp::emitGone(w, sinal, args...)`: o emit que diz se um tratador tirou o widget da arvore (`hasGone`). NOVO, `WidgetApp::DeletionHold`: o RAII que faz o que uma fase de
  frame ja faz (um widget tirado de uma arvore nao e apagado, espera pela ultima), para o CodeEditor.
- A `Range` ganhou um membro privado (`valueSetGone`), o `Menu` uma flag (`deletedFlag_`), o `TextEdit` um membro protegido (`notifyEdited`).
- Testes: `tests/StageHarness.hpp` e novo (o `Stage` da ronda 6 mudou para la; `Stage::key` passa a dar o foco ao widget tambem fora de uma frame, o que so afecta os testes).

## 6. Verificacoes

A corrida completa do `check.sh` foi feita na ponta 0a9f5b6; depois acrescentou-se so um caso de teste (564f067) e este ficheiro. Na ponta 564f067 correram o `nostd` (PASS) e o
estagio release (PASS, 47 de 47, 1 skipped); o asan e o hash NAO se repetiram nessa ponta (a corrida foi interrompida no fim do dia; o caso novo correu sob o ASan, a sos, e passou).
Dependencias: containers 34169bc e zen_plataform db76a13 (o iGUI), em
`<SP>/ig4/ct` e `<SP>/ig4/zp`; todos os comandos em scope de MemoryMax=4G, `-j3`, sem janela no ecra (Xvfb proprio, HOME e XDG em scratch).

**`tools/check.sh --base 43e1ae5 --jobs 3`** (correu como programa na ponta 0a9f5b6, `<SP>/ig7/chk`, saida em `<SP>/ig7/check_run1.out`; a de 564f067, parcial, em `check_final2.out`): `PASS nostd` (nada novo com `std::` nem `#include <...>` em src, include e adapters desde 43e1ae5), `PASS release` (47 de 47, 1 skipped), `PASS asan` (47 de 47: ASan,
UBSan e float-cast-overflow sem recuperar, e a opcao `IGUI_FAIL_ON_SANITIZER_REPORT`), `PASS hash` (os quatro valores de `bench/retained/draw.hash`, nenhum regravado: editor
87b1fe59ac36adb1 com 300 e com 3000 clipes, piano roll 7956122198be0670, graficos 957c5363d01cdd4a); "4 stages, 0 failed". O `igui_zen_lifecycle` saiu `Skipped` ("no window
here, skipped": o Xvfb da prova nao deu janela) no release; repetido tres vezes no mesmo build: `Skipped`, `Passed` e `Skipped` (visto: como as rondas 4b, 5 e 6 ja notaram, e
independente do codigo da biblioteca: era o Xvfb a fazer reset, seccao 9). O `igui_retained_round7` demora 148 s (a corrida de 0a9f5b6, 149 casos) sob o ASan (150 casos num processo; o ctest tem TIMEOUT 600).

**`std::`**: nenhuma contagem por ficheiro subiu (linhas com `std::` em 43e1ae5 e na ponta, ficheiros mexidos): so AudioWidgets.cpp desceu, 68 -> 67 (o `std::clamp` do LinearKnob passou a `clamp`);
os cabecalhos e os testes novos 0.

**Instrucoes por frame** (o contador por ptrace passo a passo da ronda 6, `<SP>/ig7/ic`, os mesmos cinco arneses, 43e1ae5 contra a ponta, CPU 4; nao depende do ruido):

| arnes | 43e1ae5 | ponta |
|-------|---------|-------|
| 200 ListBox de um item (`lb_ic`) | 695996 | 695996 |
| janela com as seis barras (`bars_ic`) | 328262 | 328262 |
| rolo de 400 batidas (`pr_ic`) | 824751 | 824751 |
| frame ociosa do editor, 300 clipes (`idle_ic`) | 973805 | 973805 |
| rolo de 32 batidas (`pr32_ic`) | 266955 | 266955 |

Iguais ao instrucao, o que se esperava: nada do que se mexeu esta no caminho de uma frame ociosa nem de um paint (so em caminhos de sinal). Os tempos nao se mediram (secao 8).

**A prova do antes**: `<SP>/ig7/before_cases.txt`, os 150 casos corridos um por processo, sob o ASan, contra a biblioteca da ronda 6 (`<SP>/ig7/base`: a worktree de 43e1ae5 com os ficheiros de teste de agora e o
CMakeLists de agora): 100 falham, todos com `heap-use-after-free`. Contra a ponta, os 150 passam (um processo por caso e tambem o programa todo num processo, "ok (150 cases)").

## 7. A zen_plataform (ramo `zen-ui-clip`, a partir da main beb1db0)

Repositorio da zen_plataform, ramo `zen-ui-clip` (a partir da main beb1db0; o ramo `mac-backend` nao se tocou; sem push). Dois commits:

- **9da0db6** o `15c4867` do ramo `igui-round6-zen-ui` (a lista: o `zui_listbox` corta o clip das linhas pelo do painel), aplicado em cima da beb1db0 sem conflitos (os mesmos hunks; o
  zen_ui nao mudou entre db76a13 e beb1db0). `test_listbox_stays_in_panel` visto falhar sem a correccao NESTA base (a main com so o teste: 4 verificacoes, 2 por modo:
  `outside == 0` e `g_list_selected == -1`) e passar com ela.
- **873fc45** o grupo e a janela. Reproduzido primeiro, com o teste novo (`tests/test_widgets.c`: `test_group_stays_in_panel`, `test_window_stays_in_panel`, nos dois modos,
  pixels e GL do backend falso): um grupo (30, 50, 300, 200) dentro de um painel (20, 20, 200, 120), e uma janela (60, 60, 300, 200) dentro de um painel (20, 20, 200, 160). O que se viu
  ANTES (14 verificacoes a falhar, 7 por modo): os textos do grupo e da janela levam um clip que passa do painel; um clique no botao do grupo fora do painel (x 260, ou y 175, ou na parte
  de baixo de uma linha que o painel corta) clica; um clique no botao da janela fora do painel clica; e o arrasto da barra de titulo da janela a partir de fora do painel leva a janela.
  Correccao, a mesma do listbox: `zui_group_begin` e `zui_window_begin` guardam o clip que estava e poem o seu cortado por ele (`ui_rect_within`); e, porque o `ui_hit` e o `ui_hovered`
  de um widget so olhavam para o rectangulo do widget e nunca para o clip (o listbox tinha o seu proprio teste), o `ui_hovered` passa a pedir tambem que o rato esteja dentro do
  clip em vigor: o que o painel esconde deixa de apanhar o rato (por leitura do codigo, e de esperar, isto tambem muda um caso sem aninhar: um widget que o rolo de um grupo tire para fora do grupo passa a ficar sem rato; nao se provou com um teste). DEPOIS: 333 verificacoes passam, 0 falham.

O que se viu, por onde passou:
- `tools/check.sh release asan` (o do repositorio: Xvfb invisivel por `tools/xvfb_run.sh`, `ZP_BUILD_DIR` e `ZP_JOBS=3` em scratch, em scope de 4G): release 34 testes PASS, asan 34 testes PASS
  (`<SP>/ig7/zpchk.out`, `zpchk/release.test.log`, `zpchk/asan.test.log`); `test_widgets` com as 333 verificacoes, `test_ui` 92, `test_draw2d` 59.
- Imagens, num Xvfb privado (exemplo de scratch `<SP>/ig7/zpdbg/zui_clip_shot.c`, uma imagem de 800 por 360, compilado contra a main e contra a ponta do ramo): as quatro linhas do
  grupo e os quatro botoes e texto da janela desenhavam-se fora dos paineis, depois ficam cortados nos bordos dos paineis (35840 pixeis diferentes):
  `<SP>/ig7/shots/clip_before.png` e `clip_after.png` (vistas).
- Os exemplos `demo_widgets_pixel` e `demo_dock_pixel` (Release, ecra de 1280 por 800, 3 s, Xvfb privado, main contra ponta): **0 pixeis diferentes** nos dois
  (`demo_widgets_pixel_before.png`, `_after.png`, `demo_dock_pixel_before.png`, `_after.png` em `<SP>/ig7/shots`; vi as duas `_after`: tem conteudo e janela). Nenhum pixel mudou onde a correccao nao era para mudar.

Todos os sitios da zen_ui que poem um clip (os de `src/ui/ui_widgets.c` e `ui_dialog.c`; `ui_core.c` poe o do ecra no inicio da frame e `ui_layout.c` guarda e repoe o clip de cada container):

| Sitio | Corta pelo clip em vigor? | |
|-------|---------------------------|--|
| `zui_listbox` | sim (commit 9da0db6) | corrigido |
| `zui_group_begin` | sim (873fc45) | corrigido |
| `zui_window_begin` | sim (873fc45) | corrigido |
| o popup do `zui_combo` (`ui->clip = popup`) | nao | de proposito: o popup flutua por cima de tudo (o comentario do codigo: escapa o clip da janela) |
| o popup de um menu (`zui_menu_begin`) e o do menu de contexto | nao | o mesmo: popups |
| `zui_statusbar_begin` (`ui->clip = bar`) | nao | a barra e do ecra todo (largura do ecra, em baixo), nao e para ir dentro de um painel; se se pusesse dentro de um, o conteudo saia (mesmo defeito); sem teste nao se mexeu |
| `zui_dialog` (`ui_dialog.c`: o do ecra todo, `panel`, a barra de ferramentas, o caminho, as caixas) | nao | o dialogo e um modal do ecra todo; os clips de dentro sao sub-rectangulos do painel do dialogo por construcao; guarda e repoe o clip que estava (`saved_clip`) |

O que muda para quem usa a zen_ui: um widget deixa de poder apanhar o rato fora do clip em que e desenhado (antes so o listbox o fazia). Nao se correu nenhuma aplicacao (ZenVideo, ZenMix, ...)
contra isto, nem o backend GL com janela real (os testes correm o modo GL do backend falso).

## 8. O que ficou de fora, e o que desconfio

- (Corrigido na seccao 9: bancada e aplicacoes correram-se depois.) A bancada (`igui_retained_bench`) nao se correu para tempos: a maquina esteve com carga 12 a 26 (outros agentes) durante toda a ronda e os tempos de uma frame ociosa
  varrem 10 a 100% entre corridas; as instrucoes por frame (o que nao depende do ruido) sao iguais nos cinco arneses da ronda 6, e nenhum caminho de frame ou de paint foi
  tocado (so caminhos de sinal). O que se acrescenta a um sinal e a chamada `WidgetApp::instance()` e a comparacao do contador (em `emitGone` e nos `hasGone`), um inteiro a
  subir e a descer por tecla ou clique no CodeEditor.
- Nao se correu o ZenVideo, o ZenMix, o ZenDAW nem as outras aplicacoes contra esta ponta (o ZenVideo correu-se depois: seccao 9; as outras nao).
- (A causa e o arranjo estao na seccao 9.) O `igui_zen_lifecycle` saiu `Skipped` na primeira passagem do release (o Xvfb de prova nao deu janela), como ja se disse na ronda 4b, 5 e 6; ver a secao 6.
- O Timeline, o PropertyGrid e o CodeEditor ficaram a meio (secao 3): quem precisar de tirar um deles de um sinal que nao esta na lista usa `WidgetApp::callDeferred`.
- Os testes do modo 0 ("dentro de uma frame") so vem o que o despachante deixa vir: um gesto que nao o alcanca (o Gizmo3D, os popups do PropertyGrid, os da cor, a Console, os
  dialogos) ficou por correr.

## 9. Verificacao depois do 745fcc6 (o que se correu e se viu; commits eba3ba5 e seguintes so mexem em testes e no script do Xvfb)

Feita por outro agente sobre o ramo como estava (745fcc6), sem tocar no codigo da biblioteca (a contagem de `std::` nao mudou: nenhum ficheiro de `src`, `include`
ou `adapters` foi tocado). Tudo em scope de 4G, `-j3`, Xvfb proprio, HOME e XDG em scratch. Ficheiros em `<SP>/ig8` (scratchpad da sessao).

**`tools/check.sh --base 43e1ae5 --jobs 3` na ponta 745fcc6** (`<SP>/ig8/check_tip.out`, logs em `<SP>/ig8/chk_tip`, 10 minutos): `PASS nostd`, `PASS release` (47 de 47, 1 skipped:
o `igui_zen_lifecycle`), `PASS asan` (47 de 47, 1 skipped: o mesmo), `PASS hash` (os quatro valores de `draw.hash`, iguais); "4 stages, 0 failed". Na ponta eba3ba5 (depois do arranjo do lifecycle), a mesma corrida (`<SP>/ig8/check_final.out`, 6,5 minutos, CPUs 0 a 5): `PASS nostd`, `PASS release` (47 de 47, **0 skipped**), `PASS asan` (47 de 47, 0 skipped),
`PASS hash` (os mesmos quatro valores); "4 stages, 0 failed"; o `igui_zen_lifecycle` passou nos dois estagios (2,05 s e 6,22 s) e 15 vezes seguidas pelo ctest (`--repeat until-fail:15`).

**O `igui_zen_lifecycle` Skipped: a causa e o arranjo (eba3ba5).** Visto com o gdb (ponto de paragem no `error_set` da zen_plataform): "cannot open the X display" no `platform_init` da 2a a
9a criacao da aplicacao. O Xvfb faz RESET quando o ultimo cliente se desliga e nesse instante recusa ligacoes; o teste cria e destroi a aplicacao 8 vezes (o `platform_shutdown` fecha
a ligacao) e a criacao seguinte apanha a janela do reset: o teste dizia "no window here, skipped" (77). Nao era do codigo. Medido: o binario de antes com o `xvfb-test.sh` de antes, 20
corridas: 11 passam e 9 saem com 77; com `-noreset` no Xvfb (`tools/xvfb-test.sh`), 20 de 20 passam. O teste so deixa a PRIMEIRA criacao dizer "no window here"; uma que falha depois
escreve FAIL e sai com 1 (o binario novo com o script velho: 3 passam e 7 falham, com "create number N failed", em vez de skip). Os outros testes de janela (`igui_zen_input`,
`igui_zen_sidetabs`) usam o mesmo script e nao se saltaram nas corridas vistas (por leitura: so este destroi e recria a aplicacao dentro do mesmo processo).

**Instrucoes por frame, as cinco do `ic` da ronda 7**: 695996, 328262, 824751, 973805 e 266955, iguais nas duas arvores (43e1ae5 e esta), outra vez.

**Instrucoes dos caminhos de sinal** (o `markDirty` passou para antes do sinal em cerca de quarenta sitios, e os `hasGone`/`emitGone` custam uma leitura do contador): contador por ptrace,
um evento ou uma chamada por regiao (`<SP>/ig8/ic/sig_ic.cpp`: 15 cenarios, 122 gestos: clique e tecla do CheckBox, TextEdit e TextInput, barra de rolagem, Timeline (clique, arrasto, regua,
passagem do rato), ComboBox, TreeView, DataGrid, TreeGrid, Knob e Fader, SpinSlider (`setValue`, `setMax`, `setPage`), CodeEditor, Chart, ToolBar, Splitter e TabLayout), 43e1ae5 contra a ponta,
CPU 4 (nao depende do ruido). Iguais em 62 dos 122; nos outros 60 a diferenca vai de -23 a +82 instrucoes (`<SP>/ig8/ic/sig_run2.txt`, tabela com `sigtab.py`):

| caminho | 43e1ae5 | ponta | |
|---------|---------|-------|--|
| CodeEditor, uma tecla escrita | 4449 / 24471 / 6864 | +82 | `DeletionHold` (o contador sobe e desce) e o `hasGone` a seguir |
| TextEdit, uma tecla escrita (`textChanged`, `cursorMoved`) | 25489 | +57 | `notifyEdited` e o `hasGone` a seguir a insercao |
| TextEdit, Return | 3924 | +58 | |
| TreeGrid, uma tecla (Left, Right, Down), um clique no cabecalho | 1469 a 2101 | +35 | o `hasGone` no fim do `onKeyPress` |
| Chart, rato sobre uma barra | 3075 | +27 | |
| ToolBar, botao; Splitter, premir | 2392; 1988 | +26; +28 | |
| ComboBox, premir na lista (a escolha) | 1475 | +27 | |
| TreeView, premir uma linha | 2466 | +23 | |
| CheckBox, largar (`toggled` e o evento "toggle") | 2144 | +7 | |
| DataGrid, premir uma linha | 2888 | -23 | o `markDirty` antes do sinal deixa o compilador juntar caudas |
| setters (`setSelectedIndex`, `setTrackMuted`, `Knob::setValue`, `SpinSlider::setStep`): 62 a 79 instrucoes | | -4 | |
| `TabLayout::setCurrentIndex`: 89 | | +5 | |

O maior em percentagem num caminho com mais de cem instrucoes: +2,4% (TreeGrid, 35 de 1469); nos caminhos de 60 a 90 instrucoes, -4 a +5 instrucoes (-6,5% a +5,6%: quatro ou cinco instrucoes).
Nenhum caminho de clique, arrasto ou escrita ficou mais lento de forma que se note: o maior custo absoluto e 82 instrucoes por tecla no CodeEditor (de 4449 a 24471).

**Bancada** (`igui_retained_bench`, 140 linhas, o mesmo `bench/retained/RetainedBench.cpp` dos dois lados, Release, `-ffile-prefix-map`, os dois binarios dao os mesmos quatro hashes; fixo ao CPU 10,
corridas alternadas): 43e1ae5 reconstruida (`<SP>/ig6/bench/bin/b43`, a antiga `tip` era de antes do f7dc0ac) contra a ponta (`bin/r7`).
- Passagem completa, 5 pares alternados (`runs/r7p_*`), com a maquina a carga 14 a 23 (o `deform` de outro agente ocupava 5 CPUs): **123 das 140 linhas ficam a mais de 3%, nos dois sentidos**
  (de -41% a +55%) e a variacao entre corridas do MESMO binario e de 20 a 150%: e ruido, nao codigo (as linhas do Chart, dos grids e do PianoRoll, cujo caminho de frame a ronda 7 nao mexeu, andam -15% a -40%
  nesta passagem e as dos widgets, +10% a +50%, e o sentido muda de uma passagem para a outra). Nao se le.
- Corridas focadas, 8 pares alternados com segundos de intervalo (o `fbench.sh` da ronda 6), com a maquina quase quieta (carga 3 a 6; `runs/f7_*`): `input/` (15 linhas: a frame ociosa de uma janela com um
  TextEdit, uma ListBox de 1000 itens e uma DataGrid de 1000 linhas, o rato a passar por oito widgets, teclas e escrita): no MINIMO a ponta sai **+0,2% a +0,7%** (+0,2 us de 36,5 a 80) em TODAS as linhas, ate nas do
  rato sobre um label; o MESMO binario contra uma copia de si proprio, no mesmo metodo, da -0,4% a 0,0% (o chao do ruido); as medianas andam 46 a 60% (ruido da maquina: so o minimo se le).
  `chart/line_100k_hover` -0,1% (6 pares), `widget/checkbox_per_widget` 0,0%. As instrucoes da MESMA cena (`<SP>/ig8/ic/input_ic.cpp`: 37 regioes, as frames ociosas, os oito passeios do rato, as teclas) sao IGUAIS ao
  instrucao em todas menos a escrita de um caractere no TextEdit (+57 de 448744) e o Backspace (+24 de 447517). O +0,5% de `input/` e por isso, com as instrucoes iguais, colocacao do codigo (a ronda 5 e a 6 ja viram
  o mesmo: +-9% por 400 bytes de NOP num objecto sem relacao, e a construcao alinhada fazia-o desaparecer). A alinhada NAO se correu desta vez: e uma conclusao por exclusao (instrucoes iguais, chao do ruido
  mais pequeno), nao uma prova.

**Revisao das mudancas de comportamento (item 2 do pedido): nenhuma aplicacao dos donos depende da ordem antiga.** Procurou-se (so leitura) em ZenVideo, ZenMix, ZenDAW, ZenHuman, ZenStage, coco, deform, animator,
zenforms, MapShape, mcpchat, ZenMusic e ZenVoice (1346 ficheiros `.cpp`/`.hpp`/`.h`, sem `build*`, `third_party` e `external`):
- Tratadores dos sinais que mudaram: ZenVideo `src/Editor.cpp` 147, 221, 290, 342, 357, 386, 512, 539, 558 a 650 (a Timeline: `onPlayheadChanged`, `onTrackMuteChanged`, `onClipContextMenu`, `onClipSelected`,
  `onEnvelopeChanged`, `onTrackSelected`, `onClipChanged`), `EditorOverlay.cpp` 144, `EditorSettings.cpp` 222 a 379; ZenMix `Editor.cpp` 202, 302 a 428, 570 a 634, `EditorFxPanel.cpp` 138 a 210,
  `EditorProperties.cpp` 158 a 259, `AboutPanel.cpp` 298, `SongList.cpp` 31; ZenDAW `ui/App.cpp` 235 a 329, `PatternsPage.cpp` 229 a 231. Lidos: nenhum le o estado que a mudanca mexe (o `markDirty`, o
  `e.consumed` e o `requestLayout` a antes do sinal nao se veem de um tratador; os que mudam a arvore usam `later`/`callDeferred`).
- Eventos globais "toggle" e "change" que o CheckBox e o ProgressBar deixam de disparar quando o tratador os tirou: `fireEvent` e `WidgetApp::on("...")` nao existem em nenhuma aplicacao (so o `.on(` do
  script do zenforms, que e dele: `zenforms/runtime/src/Script.cpp` 589, e liga a sinais proprios).
- `AssetBrowser` (`onSelect`/`onOpen` com uma copia do item): nenhuma aplicacao o usa.
- `DataGrid`/`TreeGrid::commitEdit` (estado da edicao limpo antes de `cellEdited`): so o zenforms, `runtime/src/Reflect.cpp` 845 (`connectGridEdited`: o tratador nao recebe nem le a celula) e 861.
- `TreeGrid`/`TreeView` `nodeExpanded`/`nodeCollapsed` (a lista plana refeita ANTES do sinal no TreeGrid e `flatDirty_` antes no TreeView): nenhuma aplicacao liga a eles; o `Node::setExpanded` do
  TreeGrid marca `flatDirty_`, por isso um tratador que acrescente filhos continua a ser visto.
- `TabLayout::tabClosed` (o `e.consumed` antes do sinal): `zenforms/designer/Designer.cpp` 274 (`TabBar`, que e um TabLayout): o tratador chama `closeDocument`, que faz `tabs_->removeTab(index)` e o
  TabLayout faz `removeTab(i)` outra vez depois do sinal; e a ordem de sempre (emite, depois tira) e nao mudou; o separador tirado duas vezes parece um defeito de sempre do zenforms (lido, nao corrido), que carrega uma
  iGUI de 4 de Novembro. Nao se mexeu.
- As aplicacoes `animator`, `coco`, `deform`, `ZenHuman`, `ZenStage`, `zenforms` carregam copias antigas da iGUI (de Outubro a 4 de Novembro): so notam isto quando as actualizarem; ZenVideo (175a023), ZenMix (73946ea) e ZenDAW (2e023fd) tambem carregam copias anteriores a esta ponta.

**As aplicacoes (item 3), ZenVideo e uma de zen_ui, imagens lidas.**
- ZenVideo (copias em `<SP>/ig8/zv/{main,new}`; `main` = iGUI 43e1ae5 + zen_plataform beb1db0, `new` = esta ponta + zen-ui-clip; Release, o LTO dele, `-j3`; o `main.cpp` das copias ganhou um condutor de
  cena, `ZV_SCRIPT`, que mexe o ponteiro pelo `ZenApp::setInputHook`; a pasta do dono nao foi tocada): 14 cenas num Xvfb proprio, `ZENVIDEO_NULL_AUDIO=1`: o arranque a 1440x860 e a 1000x600, com a pasta de
  32 clipes (a lista de media com barra, a timeline cheia) a 1440x860 e a 1000x600, o menu File aberto, o menu do botao direito de um clipe, o combo da biblioteca aberto, a lista de media rolada, o
  FileDialog de "Import files...", o de "Import folder...", "Open project..." (so mostra o aviso de projecto por guardar, nao abre janela), um clipe arrastado na timeline (a meio e solto), um clique num clipe: **0 pixeis diferentes
  nas 14** (`<SP>/ig8/shots/zv/main_*.ppm` e `new_*.ppm`; vistas: `new_montage.png` (combo, lista rolada, timeline a meio de um arrasto), `new_dialog2.png` (o FileDialog), `t_probe.png` (os dois menus)). O
  ZenVideo nao usa a zen_ui (`zui_`): so compila.
- Seis cenas de widgets desenhadas em processo pelo rasterizador por software de cada arvore (`<SP>/ig8/sc/scenes.cpp`: um TextEdit com as duas barras e uma TreeView com a sua, um combo aberto, um menu
  de contexto com submenu, uma ListBox e uma DataGrid rolada, um InputBox e um MessageBox, uma Timeline com um clipe a meio de um arrasto): **0 pixeis diferentes** entre 43e1ae5 e a ponta (`<SP>/ig8/sc/out`,
  vistas em `r7_montage.png`).
- zen_ui: `demo_dock_pixel` (grupos, lista com barra, combo, barra de menus, barra de estado) e `demo_widgets_pixel` (janelas, lista, combo, barra de menus, dialogo), guiados por XTest (cliques e roda por um
  Xvfb proprio, `<SP>/ig8/xdrive.py`), main contra a ponta do zen-ui-clip: o dock, 9 cenas (cursor numa linha, clique, menu File aberto e fechado, combo aberto, roda, menu Window): 0 pixeis diferentes. O
  widgets: 5 cenas iguais (lista rolada, combo aberto, janela arrastada) e **uma com 14445 pixeis diferentes: o dialogo de ficheiros (Ficheiro, Abrir) tinha a lista de ficheiros VAZIA na ponta 873fc45** (`<SP>/ig8/shots/zp/B5_main_over_tip.png`,
  vista). Era uma regressao do ramo zen-ui-clip: o dialogo poe o clip da barra lateral e o do cabecalho por atribuicao (`ui->clip = sb`, `= hr`) e deixa o ultimo em vigor; o `zui_group_begin` passou a cortar
  o seu clip pelo que esta em vigor e o corte dava um rectangulo vazio. O `test_dialog_flow` nao a via (os comandos de texto estavam todos na lista de desenho, com o clip vazio). Arranjado na zen_plataform
  (4309d99: `ui->clip = panel` antes do grupo `dlg_files`; teste novo `text_visible` nas tres vistas, 4 verificacoes que falham antes e passam depois). Com o arranjo o dialogo mostra os ficheiros e so difere da
  main no fundo e no contorno do grupo da lista (`<SP>/ig8/shots/zp/D_icons_list_main_tip.png`: o grupo tem `ZUI_GROUP_BORDER` e na main o contorno e o fundo pintavam-se com o clip velho e nao se viam).

**O que ficou por fazer (pedido do dono: so a verificacao).** Os itens 4 e 5 do pedido nao se fizeram: os sinais que a seccao 3 diz por mexer (Timeline: keyframe, envelope, redimensionar, grupo, Grab; PropertyGrid: `spinChanged`,
`spinStep`, `commitEdit`, os popups; Console, SymbolOutline, FileDialog e dialogos, Gizmo3D, VideoCanvas, CurveEditor, GradientEditor, SpinSlider) continuam por mexer, o buraco do `hasGone` (um widget novo no mesmo endereco)
tambem, e o `zui_statusbar_begin` e o teste do widget rolado para fora de um grupo (zen_plataform, `docs/SUSPEITAS.md`).

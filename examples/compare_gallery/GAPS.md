# Levantamento de paridade: iGUI vs demo do ImGui

O que o demo do ImGui (`imgui_demo.cpp`, o `ShowDemoWindow()`) tem e o **modo
imediato do iGUI não exprime**. Cada linha desta lista corresponde a um
`gallery::reportGap()` em [GalleryIgui.cpp](./GalleryIgui.cpp), que é a fonte de
verdade: o espelho regista a lacuna no sítio exato onde ela aparece.

Para tirar a lista atualizada do binário:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIGUI_BUILD_COMPARE_GALLERY=ON \
      -DIGUI_BUILD_SDL2_DEMO=ON -DIGUI_BUILD_OPENGL_BACKEND=ON -DIGUI_BUILD_FONT=ON \
      -DIGUI_BENCH_IMGUI_DIR=/caminho/para/imgui
cmake --build build --target igui_compare_gallery
./build/examples/compare_gallery/igui_compare_gallery --hidden --frames=120 --open-all
```

O `--hidden` corre sem janela visível; o `--open-all` abre todas as secções e
sub-secções (sem ele só se vê o que estiver aberto, e as lacunas das secções
fechadas não são registadas). O resumo no fim imprime as lacunas e a geometria dos
dois lados: na revisão atual são **40 lacunas** contra 222 itens submetidos pelo
espelho.

Para uma secção de cada vez - é assim que se põe lado a lado o mesmo ecrã, com a
secção aberta à mão do lado do ImGui - o `--open=NOME` abre só essa
(`--open=Combo`, `--open="Tables & Columns"`), e o `--click=X,Y` carrega uma vez no
painel do iGUI nesse ponto, para os popups (que só abrem com rato) aparecerem numa
captura: `--open=Combo --click=150,405 --frames=14 --screenshot=combo.png`.

A lista impressa cobre **as secções que foram desenhadas** naquele frame: a
lacuna é registada no sítio do código onde ela aparece, e uma secção fechada não
corre esse código. Com tudo aberto (como o demo abre) a lista fica completa - e
este documento é o levantamento exaustivo, independente do que está aberto.

## Como ler a tabela

- **existe** — há um widget do iGUI que faz o mesmo (por vezes com outro nome).
- **parcial** — há algo que serve, mas falta a parte indicada.
- **não existe** — não há equivalente; no espelho fica uma nota `[iGUI: nao existe]`
  e a lacuna entra no relatório.

## Texto

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `Text` / `TextUnformatted` | `label` | existe |
| `"texto##id"` / `"##id"` num rótulo | `visibleLabel` (id com a string toda, desenha só o que vem antes do `##`) | existe; um `label("a##b")` desenha `a##b` (o `Text` do ImGui também não trata de `##`) |
| `TextColored` / `TextV` | `textColored` (item próprio, com o layout a avançar) | existe |
| `LabelText` | `label` + `sameLine` + `label` | parcial: não é um item próprio |
| `TextDisabled` | `textDisabled` (usa `theme.textDisabled`) | existe |
| `TextWrapped` / `PushTextWrapPos` | `textWrapped` (largura opcional) | parcial: sem stack de wrap positions |
| `Bullet` / `BulletText` | `bulletText` (com wrap, o segundo alinha sob o primeiro) | existe |
| `SeparatorText` | `separatorText` | existe |
| `SetTooltip` | `tooltip` | existe |

## Botões e toggles

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `Button` / `SmallButton` / `InvisibleButton` | `button` / `smallButton` / `invisibleButton` | existe |
| `ArrowButton` | — | **não existe** |
| `Checkbox` | `checkbox` | existe |
| `CheckboxFlags` (bitfield int/unsigned) | — | **não existe** (faz-se com N checkboxes) |
| `RadioButton` | `radioButton` | existe |
| `ImageButton` / `SmallImageButton` | `imageButton` / `smallImageButton` | existe |
| `Selectable` com flags | `selectable(label, selected, width)` | parcial: sem `SpanAllColumns`, `AllowDoubleClick`, `Disabled`, `AllowOverlap`, alinhamento à direita |

## Sliders, drags e inputs numéricos

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `SliderFloat` / `SliderInt` | `sliderFloat` / `sliderInt` | existe |
| `VSliderFloat` / `VSliderInt` | `vSliderFloat` / `vSliderInt` | existe (o `fader` é outra coisa: um controle de mistura com cap rígido) |
| `SliderAngle`, `SliderScalar(N)` | — | **não existe** |
| `SliderFloat2/3/4`, `SliderInt2/3/4` | 1 slider por componente | **não existe** |
| `DragFloat` / `DragInt` | `dragFloat` / `dragInt` | existe |
| `DragFloat2/3/4` | `dragFloat2/3/4` | existe |
| `DragInt2/3/4`, `DragScalar(N)` | — | **não existe** |
| `DragFloatRange2` / `DragIntRange2` | 2 drags | **não existe** (min+max numa só widget) |
| Flags de drag/slider (`Clamp`, `Wrap`, `Logarithmic`, `AlwaysClamp`, `NoInput`, `NoRoundToFormat`, `NoSpeedTweak`) | — | **não existe**: os ranges são sempre fixos |
| `InputFloat` / `InputFloat2/3/4` | `inputFloat` / `inputFloat2/3/4` | existe |
| `InputInt` | `inputInt` | existe |
| `InputInt2/3/4` | — | **não existe** |
| `InputDouble` | — | **não existe** |
| `InputScalar(N)` | — | **não existe** |
| `SliderFloat` com formato próprio (`%.3f`) | `precision` | parcial |
| — | `stepperInt` | (extra do iGUI) |

## Texto editável

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `InputText` | `inputText` | existe |
| `InputTextMultiline` | `inputTextMultiline` | existe |
| `InputTextWithHint` | `inputTextWithHint` (a dica sai na cor do texto desativado enquanto o campo está vazio) | existe |
| `CallbackCharFilter`, `CallbackCompletion`, `CallbackHistory`, `CallbackResize` | — | **não existe** |
| `ImGuiInputTextFlags_*` (CharsDecimal, CharsHexadecimal, ReadOnly, Password, AllowTabInput, EnterReturnsTrue, NoUndoRedo, ...) | — | **não existe**: sem flags |
| `InputText` com `Hint` e seleção por teclado | parcial | o caret/seleção existem; a navegação por teclado completa não |

## Combos, listas e seleção

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `Combo` (lista de strings) | `comboBox` | existe |
| `BeginCombo`/`EndCombo` (conteúdo próprio: árvore, imagens, multi-seleção) | `beginCombo`/`endCombo` | existe: o popup leva o que o chamador submeter entre os dois (o cursor do layout fica dentro dele e `availableWidth()` passa a ser a largura do popup). A largura é a do cabeçalho e cresce com o conteúdo; a altura é a do conteúdo até `visibleItems` linhas (8 por omissão, como o `HeightRegular` do ImGui) |
| `Combo` com filtro de pesquisa | `beginCombo` + `inputText` | existe: o filtro é conteúdo do popup, submetido pelo chamador (a galeria faz isso) |
| `ImGuiComboFlags_*` (NoArrowButton, NoPreview, WidthFitPreview, HeightSmall/Regular/Largest) | — | **não existe**: sem flags |
| Scroll dentro do popup de um combo (o ImGui mostra 8 linhas e rola) | — | **não existe**: o que passa de `visibleItems` linhas é cortado, e um `beginChild` lá dentro também não funciona (o `beginLayout` fecha o popup) |
| `ListBox` (uma entrada) | `listBox` | existe |
| `ListBox` com seleção múltipla | — | **não existe** |
| `BeginMultiSelect`/`EndMultiSelect` (clique, Ctrl, Shift, Ctrl+A, Escape) | `beginMultiSelect`/`endMultiSelect` | existe: a seleção é da aplicação (um `Span<bool>`, um por item) e o bloco só lhe aplica o que o rato e as teclas pediram. Dentro do bloco cada `selectable` é um item, pela ordem em que é submetido (o `MultiSelectFlags` tem `SingleSelect`, `NoSelectAll`, `NoRangeSelect` e `ClearOnEscape`) |
| `ImGuiMultiSelectIO` (os pedidos `SetAll`/`SetRange`/`BeginClear`/...) e `SetNextItemSelectionUserData` (ids que não são índices) | — | **não existe**: sem IO de pedidos e sem ids de item |
| `ImGuiSelectionBasicStorage` / `ImGuiSelectionExternalStorage` | — | **não existe**: quem guarda a seleção é a aplicação |
| `IsItemToggledSelection` / `IsItemSelected` | `isItemSelected`/`isItemToggledSelection` | existe (do último item submetido no bloco) |
| Os ~12 exemplos do demo (clipper, déleção, dual list box, tabela, checkboxes, múltiplos escopos, tiled assets browser, árvores, avançado) e o box-select (arrastar) | — | **não existe**: sem clipper, sem auto-select na navegação, sem box select |

## Árvores, headers, tabs

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `TreeNode` | `treeNode` | existe |
| `TreeNodeEx` com flags (`Selected`, `DefaultOpen`, `OpenOnArrow`, `SpanAvailWidth`, `SpanFullWidth`, `Framed`, `Leaf`, `Bullet`, `NavLeftJumpsToParent`) | `treeItem` com `TreeItemStyle` (selected, leaf, typeColor) | parcial: sem framed, bullet nem span |
| `TreePush`/`TreePop`, `Indent`/`Unindent` | `indent`/`unindent` | existe |
| `CollapsingHeader` | `collapsingHeader` | existe |
| `CollapsingHeader` com botão de fecho (`bool* p_open`) | `collapsingHeader(label, expanded, open)` | existe (o x fecha-o e o header deixa de ser desenhado) |
| `CollapsingHeader` com bullet / closable group | — | **não existe** |
| `BeginTabBar`/`BeginTabItem` | `tabBar` | parcial: o `tabBar` é uma faixa com índice, não abre e fecha conteúdo com id próprio |
| Flags de tab (`Fitting`, `AutoSelectNewTabs`, `TabListPopup`, `TabListButton`, `NoCloseWithMiddleMouseButton`, `NoReorder`) | — | **não existe** |
| Tab com botão de fechar / `ImGuiTabItemFlags_SetSelected`, `UnsavedDocument` | — | **não existe** |

## Tabelas

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `BeginTable`/`TableNextColumn`/`TableHeadersRow` | `beginTable`/`tableNextColumn`/`tableHeader` | existe (o `tableHeader` faz o sort) |
| `TableSetupColumn` com pesos e resize | `beginTable(id, weights)` | parcial: os pesos existem, falta o resto |
| `ImGuiTableFlags_*`: Resizable, Reorderable, Hideable, Sortable, Borders, RowBg, ScrollX/Y, SizingFixedFit, FreezeRows/Cols, ContextMenuInBody, NoPad | — | **não existe** |
| Tabelas aninhadas, tabelas com header agrupado | — | **não existe** |
| — | `beginVirtualTable` | (extra do iGUI) |

## Plots

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `PlotLines` (overlay, escala, cores do tema) | `plotLines` | existe (decimado a uma coluna por pixel, desenhado como uma só fita, destaca a amostra sob o ponteiro, devolve o índice) |
| `PlotHistogram` | `plotHistogram` | existe (barras a partir da linha do zero, que sobe para o topo com escalas negativas, desenhadas numa só chamada) |
| As variantes do demo (dados animados, `PlotEx` interna) | — | parcial: os dados são sempre `const float *` + contagem (sem *getter* nem *stride*), e a escala automática é tudo-ou-nada |
| — | `gradientEditor`, `curveEditor`, `timeline`, `sequencer` | (extras do iGUI, noutro domínio) |

## Cores

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `ColorEdit3` | `colorEdit` (sempre RGBA) | parcial: só 4 componentes |
| `ColorEdit4` | `colorEdit` | existe |
| `ColorPicker4` | `colorEdit` com altura grande | parcial |
| `ColorButton` | `colorButton` (com o rótulo ao lado, como o `colorEdit`) | existe |
| Flags de color edit (`AlphaBar`, `AlphaPreview`, `InputRGB/HSV/HEX`, `Sub`, `NoInputs`, `NoLabel`, `PickerHueWheel`/`Bar`/`Triangle`/`Square`) | — | **não existe** |
| `SetColorEditOptions` | — | **não existe** |
| Palette widgets | — | **não existe** |

## Barras de progresso

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `ProgressBar` (sem texto) | `progressBar` | existe |
| `ProgressBar` com overlay de texto | `progressBar(value, maximum, bounds, overlay)` | existe (o overlay sai centrado por cima da barra) |
| Barra indeterminada (`fraction < 0`) | `progressBar` com valor negativo | existe (uma faixa de 20% atravessa-a com o tempo do contexto) |

## Tooltips

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `SetTooltip` (texto) | `tooltip(text)` | existe |
| `BeginTooltip`/`EndTooltip` com widgets dentro | `beginTooltip`/`endTooltip` | existe (a caixa mede-se pelo conteúdo; nada lá dentro reage ao rato, para não roubar o hover a quem a pediu) |
| `BeginItemTooltip`/`SetItemTooltip` | `tooltip`/`beginTooltip` logo depois do widget | parcial: as duas funcionam sobre o último item, sem um pedido explícito para um id |
| Atraso configurável por item, flags de hover | `setTooltipDelay` (global) | parcial |

## Popups e modais

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `OpenPopup`/`BeginPopup`/`EndPopup` (pilha de popups, ids, posição, tamanho) | `beginDialog(titulo, open, size)` | parcial: o estado é da aplicação, não há pilha nem `OpenPopup` |
| `BeginPopupModal`/`CloseCurrentPopup` | `messageBox` | parcial: é uma message box, não um modal com conteúdo livre (a `beginDialog` cobre o caso simples) |
| `BeginPopupContextItem`/`Window`/`Void` | `beginContextMenu(id, rect)` | parcial: precisa do rect à mão |
| Popups aninhados, `IsPopupOpen`, fecho por clique fora com flags | — | **não existe** |
| `BeginTooltip` como popup | — | **não existe** |

## Menus

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `BeginMenuBar` (dentro da janela) | `beginMenuBar(rect)` | existe: a barra é um item do layout (o cursor fica abaixo dela, como no grupo que o ImGui abre lá dentro), com rect e hover do item e sem id |
| `BeginMainMenuBar` (barra da aplicação) | `beginMainMenuBar`/`endMainMenuBar` | existe: uma janela sem nada à volta no topo do ecrã, com a ordem de cima (é o que a desenha por cima das outras e lhe dá o ponteiro antes delas), e com os `beginMenu`/`menuItem` de sempre lá dentro. A altura que ocupa é o `mainMenuBarHeight()` (a aplicação tem de deixá-la livre, como o work area do ImGui) |
| `BeginMenu`/`EndMenu`/`BeginSubMenu` | `beginMenu`/`endMenu`/`beginSubMenu` | existe |
| `MenuItem` com atalho (`"Open", "Ctrl+O"`) e checkbox/selected | `menuItem(label, shortcut, enabled, selected)` / `menuCheckbox` | existe: o atalho é desenhado à direita, em baixo-relevo, e conta para a largura do popup; quem o trata é a aplicação (é texto) |
| `MenuItem` com enabled/disabled | `menuItem(label, enabled)` | existe |
| `BeginPopupMenu`/`ImGuiPopupFlags`, `BeginMenu` com `enabled` | parcial | |

## Drag & drop

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `BeginDragDropSource`/`EndDragDropSource` | `beginDragSource(id, type, data, preview, bounds)` | parcial: não há previews nem flags (só um texto) |
| `SetDragDropPayload` (payload com tipo e flags) | `beginDragSource` (id + tipo + valor) | parcial: sem flags de condição |
| `BeginDragDropTarget`/`AcceptDragDropPayload` | `acceptDragDropTarget(tipo, rect, payload)` | parcial |
| `ImGuiDragDropFlags_*` (SourceNoPreviewTooltip, AcceptBeforeDelivery, PayloadAutoExpire, ...) | — | **não existe** |
| Os exemplos do demo (reordenar, copiar/trocar, arrastar para reordenar) | — | **não existe** |

## Blocos desativados

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `BeginDisabled`/`EndDisabled` | `beginDisabled`/`endDisabled` | existe (aninhável; os widgets ficam sem interação e o desenho é atenuado no `DrawList`, sem acumular a atenuação) |
| `IsItemDisabled`, `PushItemFlag(Disabled)` | `isDisabled()` (diz se está dentro do bloco), `isItemDisabled()` | existe: `isItemDisabled` responde pelo último item; as flags por item (`PushItemFlag`) não existem |

## Layout e scrolling

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `SameLine`, `Spacing`, `Dummy`, `Indent`, `Unindent`, `NewLine` | `sameLine`, `spacing`, `dummy`, `indent`, `unindent`, `newLine` | existe |
| `BeginChild`/`EndChild` | `beginChild(id, altura, border, largura)` | parcial |
| `ImGuiChildFlags_*`: Borders, ResizeX/Y, AutoResize, AlwaysAutoResize, NavFlattened | — | **não existe** |
| `BeginGroup`/`EndGroup` | `beginGroup`/`endGroup` | existe: o bloco é um item lá fora (a margem muda lá dentro, `sameLine` a seguir salta o bloco todo), tem rect e responde ao hover, sem id nem clique (o `beginPropertyRow` continua a ser a linha rótulo/valor alinhada) |
| A altura de uma linha é a do item mais alto dela | `LayoutState::lineTop`/`lineHeight` | existe (era a do último item, o que puxava a linha seguinte para cima quando o último item de um `sameLine` era mais baixo) |
| `Columns`/`BeginColumns` (API antiga) | — | **não existe** |
| `ImGuiListClipper` | `beginVirtualList` (+Table/Tree) | parcial: exige `first/last` explícitos |
| `PushItemWidth`/`PopItemWidth`/`SetNextItemWidth`/`CalcItemWidth` | `pushItemWidth`/`popItemWidth`/`setNextItemWidth`/`calcItemWidth` | existe (uma guarda de profundidade: um `pushItemWidth` dentro de outro bloco herda a largura de fora em vez de a repor) |
| `AlignTextToFramePadding` | — | **não existe** |
| `SetCursorPos`/`GetCursorPos`/`GetContentRegionAvail` | `setCursor`/`cursor`/`availableWidth`/`availableHeight` | existe |
| Scroll horizontal, scrollbar customizada | parcial | |
| `GetWindowPos/Size/Width/Height`, `SetNextWindowSizeConstraints` | `windowBounds` | parcial |
| `ImGuiWindowFlags_NoTitleBar` | `WindowOptions::titleBar = false` | existe |
| `ImGuiWindowFlags_NoMove` / `NoResize` | `WindowOptions::move` / `resize` | existe |
| `ImGuiWindowFlags_NoSavedSettings` | `WindowOptions::saveSettings` | existe |
| Geometria fixa por frame (`SetNextWindowSize(..., Always)`) | `WindowOptions::fixedBounds` | existe |
| `ImGuiWindowFlags_NoScrollbar/NoScrollWithMouse`, `AlwaysAutoResize`, `NoBackground`, `NoBringToFrontOnFocus`, `NoInputs`, `NoFocusOnAppearing`, `MenuBar`, `NoDecoration`, `NoCollapse`, `NoNav*`, `NoDocking` | — | **não existe**: a janela é redimensionável e movível por omissão, e o resto dos flags não tem interruptor |

## Fontes

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `PushFont`/`PopFont` com vários tamanhos | — | **não existe**: uma fonte e um tamanho por contexto (o `theme_.fontSize`) |
| Fonte por widget (`ImFont*` no `PushFont`) | — | **não existe** |
| `AddFontFromMemoryTTF` (uma fonte a partir de bytes) | `FontAtlas(bytes, ranges, w, h, bakedSize)` | existe (os bytes são validados antes de chegarem ao stb, que não se defende sozinho de um ficheiro que não seja fonte) |
| `AddFontFromFileTTF` | o ficheiro é lido pela aplicação e passado como bytes | existe (não há leitura de disco dentro da biblioteca, que não tem sistema de ficheiros) |
| Merge de ícones numa fonte, `GlyphRanges` por fonte | — | **não existe**: uma fonte por atlas, com os ranges do construtor |
| `ImFontAtlas` dinâmico (backend fornece as texturas: `ImTextureData`) | — | **não existe**: o atlas é construído uma vez e o backend carrega-o |
| `ShowFontSelector`, `GetFontSize`, `RasterizerDensity`, escala de fonte | — | **não existe** |

## Imagens

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `Image` | `image(texture, w, h, uv, tint)` | existe |
| `ImageButton` / `ImageWithBg` | `imageButton` | parcial: sem fundo próprio |
| Texturas dinâmicas (`ImTextureRef`/`ImTextureData`, atlas gerido pelo backend) | `TextureId` + `addImage` | parcial: o backend cria a textura, não há protocolo de atualização incremental |

## Estado do item e da janela

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `IsItemHovered/Active/Clicked/Focused/Edited/Disabled` | `isItemHovered/isItemActive/isItemClicked/isItemFocused/isItemEdited/isItemDisabled` | existe (o `LastItemData` do iGUI); há também `isHovered(id)`/`isClicked(id)` para widgets próprios |
| `IsItemActivated/Deactivated/DeactivatedAfterEdit`, `IsItemToggledOpen`, `IsItemVisible`, `IsItemEnabled` | — | **não existe** |
| `GetItemRectMin/Max/Size`, `GetItemID` | `itemRect()`, `itemId()` | existe |
| `GetItemFlags` | — | **não existe** |
| `IsItemHovered` com flags (`Delay`, `Stationary`, `AllowWhenBlockedBy*`, `AllowWhenDisabled`, `RectOnly`) | `isItemHovered()` sem flags | parcial |
| `IsItemHoverable`, `SetItemAllowOverlap`, `SetNextItemAllowOverlap` | — | **não existe** |
| `IsWindowFocused/Hovered/Appearing/Collapsed`, `IsAnyItemHovered/Active` | `wantsPointer`/`wantsKeyboard`/`isMenuOpen` | parcial |
| `IsRectVisible`, `GetWindowDrawList`, `ImDrawList` público | `draw*` do `Context` | parcial: o `DrawList` existe, mas a aplicação só desenha pelo Context |
| `SetItemTooltip` | `tooltip` | parcial |

## Teclado, rato e navegação

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `IsKeyPressed/IsKeyDown/IsKeyReleased/IsKeyChordPressed`, `GetKeyPressedAmount` | `isKeyPressed(key)`/`shortcut(key, ctrl, shift)` | parcial: por `KeyCode` (poucas teclas) |
| `IsMouseDown/Clicked/DoubleClicked/Dragging/Hovered`, `GetMouseDragDelta`, `SetMouseCursor` | `isPointerButtonDown`, `pointerPosition`, `pointerPressedPosition` | parcial |
| `NavEnableKeyboard/Gamepad`, foco programático (`SetKeyboardFocusHere`, `SetItemDefaultFocus`), `IsItemFocused`, tab order explícita | — | **não existe** (o iGUI tem um `advanceFocus` interno, sem API) |
| `WantCaptureMouse/Keyboard` | `wantsPointer`/`wantsKeyboard`/`wantsTextInput` | existe |
| `SetNextFrameWantCaptureKeyboard/Mouse`, `Shortcut`, `SetKeyOwner` | — | **não existe** |
| `GetIO`/`ImGuiIO` (config global, `ConfigFlags`, `BackendFlags`, `IniFilename`, `FontGlobalScale`, `MouseDrawCursor`, `ConfigWindowsMoveFromTitleBarOnly`, ...) | `Theme` + `FrameInfo` + `setWindowStatePath` | **não existe** objeto de configuração global |

## Ferramentas do demo (não são widgets)

| Demo do ImGui | iGUI | Estado |
| --- | --- | --- |
| `ShowMetricsWindow` (Debugger) | — | **não existe** |
| `ShowStyleEditor` | — | **não existe** |
| `ShowAboutWindow` | — | **não existe** |
| `ShowUserGuide` (tabelas de atalhos) | — | **não existe** |
| `ShowDebugLogWindow` | — | **não existe** |
| `ShowIDStackToolWindow` | — | **não existe** |
| `ShowStyleSelector` / `ShowFontSelector` | — | **não existe** |
| Style vars (`PushStyleColor/Var`, `PopStyleColor/Var`), `ImGuiStyle` mutável em runtime | `setTheme` (global) + cores por widget | parcial |
| Config flags (`io.ConfigFlags`) | — | **não existe** |
| Log/clipboard (`LogToTTY`, `LogToClipboard`) | clipboard pelo backend, sem log | parcial |

## Apps de exemplo (menu "Examples" do demo)

O demo abre catorze aplicações de exemplo pelo menu. O iGUI tem widgets que
resolvem o mesmo problema, mas não como exemplos prontos:

| App do demo | iGUI | Estado |
| --- | --- | --- |
| Main menu bar | `beginMenuBar` numa janela | parcial: falta uma barra de aplicação |
| Assets Browser | `beginVirtualList`/`beginVirtualTable` | parcial: os widgets existem, não o exemplo |
| Console | (widget de consola no modo retido) | parcial: só no modo retido |
| Custom rendering | `draw*` do Context | parcial |
| Documents | `beginDockSpace`/`beginDockPanel` | parcial |
| Log | — | **não existe** |
| Property editor | `beginPropertyRow` | parcial |
| Simple layout | `beginChild` + `sameLine` | parcial |
| Simple overlay | `draw*` + `wantsPointer` | parcial |
| Auto-resizing window / Constrained-resizing / Fullscreen | `beginWindow` com bounds; sem auto-resize nem constraints | parcial |
| Long text display | `label` sem wrap | parcial |
| Manipulating window titles | `beginWindow(titulo)` | parcial |

## E o que o iGUI tem que o demo do ImGui não tem

Para a balança não ficar torta: no **modo imediato**,

- docking de série (`beginDockSpace`/`beginDockPanel`, com arrastar tabs entre regiões
  e layout guardado no `.ini`) — no ImGui é um ramo separado;
- editor de código com realce de sintaxe (`codeEditor`) e as operações de clipboard
  programáticas;
- widgets de domínio: `fader`, `gizmo2D`/`gizmo3D`, `splitter`, `gradientEditor`,
  `curveEditor`, `sequencer`, `timeline`;
- vistas virtualizadas de série: `beginVirtualList`/`beginVirtualTable`/`beginVirtualTree`;
- `messageBox`, `beginDialog` e `showToast` como chamadas de primeira linha;
- estado das janelas no `.ini` (posição, tamanho, minimizada/maximizada, layout de
  docking) com a mesma ideia do `imgui.ini`;
- um `pushUndo`/`undo`/`redo` no contexto;
- e, fora do modo imediato, uma segunda API retida sobre o mesmo motor — que traz
  node editor, timeline, piano roll, thumbnail grid, file dialog, console, dock
  panels e um motor de rasterização próprio.

---

# TODO

Ordem sugerida: primeiro o que é barato e aparece em qualquer UI, depois o que
precisa de estado novo no `Context`, e por fim o que mexe no motor de desenho ou
na API de fontes. Cada item diz onde implementar.

## P0 — barato, uso geral

Feito nesta ronda (já no `main`, com testes em `tests/test_widgets.cpp`):

- [x] **Opções de janela** — `Context::WindowOptions` com `titleBar`, `move`,
      `resize`, `saveSettings` e `fixedBounds`; é o que permite a faixa de números
      do espelho ser uma janela sem moldura, como a do ImGui
      (`ImGuiWindowFlags_NoTitleBar|NoResize|NoMove`).
- [x] **`displaySize()`** — o tamanho do ecrã que o frame trouxe, para uma janela
      ou sobreposição se colocar sem saber quem a chamou.
- [x] **Série e barras numa só primitiva** — `DrawList::addPolyline` (uma fita
      por série, com a orla suave de fora a fora e nós em esquadria normalizada e
      limitada) e `DrawList::addRectFilledBatch` (um reserva de índices e um teste
      de clip para todas as barras). Os plots passaram a usá-las: 44.4 → 31.9 µs
      na cena `plots` do benchmark, e 3836 → 2932 vértices.
- [x] **Tooltip com widgets dentro** — `beginTooltip`/`endTooltip`: caixa medida
      pelo conteúdo (via `LayoutState::contentExtent`), colocada ao lado do
      ponteiro e virada para dentro quando não cabe, desenhada por cima das
      sobreposições das janelas e por baixo dos modais. O corpo vai para uma lista
      própria e o clip passa a ser o do ecrã, porque o tooltip sai fora da janela
      onde foi submetido; dentro dele o `itemHovered`/`itemClicked` respondem
      sempre que não, senão o hover saía ao dono e os dois piscavam.

- [x] **`BeginDisabled`/`EndDisabled`** — contador no `Context`, interação cortada
      no `itemHovered`/`itemClicked` e atenuação central no `DrawList`
      (`setDisabledFade`), por isso vale para qualquer primitiva, incluindo as
      widgets compostas.
- [x] **Variantes de texto** — `textDisabled`, `textColored`, `textWrapped` e
      `bulletText`, todas como itens de layout (medem e avançam o cursor), com
      wrap por codepoint.
- [x] **`progressBar` com overlay e indeterminada** — `overlay` centrado na barra
      e `value < 0` para a faixa que anda com o tempo do contexto.
- [x] **`colorButton`** — amostra clicável, com o rótulo ao lado.
- [x] **`inputTextWithHint`** — a dica sai na cor do texto desativado enquanto o
      campo está vazio.
- [x] **Fecho nos headers** — `collapsingHeader(label, expanded, open)`: o `x`
      fecha-o e o header deixa de ser desenhado.
- [x] **`plotLines` / `plotHistogram`** — série `const float *` + contagem lida em
      anel (`offset`), escala automática (`scaleMax <= scaleMin`) ou explícita,
      decimação a uma coluna por pixel, destaque da amostra sob o ponteiro com
      tooltip, overlay centrado e cores do tema (`plotBackground`, `plotLines`,
      `plotHistogram`).

Continua em falta neste nível:

- [ ] **Bullet nos headers.** `treeNode` com um modo bullet
      (`ImGuiTreeNodeFlags_Bullet`/`SpanAvailWidth`), que partilha o indent com o
      `bulletText`.
      *Onde:* `src/Gui.cpp` (`treeNode`/`treeItem`), `include/igui/Gui.hpp`.
- [x] **Convenção `##` nos rótulos.** "Gravar##ficheiro" desenha "Gravar" mas o
      id é a string toda, e "##só-id" não desenha nada: é o que deixa dois widgets
      com o mesmo texto visível terem estados separados, e o que faz um campo
      numérico não ocupar largura com um rótulo (o demo usa `##` em todo o lado,
      por isso é isto que agora faz os dois painéis escreverem o mesmo). Aplicado
      em `visibleLabel()`, usado onde o rótulo é medido ou desenhado - e a medida
      do *layout* também é a do texto visível (um `##id` faz o item ser só a
      caixa, sem largura de rótulo).
      *Onde:* `src/Gui.cpp` (`visibleLabel`), `tests/test_item_status.cpp`.

## P1 — dimensão média

- [ ] **Flags de drag/slider e as variantes numéricas que faltam**:
      `Clamp`, `Wrap`, `Logarithmic`, `AlwaysClamp`, `NoInput`; e depois
      `dragInt2/3/4`, `sliderFloat2/3/4`, `sliderInt2/3/4`,
      `inputInt2/3/4`, `inputDouble` (feito) e o `vSliderFloat`/`vSliderInt`
      (feito, com o `sliderValue` a aceitar os dois eixos).
      *Onde:* `sliderValue`/`editNumericText` em `src/Gui.cpp:1256`, e as
      declarações em `include/igui/Gui.hpp:458-471`.
- [x] **Consultas do item anterior**: `isItemHovered`, `isItemActive`,
      `isItemClicked`, `isItemFocused`, `isItemEdited`, `isItemDisabled`,
      `itemRect` e `itemId` — o `LastItemData` do iGUI (`Context::ItemStatus`,
      `item_`), preenchido por `itemHovered`/`itemClicked`/`sliderValue`/
      `advanceLayout`. Um widget composto (o slider com o editor de valor, o drag
      com o campo que abre) continua a ser um item: `keepItemStatus` funde o
      sub-item principal depois de o editor correr.
      *Onde:* `src/Gui.cpp` (`itemHovered`, `itemClicked`, `keepItemStatus`,
      `sliderValue`, `advanceLayout`), `include/igui/Gui.hpp` (`ItemStatus`),
      `tests/test_item_status.cpp`.
- [ ] **O resto das consultas do item**: `isItemActivated`/`isItemDeactivated`/
      `isItemDeactivatedAfterEdit`, `isItemToggledOpen`, `isItemVisible` e os
      flags do `IsItemHovered` (`Delay`/`Stationary`/`AllowWhenBlockedBy*`/
      `RectOnly`).
      *Onde:* o `ItemStatus` em `include/igui/Gui.hpp` já tem o sítio.
- [x] **Layout**: `newLine`, `pushItemWidth`/`popItemWidth`/
      `setNextItemWidth`/`calcItemWidth`. Um `##id` também já não gasta largura de
      rótulo, que era o que faltava para os campos do demo ficarem iguais.
      *Onde:* `src/Gui.cpp` (`newLine`, `setNextItemWidth`, `calcItemWidth`,
      `advanceLayout`), `tests/test_item_status.cpp`.
- [x] **Um popup aberto é dono do ponteiro na sua área.** Antes perguntava-se "este
      ponto está na área do popup?" e isso deixava passar o que está por baixo (o
      rect de um widget de conteúdo também cai lá dentro): com um dropdown aberto
      em cima de um botão, o botão ficava hot sob o menu. A pergunta passou a ser
      "estamos a submeter os itens do popup?" (`activeMenu_`, `comboItemsSubmitted_`).
      Com isto, a barra de menus passou a avançar o layout como uma barra do ImGui
      (era o mesmo sintoma para quem não deixasse os 26 px à mão; os `spacing` de
      compensação saíram dos exemplos).
      *Onde:* `src/Gui.cpp` (`pointerBlockedByOpenPopup`, `endMenuBar`,
      `setItemHoverRect`), `tests/test_item_status.cpp`.
- [x] **`beginGroup`/`endGroup`**: um bloco que é um item para o layout de fora.
      Dentro, a margem é a do grupo e as linhas são as do grupo; fora, o rect é a
      caixa do que ficou lá dentro, o hover responde na caixa toda e não há id
      (um clique é sempre de um widget lá dentro). No espelho é o que põe as
      quatro colunas de sliders verticais do demo lado a lado.
      *Onde:* `src/Gui.cpp` (`beginGroup`/`endGroup`, `advanceLayout`),
      `include/igui/Gui.hpp` (`GroupState`), `tests/test_item_status.cpp`.
      Com isto veio a regra da altura da linha (a do item mais alto), que é o que
      o ImGui usa e o que faz a linha seguinte cair abaixo do bloco todo.
- [ ] **Layout, o resto**: `alignTextToFramePadding`. A altura de uma linha
      também podia ter uma versão "por omissão" configurável (o `style.ItemSpacing`
      já existe); falta ainda uma largura por omissão de *container* (`pushItemWidth`
      tem uma só profundidade de guarda).
      *Onde:* `src/Gui.cpp` (`beginLayout`, `advanceLayout`, `sameLine`).
- [x] **`beginCombo`/`endCombo`**: popup de conteúdo livre, com o `drawComboHeader`
      a servir os dois (o `comboBox` e o `beginCombo`), o cursor do layout dentro do
      popup e a medida do conteúdo a dar-lhe o tamanho no frame seguinte.
      *Onde:* `src/Gui.cpp` (`beginComboImpl`/`endCombo`), `tests/test_item_status.cpp`.
- [ ] **Scroll dentro do popup de um combo** (o ImGui mostra 8 linhas e rola) e
      aceitar um `beginChild` lá dentro para o fazer à mão.
      *Onde:* `src/Gui.cpp` (`beginComboImpl`), a máquina de scroll do `listBox`.
- [x] **`menuItem` com atalho desenhado**: `menuItem(label, shortcut, enabled,
      selected)` desenha o atalho à direita e soma-o à largura medida do menu.
      *Onde:* `src/Gui.cpp` (`menuItemInternal`), `tests/test_item_status.cpp`.
- [x] **`beginMainMenuBar`** (a barra da aplicação, que não pertence a nenhuma
      janela): uma janela sem barra de título, sem mover nem redimensionar, com os
      limites fixos no topo do ecrã, e com a ordem mais alta no fim de cada frame
      (é o que a põe por cima das janelas e com o ponteiro antes delas).
      *Onde:* `src/Gui.cpp` (`beginMainMenuBar`/`endMainMenuBar`), `tests/test_item_status.cpp`.
- [ ] **Work area**: no ImGui as janelas são limitadas ao espaço que sobra da barra
      (o `viewport->WorkOffsetMin.y`); aqui a aplicação tem de deixar a faixa livre
      pelo `mainMenuBarHeight()`.
      *Onde:* `src/Gui.cpp` (`beginWindow`, `mainMenuBarHeight`).
- [ ] **`arrowButton` e `checkboxFlags`** (bitfield).
      *Onde:* `src/Gui.cpp:1336`/`1386`.

## P2 — estruturas novas

- [x] **Seleção múltipla** (`beginMultiSelect`/`endMultiSelect` com `SingleSelect`/
      `NoSelectAll`/`NoRangeSelect`/`ClearOnEscape`, `isItemSelected`/
      `isItemToggledSelection`): a seleção é um `Span<bool>` da aplicação e cada
      `selectable` do bloco é um item, pela ordem em que é submetido. As mesmas
      acções do ImGui (`MultiSelectItemFooter`): o clique limpa e escolhe, o Ctrl
      troca um, o Shift escolhe o alcance, o Ctrl+Shift copia o estado do item de
      origem, o Ctrl+A escolhe tudo e o Escape limpa.
      *Onde:* `src/Gui.cpp` (`beginMultiSelect`/`applyMultiSelectClick`),
      `tests/test_item_status.cpp`.
- [ ] **O resto do modelo de seleção**: o `ImGuiMultiSelectIO` (pedidos) para quem
      tem a seleção em ids e não em índices, o box select (arrastar uma caixa), o
      auto-select na navegação por teclado, o `SetNextItemSelectionUserData`, o
      `listBox` de seleção múltipla e o `beginVirtualList` como clipper.
      *Onde:* `src/Gui.cpp` (`applyMultiSelectClick`), `src/Gui.cpp` (`listBox`).
- [ ] **Flags de tabela**: `resizable`, `reorderable`, `hideable`, `sortable`,
      `borders`/`rowBg`, `scrollX/Y`, `sizingFixedFit`, `freezeRows/Cols` e o
      menu de contexto de coluna. A infraestrutura de colunas por peso já existe.
      *Onde:* `src/Gui.cpp` (`tableColumnX`, `tableColumnWidth`, `beginTable`),
      `include/igui/Gui.hpp:729-743`.
- [ ] **Popups com pilha e ids**: `openPopup`, `beginPopup`, `beginPopupModal`,
      `closeCurrentPopup`, `isPopupOpen`, fecho por clique fora com flags,
      popups aninhados. Hoje o `beginDialog`/`messageBox` cobrem o caso simples.
      *Onde:* `src/Gui.cpp` (a máquina de `openMenu_`/`activeModal_` é o
      precedente), `include/igui/Gui.hpp:604`.
- [ ] **Fontes**: `pushFont`/`popFont` com vários tamanhos (e, mais tarde,
      ficheiros diferentes). Exige o `FontAtlas` com mais do que uma face;
      a medição e o desenho já são por `FontId`.
      *Onde:* `src/FontAtlas.cpp`, `include/igui/FontAtlas.hpp`, e o
      `theme_.font`/`theme_.fontSize` em `src/Gui.cpp` (`measureText`, `drawText`).
- [ ] **Navegação por teclado e foco programático**: expor o `advanceFocus` que
      já existe (`setKeyboardFocusHere`, `isItemFocused`, `focusNextWindow`,
      `activateItem`), e uma tab order explícita.
      *Onde:* `src/Gui.cpp:1255` (`advanceFocus`, `registerFocusable`),
      `include/igui/Gui.hpp`.
- [ ] **Flags de drag & drop** (preview próprio, aceitar antes de largar, expirar
      payload, condição de entrega) e os exemplos de reordenar/copiar.
      *Onde:* `src/Gui.cpp` (`beginDragSource`, `acceptDragDropTarget`,
      `drawDragDropPreview`).

## P3 — ferramentas e infraestrutura

- [ ] **Painel de métricas** (janelas, draw lists, vértices, tempo por janela) —
      útil para diagnosticar, e o iGUI já tem os contadores no `Context`.
- [ ] **Editor de tema** em runtime (cores, paddings, raio) por cima de
      `setTheme`, sem recompilar.
- [ ] **Log de debug** das ações do contexto (cliques, ids que reagem) para
      resolver sobreposições de ids.
- [ ] **Style vars** (`pushStyleColor`/`pushStyleVar`) para mudar uma cor ou
      medida num bloco, sem tocar no tema global.
- [ ] **Protocolo de texturas dinâmicas** no backend (como o
      `ImGuiBackendFlags_RendererHasTextures` do ImGui 1.92): atualizações
      incrementais do atlas e texturas de ícones, em vez do upload inteiro.
- [ ] **`imGuiIO` equivalente**: um objeto de configuração e estado global
      (escala de fonte, cursor, limites de ini, flags de comportamento) em vez
      de setters espalhados pelo `Context`.

## Notas

- As lacunas que **não** valem a pena copiar: as APIs obsoletas do ImGui
  (`Columns`, `BeginChildFrame`, `DragScalar(N)` genérico) e o
  `PushAllowKeyboardFocus`/`PopAllowKeyboardFocus`, já removidos.
- O espelho (`GalleryIgui.cpp`) é a checklist viva: cada `reportGap()` que
  desaparecer passa a ser uma linha a menos no relatório do demo, e o número de
  lacunas impresso no fim é o progresso.

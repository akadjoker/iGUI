# Ronda 4 (ramo igui-round4, a partir de origin/main 1a77b1f)

Lista de 21 itens (defeitos e adicoes que o zenMCAP e o ZenDAW contornaram, ver docs/LIBRARY-NOTES.md do ZenDAW). Este
ficheiro diz o que esta feito, em que commit, e o que falta, para quem continuar a partir do ramo. Um item por commit,
cada um com o seu teste (falha antes, passa depois); os testes novos estao em tests/test_retained_basic.cpp (alvo
igui_retained_basic) e tests/test_retained_menus.cpp (igui_retained_menus). Regras: sem `std::` nem cabecalhos da
biblioteca standard (a ct substitui-a; `tools/check.sh`, fase nostd), sem abrandar nada (benches `igui_retained_bench`
fixados com `taskset -c 2`, a mesma fonte do bench dos dois lados, minimo de 5 corridas alternadas, e o que mexe menos
de 3% so conta se sobreviver a recompilar os dois lados com `-falign-functions=64 -falign-loops=64`), tudo num scope
systemd com MemoryMax=4G, `-j3`, os testes que abrem janelas so por `tools/xvfb-test.sh`.

Como construir e correr (a partir da raiz do repositorio): `IGUI_CONTAINERS_DIR=<ct em 34169bc> IGUI_ZEN_PLATFORM_DIR=<zen_plataform em db76a13> tools/check.sh --build <dir> --jobs 3`.

## Estado

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 1 | CheckBox sem visto (e o do cabecalho do Collapsible) | feito | 77c1ffe | testCheckBoxDrawsATick |
| 2 | Button: hint e paint a discordar (SplitButton, ToolBar, ImageButton, IconButton) | feito (Button e SplitButton; os outros ja concordavam) | d387b39 | testButtonAtItsHintShowsItsWholeLabel |
| 3 | Label e ComboBox::sizeHint com a estimativa 0,6 x corpo | por fazer (ver abaixo) | | |
| 4 | Menu: atalho por cima da seta do submenu | feito | 08496df | testMenuShortcutDoesNotOverlapTheSubmenuArrow |
| 5 | Collapsible::sizeHint inclui o proprio rect | feito | e1b816c | testCollapsibleHintIgnoresItsRect |
| 6 | ScrollView::layout: barra horizontal presa, barra vertical fina | por fazer | | |
| 7 | FloatWindow::setFloatPos nao faz layout | feito | 602345d | testFloatWindowSettersLayOut |
| 8 | PianoRoll::onMousePress com botoes 1/2/3 (a janela da 0/1/2) | feito (e o mesmo engano no WaveformView; os outros widgets estao certos) | 8dea1b8 | testPianoRollButtons, testWaveformViewButtons |
| 9 | WidgetApp: layout so no resize / requestLayout() | por fazer (ver abaixo) | | |
| 10 | Chart: segmentos duplicados, rotulo de fatia pequena, histograma sem nome na legenda, formatValue >= 1e9 | por fazer | | |
| 11 | Fuga de Menu ao sair | analisado, por fazer (ver abaixo) | | |
| 12 | MenuAction: icone | por fazer | | |
| 13 | Menu: toggled(bool) e auto-toggle desligavel | por fazer | | |
| 14 | Menu::aboutToShow | por fazer | | |
| 15 | Popup com widgets arbitrarios; FloatWindow com dono | por fazer | | |
| 16 | API publica para injectar input sintetico | por fazer | | |
| 17 | SideTabBar e ToolBar: icones da aplicacao e valores no Theme | por fazer | | |
| 18 | ImageButton: desactivado, hover, premido, tooltip como o IconButton | por fazer | | |
| 19 | Timeline: regua de compassos; PianoRoll: velocidade, sinais com o indice | por fazer; `PianoRoll::setRowHeight` ja existe (altura das linhas configuravel) | | |
| 20 | Chart: dados double; titulos dos eixos | por fazer | | |
| 21 | Slider: leitura do valor | por fazer | | |

Benches: nao se correu nenhuma campanha ainda (so o hash de desenho em cada commit). O commit d387b39 muda
bench/retained/draw.hash (os botoes cujo texto medido passa da estimativa ficam mais largos). A comparacao completa
main contra o ramo (simples e alinhada) esta por fazer.

## Notas para os itens por fazer (o que se apurou a ler o codigo)

- 3: `Label::sizeHint` ja usa `hintedTextWidth` (maximo entre a estimativa e a largura medida, em BasicWidgets.cpp);
  falta a largura so medida, e um minimo pequeno para o Label com `setElide(true)` (os `setElide(true)` de
  tests/EditorTree.hpp e do bench mudam de largura: o hash muda). `ComboBox::sizeHint` (ComboBox.cpp) soma a estimativa
  de cada item: medir com uma memoria (largura e corpo da fonte) para nao medir todos os itens em cada hint.
- 6: ver `ScrollView::layout` (ScrollWidgets.cpp, ~221-238); `Theme::scrollbarWidth` (12) ja existe, ver quem a usa.
- 9: `Widget::markDirty()` nao serve (so o SpinSlider o le, e e chamado por todos os setters e pelo layout); a regra
  proposta: um `Widget::invalidateLayout()` que levanta a bandeira `needsLayout_` da superficie viva (uma escrita), chamado
  por addChild, removeChild, moveChild, setVisible (so se mudou), setMargins/setStretch/setLayoutAlign, os setters de
  BoxLayout, e os setText dos widgets cujo hint depende do texto. Cuidado com os widgets que mexem em filhos dentro do
  paint (viram layout a cada frame) e medir um teste de frame ociosa que conte as chamadas a layout(). O tests/RetainedHarness.hpp
  tem `relayout()` por causa disto; os testes novos que adicionam filhos depois da primeira frame usam-no.
- 11: o `Menu` e dono dos seus `MenuAction` (apaga-os no destrutor e em `clearActions()`), mas `MenuAction::setSubmenu`
  nao e dono do submenu (o comentario diz "not owned"): quem faz `new Menu` para o submenu e o reconstroi (menu de
  ficheiros recentes) tem de o apagar, e os exemplos (examples/sdl2_original_widgets/stages/stage_menus.cpp) nao o fazem.
  `clearActions()` tambem nao poe a zero `hoveredIdx_`, `activeSubmenu_` e `activeSubmenuIdx_` (ponteiro solto se o menu
  esta aberto). Proposta: `Menu::addSubmenu(texto)` que cria um submenu pertencente a accao e `setSubmenu(Menu*, bool owned = false)`;
  `clearActions` e o destrutor apagam os que a accao possui; documentar; teste sob ASan (alvo igui_retained_menus corre
  no `check.sh asan`).

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

## Estado (2026-11-17, fim do dia; a ponta de igui-round4 e e5ba9a7)

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 1 | CheckBox sem visto (e o do cabecalho do Collapsible) | feito | 77c1ffe | testCheckBoxDrawsATick |
| 2 | Button: hint e paint a discordar (SplitButton, ToolBar, ImageButton, IconButton) | feito (Button e SplitButton; os outros ja concordavam) | 37eb3ea | testButtonAtItsHintShowsItsWholeLabel |
| 3 | Label e ComboBox::sizeHint com a estimativa 0,6 x corpo | codigo escrito mas nao compilado, no ramo igui-round4-wip (5f59819) | | testLabelAndComboHintsAreMeasured (no wip) |
| 4 | Menu: atalho por cima da seta do submenu | feito | 9074d03 | igui_retained_menus testMenuShortcutDoesNotOverlapTheSubmenuArrow |
| 5 | Collapsible::sizeHint inclui o proprio rect | feito | e1b816c | testCollapsibleHintIgnoresItsRect |
| 6 | ScrollView::layout (barra horizontal presa) e espessura das barras no Theme | codigo escrito mas nao compilado, no wip: ScrollBar::thickness() = Theme::scrollbarWidth (12) em vez de 8, tambem nas grelhas (kBarW) e no Timeline | | testScrollViewBars (no wip) |
| 7 | FloatWindow::setFloatPos nao faz layout | feito | 602345d | testFloatWindowSettersLayOut |
| 8 | PianoRoll::onMousePress com botoes 1/2/3 (a janela da 0/1/2) | feito (e o mesmo engano no WaveformView; os outros widgets estao certos) | 8dea1b8 | testPianoRollButtons, testWaveformViewButtons |
| 9 | WidgetApp: layout so no resize / requestLayout() | feito em dois commits: invalidateLayout (2bf456d) e a correcao de nao repor filhos postos a mao (e5ba9a7); ver a nota abaixo | 2bf456d, e5ba9a7 | testLayoutRunsByItself, testAutomaticLayoutKeepsHandPlacedWidgets, igui_retained_idle testIdleFrameDoesNoLayout |
| 10 | Chart: segmentos duplicados, rotulo de fatia pequena, histograma sem nome na legenda, formatValue >= 1e9 | formatValue feito (57941ab, testFormatValue). Segmentos duplicados: causa achada e corrigida no wip (DrawList::pathStroke, ver abaixo), nao compilada. Faltam o rotulo da fatia pequena e o nome do histograma na legenda | 57941ab | |
| 11 | Fuga de Menu ao sair | feito: contrato escrito no cabecalho, Menu::addSubmenu e setSubmenu(m, owned) novos; setSubmenu(m) continua sem apagar | f79a949 | igui_retained_menus testMenuOwnership (ASan limpo; com o destrutor mutilado o LSan apanha) |
| 12-17 | MenuAction icone; Menu toggled e auto-toggle; aboutToShow; popup com widgets; API de input sintetico; SideTabBar/ToolBar com icones da aplicacao e Theme | por fazer | | |
| 18 | ImageButton: desactivado, hover, premido, tooltip como o IconButton | ja existiam (fixados por teste); a imagem desactivada passa a esbatida | e8e406e | testImageButtonStates |
| 19 | Timeline: regua de compassos; PianoRoll: velocidade, sinais com o indice | por fazer; `PianoRoll::setRowHeight` ja existe (altura das linhas configuravel) | | |
| 20 | Chart: dados double; titulos dos eixos | por fazer | | |
| 21 | Slider: leitura do valor | por fazer | | |

## Item 9: o que muda e o que se decidiu (a pergunta do dono)

Antes desta ronda, `WidgetApp::paint` so fazia layout com `needsLayout_` (resize, setStage, `requestLayout()`, que varios
widgets chamam: DockPanel, SideTabBar, Collapsible, ToolBar...) e, nesse caso, repunha o rectangulo de cada filho directo do
stage para a janela toda (`for c in root->children(): c->setRect({0,0,w,h})`). O commit 2bf456d fazia o `invalidateLayout()`
(addChild, removeChild, setVisible, setText...) passar pelo mesmo caminho: um filho do stage posto a mao com `setRect` (painel,
overlay) passava a ser reposto por qualquer addChild ou setText na arvore. O commit e5ba9a7 corrige: dois pedidos,
`requestLayout()` (como sempre: aplica a janela e repoe os filhos do stage) e `requestRelayout()` novo (`Surface::relayout_`), que
e o que o `invalidateLayout()` faz: os contentores voltam a arrumar os filhos e dos filhos directos do stage so toma a janela o
que ainda nao tem tamanho (acabado de acrescentar). O que um contentor (BoxLayout, Collapsible, TabLayout...) faz aos seus
filhos no seu layout() e igual ao do resize; um Widget simples nao mexe nos filhos. O teste testAutomaticLayoutKeepsHandPlacedWidgets
falha com a versao anterior (4 verificacoes) e passa agora.

Aplicacoes do dono (grep de `setRect(` e de filhos postos directamente no stage, em src de ZenVideo, ZenMix, ZenDAW, coco, deform,
ZenStage, animator, MapShape, ZenHuman; zenforms nao tem src na raiz): todas tem UM filho no stage, um BoxLayout (ou o FormRoot no
zenforms) que enche a janela; os `setRect` que ha estao dentro de widgets proprios (layouts de AboutPanel, SideSplitter, SongList,
PatternsPage...), em popups (DropPopup, RenamePopup: nao estao na arvore) ou em dialogos do animator (`dialog->setRect(...)`,
provavelmente flutuantes: confirmar). Nenhuma poe a mao um filho do stage que sobreviva a um resize, por isso nao ha caminho
conhecido onde o layout automatico mova o que a aplicacao pos. A verificar: o zenforms (`FormRoot::layout` reaplica as ancoras
de `designW_/designH_` aos filhos em cada layout, agora mais vezes: idempotente, mas o designer arrasta controlos com setRect;
ver se o arrastar sobrevive a um addChild ou setText).

## Achado novo do item 10 (no wip)

`DrawList::pathStroke` (Retained.cpp): numa polilinha aberta o primeiro e o ultimo ponto tinham a normal de um dos lados a zero,
e o desvio de mitra `(n1+n2)/|n1+n2|^2 * 2` saia duas vezes maior: o primeiro e o ultimo segmento de QUALQUER polilinha aberta
(graficos de linhas, o visto da CheckBox, icones) saiam com o dobro da espessura nas pontas (imagem
scratchpad/ig4/pics/item10_line_before.png, perdida se a maquina reiniciar: reproduz-se com Chart::plot de 6 pontos e
setLineWidth(4)). A correcao no wip: a normal do lado que falta e a do outro. Vai mexer no hash de desenho se o bench tiver
polilinhas abertas (o piano roll?): reler o hash e actualizar em conjunto.

## Por verificar (nada disto foi feito)

- `tools/check.sh` completo na ponta (so se correram: build Release + ctest, 40 testes, o hash de desenho, a fase nostd a mao, e o
  ASan so no igui_retained_menus).
- Benches: main contra a ponta, fixadas (`taskset -c 2`), alternadas, minimo de 5 corridas, simples e alinhadas
  (`-falign-functions=64 -falign-loops=64`), com atencao a frame ociosa, `editor*/layout_only`, `update_only` e as linhas de
  input (o item 9 mexe em todo o setText, setVisible e addChild), mais cenas novas para os hints de Label e Button e para menus.
  O dono nao aceita abrandamentos.
- ZenDAW e ZenVideo contra a ponta (copias em scratch: rsync do projecto sem build, third_party/iGUI trocado pela arvore do
  worktree), so compilar e ligar, e correr cada um escondido (`tools/run_hidden.sh --size 1280x800 -- ...` no ZenDAW; no ZenVideo
  `xvfb-run -a` com HOME e XDG em scratch) com `ZEN_SCREENSHOT=ficheiro.ppm ZEN_SCREENSHOT_FRAME=60`, e comparar com a mesma app
  na origin/main. Na sessao a build do ZenDAW na origin/main acabou (OK); a da ponta estava a meio quando a maquina parou.

## Proximos passos, por ordem

1. No ramo igui-round4-wip (5f59819, nada compilado): compilar, corrigir, correr os testes novos e a bateria, actualizar o hash de
   desenho se mudar (o item 6 e o 10 mexem em barras e em polilinhas), e fazer commits separados por item (3, 6, 10) em
   igui-round4, cada um com o teste a falhar antes e a passar depois (git stash dos src para o ver falhar).
2. `tools/check.sh` completo e as benches (acima); so depois seguir.
3. ZenDAW e ZenVideo (acima).
4. Resto do item 10 (rotulo da fatia pequena do pie/donut; nome do histograma na legenda: sugestao `Series N` para uma serie sem
   nome quando ha mais de uma, mais um acessor `legendLabel(i)` para testar).
5. Stage B por ordem (12 a 17, 19 a 21), um commit por item com o seu teste. Pontos a decidir: 13 (Menu::onMousePress vira o
   checked_ antes de emitir triggered: sinal `toggled(bool)` e `setAutoToggle(false)` por accao), 15 (popup com widgets: reutilizar
   showPopup com owned=false e o lendFocus), 16 (envolver o IO de WidgetApp::update: `feedMouseMove/Button/Wheel/Key/Text`).
6. Mudancas de aspecto ou de comportamento por omissao ate agora (para as notas de cada aplicacao): CheckBox e cabecalho de
   Collapsible com visto (77c1ffe); Button mais largo quando o texto medido passa da estimativa e SplitButton 6 pixels mais largo,
   com o hash do editor mudado (37eb3ea); atalho do menu mais a esquerda nos itens com submenu (9074d03); Collapsible::sizeHint
   sem a largura do proprio rect (e1b816c); setFloatPos/setFloatSize actualizam logo os rectangulos (602345d); PianoRoll e
   WaveformView com botoes 0/1/2, e a ZenDAW tem de tirar a traducao em `Roll::onMousePress` (8dea1b8); layout automatico
   (2bf456d, e5ba9a7); ImageButton desactivado esbatido (e8e406e); Chart::formatValue sem %.3g (57941ab).
7. O que a ZenDAW pode apagar quando os itens estiverem feitos: a traducao de botoes do `Roll::onMousePress` (ja), os
   `requestLayout()` a mao em `App::refreshAll`/`showPage` (ja, com o item 9), e depois dos itens 17, 19: `PageRail` e `TileBar`
   (icones da aplicacao e Theme na SideTabBar/ToolBar) e o `RollPane::pushNotes` (sinais com o indice e a velocidade por nota).

## Notas para os itens por fazer (o que se apurou a ler o codigo)

- 3: `Label::sizeHint` ja usa `hintedTextWidth` (maximo entre a estimativa e a largura medida, em BasicWidgets.cpp);
  falta a largura so medida, e um minimo pequeno para o Label com `setElide(true)` (os `setElide(true)` de
  tests/EditorTree.hpp e do bench mudam de largura: o hash muda). `ComboBox::sizeHint` (ComboBox.cpp) soma a estimativa
  de cada item: medir com uma memoria (largura e corpo da fonte) para nao medir todos os itens em cada hint.
- 6: ver `ScrollView::layout` (ScrollWidgets.cpp, ~221-238); `Theme::scrollbarWidth` (12) ja existe, ver quem a usa.
- 9 (feito): a regra esta em `Widget::invalidateLayout()` (comentario em Widget.hpp). Faltam, se se quiser a regra completa,
  os `setText` e afins de outros widgets cujo hint depende do texto (RadioButton, Switch, ComboBox::addItem, Tabs...): cada
  um e uma linha `invalidateLayout();`. Os widgets que mostram e escondem um filho sem que ninguem o posicione (overlays)
  marcam `layoutNeutral_` (membro protegido), como o grabber do SpinSlider; sem isso cada entrada do rato pedia layout.
  O teste de frame ociosa (test_retained_idle) conta as passagens de layout na janela do editor.
- 11 (feito): ver o commit f79a949 e o comentario da classe Menu em MenuWidgets.hpp. Aplicacoes do dono com setSubmenu: coco,
  deform, ZenHuman, ZenStage (src/app/MainWindow.cpp), todas com `setSubmenu(recentMenu_.get())` e o submenu em ct::unique_ptr
  da aplicacao (o ZenStage tambem `clipTransition_->setSubmenu(transitionMenu_.get())`): por isso a posse por omissao nao
  mudou. Nota sobre RTTI/UBSan no README (opcao IGUI_NO_RTTI).

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

## Estado (2026-11-18; a ultima alteracao de codigo de igui-round4 e e2138e5; sobre ela so este ficheiro)

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 1 | CheckBox sem visto (e o do cabecalho do Collapsible) | feito | 77c1ffe | testCheckBoxDrawsATick |
| 2 | Button: hint e paint a discordar (SplitButton, ToolBar, ImageButton, IconButton) | feito (Button e SplitButton; os outros ja concordavam) | 37eb3ea | testButtonAtItsHintShowsItsWholeLabel |
| 3 | Label e ComboBox::sizeHint com a estimativa 0,6 x corpo | feito: o Label pede a largura medida, o Label com setElide(true) pede no maximo as reticencias, o ComboBox mede o item mais largo com a fonte do widget e guarda a largura; o hash do editor mudou (ver abaixo) | 05b75ea | testLabelAndComboHintsAreMeasured (7 verificacoes falham sem o src) |
| 4 | Menu: atalho por cima da seta do submenu | feito | 9074d03 | igui_retained_menus testMenuShortcutDoesNotOverlapTheSubmenuArrow |
| 5 | Collapsible::sizeHint inclui o proprio rect | feito | e1b816c | testCollapsibleHintIgnoresItsRect |
| 6 | ScrollView::layout (barra horizontal presa) e espessura das barras no Theme | metade feita: ScrollView::layout (a barra horizontal presa e o conteudo mais largo arrumado apertado) esta na igui-round4 (43743db, hash igual). A ESPESSURA das barras NAO entrou: esta no ramo igui-round4-scrollbar-theme (7a48f94, em cima de e2138e5) porque muda o aspecto de todas as aplicacoes e o wip rebentava 2 testes que ja existiam (ver abaixo) | 43743db (e 7a48f94 no ramo a parte) | testScrollViewHorizontalBar (2 verificacoes falham sem o src) |
| 7 | FloatWindow::setFloatPos nao faz layout | feito | 602345d | testFloatWindowSettersLayOut |
| 8 | PianoRoll::onMousePress com botoes 1/2/3 (a janela da 0/1/2) | feito (e o mesmo engano no WaveformView; os outros widgets estao certos) | 8dea1b8 | testPianoRollButtons, testWaveformViewButtons |
| 9 | WidgetApp: layout so no resize / requestLayout() | feito em dois commits: invalidateLayout (2bf456d) e a correcao de nao repor filhos postos a mao (e5ba9a7); ver a nota abaixo | 2bf456d, e5ba9a7 | testLayoutRunsByItself, testAutomaticLayoutKeepsHandPlacedWidgets, igui_retained_idle testIdleFrameDoesNoLayout |
| 10 | Chart: segmentos duplicados, rotulo de fatia pequena, histograma sem nome na legenda, formatValue >= 1e9 | formatValue feito (57941ab, testFormatValue). Segmentos duplicados: feito (DrawList::pathStroke, e2138e5, ver abaixo; muda o desenho de todas as polilinhas abertas). Faltam o rotulo da fatia pequena e o nome do histograma na legenda | 57941ab, e2138e5 | testFormatValue, testOpenPolylineHasTheSameWidthAtItsEnds (2 verificacoes falham sem o src) |
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

## Item 10: o achado do pathStroke (feito, e2138e5)

`DrawList::pathStroke` (Retained.cpp): numa polilinha aberta o primeiro e o ultimo ponto tinham a normal de um dos lados a zero,
e o desvio de mitra `(n1+n2)/|n1+n2|^2 * 2` saia duas vezes maior: o primeiro e o ultimo segmento de QUALQUER polilinha aberta
(graficos de linhas, icones, as linhas de dois pontos dos rebordos) saiam com o dobro da espessura nas pontas, e uma linha de
dois pontos saia toda a dobrar. A correcao: a normal do lado que falta e a do outro. Nos pontos do meio e nos caminhos fechados
nada muda. O hash do editor muda (192 de 9096 vertices, 24 tracos: a linha por baixo da barra de menus, os rebordos e os
icones das tres barras de ferramentas, as setas das ComboBox; o piano roll fica igual).

EFEITO NAS APLICACOES (visto no ZenVideo, ver abaixo): as linhas de dois pontos (rebordos, sublinhados, separadores) ficam com
metade da espessura que tinham e os icones de traco (o X, o +, o -, o caixote do lixo, a seta de importar, a seta das ComboBox)
ficam mais finos e mais nitidos. Quem escolheu a espessura a olho contra o desenho antigo vai ver as linhas mais leves: se for
demais, sobe-se a espessura pedida na aplicacao, nao se volta ao desenho com o dobro. Uma linha de espessura 1 centrada numa
coordenada inteira fica a meio de duas linhas de pixels (a 50%), como qualquer traco com anti-aliasing.

## Item 6: o que ficou de fora e porque (ramo igui-round4-scrollbar-theme)

O wip trazia duas coisas no item 6. A do ScrollView::layout entrou (43743db). A espessura das barras (`ScrollBar::thickness()` =
`Theme::scrollbarWidth`, 12 por omissao, em vez dos 8 fixos de `ScrollBar::kBarThickness`) esta num commit a parte, no ramo
`igui-round4-scrollbar-theme` (7a48f94, em cima de e2138e5, compila e passa as 40 provas, com o hash do desenho actualizado), e
NAO esta na igui-round4, por quatro razoes:
- muda o aspecto de todas as aplicacoes: barras 4 pixels mais grossas por omissao (10 e 16 nos temas claro e outro) e a area util
  dos ScrollView, das listas e das grelhas 4 pixels mais estreita quando a barra aparece;
- o wip nao compilava toda: o ThumbnailGrid usa `ScrollBar::kBarThickness` em duas linhas (a barra e o espaco que se reserva para
  ela) e ficava de fora; o ramo ja o inclui;
- o wip rebentava dois testes que ja existiam e esperam 8: `igui_original_widgets` (test_scroll_view_wheel_reaches_the_end_with_both_bars
  soma `ScrollBar::kBarThickness + 2` aos limites de rolagem do ScrollView) e `igui_retained_grid` (as colunas acabam onde a
  barra comeca: 628 de 640, esperava 632); o ramo ja os ajusta;
- as aplicacoes do dono que desenham barras proprias usam a constante publica `ScrollBar::kBarThickness` (ZenMix: SongList e
  AboutPanel; ZenHuman: a reserva do AssetGrid em AppearancePanel): passariam a ter na mesma janela barras proprias de 8 ao lado
  das da biblioteca de 12.
Decidir: ou se aceita o novo aspecto e se fazem merge/cherry-pick do ramo (e as aplicacoes passam a usar `ScrollBar::thickness()`
em vez da constante), ou se muda o valor por omissao de `Theme::scrollbarWidth` para 8 (que muda o TextEdit, o unico que ja o lia).

## Item 3: o que muda no aspecto (05b75ea)

O Label pede a largura medida do texto (antes: o maior entre a medida e 0,6 x corpo x letras), por isso um Label de texto estreito
(numeros, relogios, "0.50") fica mais estreito e o vizinho com stretch toma a diferenca: no editor do bench o relogio do transporte
passa de 117,6 para 95,85 de largura e o valor dos 13 parametros de 50,4 para 42,6. Um Label com `setElide(true)` pede no maximo a
largura das reticencias: numa linha horizontal precisa de `setStretch()` (a ZenMix e a ZenVideo, que usam setElide(true), tem-no em
colunas ou com stretch: lidas todas as ocorrencias, 13 na ZenMix e as do Editor.cpp e 5 do EditorSettings.cpp na ZenVideo), numa coluna toma a largura do pai. O ComboBox mede o item mais largo.
O ZenVideo e o ZenDAW nao mostram diferenca de largura no ecra de arranque (ver abaixo).

## Um teste com defeito achado de caminho: test_retained_fuzz (1c18f64)

O `scratch_` do fuzz era um ponteiro cru para o BoxLayout das propriedades do editor: quando o widget era tirado da arvore e
apagado, o `isIn(scratch_)` so comparava enderecos, e um widget feito depois no mesmo endereco passava por ele e tomava os filhos
novos; onde a corrida punha os widgets dependia do que o malloc devolvia, e a verificacao "the same seed gives the same pictures"
falhava (sementes 5 e 6, sempre) assim que o ComboBox::sizeHint passou a medir os itens (so mexe no padrao de alocacoes). Agora o
`forget()` poe o `scratch_` a nulo antes de cada removeChild do teste. So teste.

## O que se verificou em 2026-11-18 (a ponta de codigo e e2138e5)

`tools/check.sh --build <dir> --jobs 3` completo, com `IGUI_CONTAINERS_DIR` em ct 34169bc e `IGUI_ZEN_PLATFORM_DIR` em zen_plataform
db76a13: PASS nostd (nada novo com std:: nem #include <...> em src, include e adapters desde origin/main), PASS release (40 de
40), PASS asan (ASan e UBSan, 40 de 40), PASS hash (os tres somas de bench/retained/draw.hash: editor 87b1fe59ac36adb1, as duas
vezes, e piano roll 7956122198be0670; o do editor mudou em 05b75ea e em e2138e5, ver acima, o do piano roll nao).

Benches (`igui_retained_bench`, origin/main 1a77b1f contra a ponta, a mesma fonte do bench dos dois lados, `taskset -c 2`, 6 corridas
alternadas de cada lado, minimo; a maquina estava com carga de outro processo parte do tempo, por isso so contam os minimos):
- frame ociosa: editor300 89,97 -> 89,01 us (-1,1%), editor3000 97,82 -> 97,05 (-0,8%); update_only 0,03 -> 0,03; paint_only -0,9%;
  timeline300/3000 idle_frame -0,3% e -0,2%; as linhas de input (rato por cima de cada widget, teclas, escrever) +0,2% a +0,8%
  (alinhado: -0,1% a +0,3%); as de grelhas (datagrid, treegrid, propgrid) -0,2% a +2,1% (alinhado: -0,2% a +1,8%); as
  widget/*_per_widget -3,0% a +4,4% (alinhado -3,8% a +2,3%: ruido de posicao do codigo).
- mais rapido: editor*/layout_only 10,55 -> 7,40 us (-30%) e label_sizehints 2,58 -> 1,49 (-42%) (o Label que corta ja nao mede o
  texto comprido), layout_frame e resize_frame -4%.
- editor*/idle_frame_no_timeline, no_tl_no_side e no_tl_side_menu: +37%, +25%, +39%. NAO e abrandamento: e a cena. O bench esconde
  widgets com setVisible(false) e mede frames ociosas; na main o setVisible nao pedia layout, por isso a frame desenhava a mesma
  geometria sem o widget escondido; na ponta (item 9) o setVisible pede layout, os restantes ocupam o espaco que sobrou e a frame
  desenha mais coisas (o painel de propriedades cresce). Prova: com um `requestLayout()` + frame depois de cada setVisible(false)
  nos dois lados (variante do bench so para isto), a main da 83,0 / 36,1 / 31,1 us e a ponta 83,8 / 36,6 / 31,7 (+1,0%, +1,4%,
  +1,9%). O bench oficial nao mudou (a fonte tem de ser a mesma dos dois lados).
- editor*/text_width_x100: 16,0 -> 17,6 us (+10%), repetido com os dois lados recompilados com `-falign-functions=64 -falign-loops=64`
  (16,05 -> 17,69, +10%) e com `-falign-jumps=64 -falign-labels=64` tambem (16,72 -> 18,72, +12%): sobrevive ao alinhamento, mas
  nao e codigo novo: o codigo do caminho (textWidth, calcTextSize, findGlyph) nao mudou; num Harness vazio os dois lados dao o mesmo
  (16,0); a bisecao por commits mete-o em 05b75ea (so a parte do Label::sizeHint basta: +8%; so o ComboBox +2%; so o setElide 0);
  e na c3c3dc8 (nada meu), 400 bytes de NOP no fim de BasicWidgets.o (um objecto que nao tem a ver com o texto) levam a mesma
  linha de 15,8 para 18,5 (+17%) sem mudar uma linha de codigo, e 0, 32, 64 e 96 bytes dao 15,8: a linha e sensivel a posicao do
  codigo num grao que estas opcoes nao controlam. Nenhuma linha de frame (idle_frame, paint_only) mexe por causa disto.
  text/elided_8000_chars_frame +3,0% a +3,6% e o mesmo ciclo (mede um texto de 8000 letras) diluido.
- custo das mutacoes do item 9 (micro-medida fora do bench, minimo de 3 corridas): setText 6 -> 8 ns, setVisible 0 -> 3 ns,
  createChild + removeChild 110 -> 120 ns; a frame a seguir a um setText custa +7 us (o layout que passa a fazer-se sozinho,
  que o bench `layout_only` da por 7,4 us; antes a aplicacao chamava `requestLayout()` ou nao tinha layout).
- ficaram por fazer as cenas novas para os hints de Label e Button e para menus (o bench so cobre o Label no layout_only e no
  label_sizehints).

ZenDAW e ZenVideo (copias em scratch: rsync do projecto sem build, `third_party/iGUI` trocado pela arvore de origin/main e da ponta;
Release, `-j3`): os dois compilam e ligam contra a ponta sem tocar no codigo das aplicacoes. Corridos escondidos
(ZenDAW: `tools/run_hidden.sh --size 1280x800`, `--null-audio`; ZenVideo: `xvfb-run -a`, `ZENVIDEO_NULL_AUDIO=1`; HOME, XDG e
ZENDAW_DATA/ZENVIDEO_DATA em scratch) com `ZEN_SCREENSHOT` e `ZEN_SCREENSHOT_FRAME=60`:
- ZenDAW: o ecra de arranque (projecto vazio) e o do exemplo house.zdp: pixel a pixel iguais na main e na ponta (0 pixeis
  diferentes nas duas).
- ZenVideo (ecra de arranque, 1440x860): so mudam tracos, nada de largura nem de texto: a linha por baixo da barra de menus, o
  rebordo e os icones das barras de ferramentas (importar, tesoura, caixote, X, cadeado, +, -, o separador vertical), a linha por
  baixo dos botoes de transporte e a seta da ComboBox "Media files": mais finos e nitidos na ponta (item 10). Nao se viu o painel
  de propriedades com um clip escolhido (o ecra de arranque diz "No clip selected").

## Por verificar

- `tools/check.sh` e as benches foram feitos na ponta de codigo; falta repeti-los quando se fizer o resto do item 10 e o Stage B.
- ZenMix, ZenStage, animator, coco, deform, ZenHuman contra a ponta (so se leram os setElide do ZenMix e as constantes
  `kBarThickness`): o aspecto das polilinhas muda em todas.
- O ZenVideo com um projecto aberto e um clip escolhido (painel de propriedades com os Labels que cortam) e o ZenDAW noutras
  paginas que o arranque.

## Proximos passos, por ordem

1. Decidir o item 6 (espessura das barras): ver "Item 6: o que ficou de fora" acima. O ramo igui-round4-scrollbar-theme esta pronto
   para um merge ou cherry-pick se se aceitar o aspecto; o ramo igui-round4-wip (5f59819) fica obsoleto (os itens 3, 6a e 10
   passaram para a igui-round4 com os seus testes, a espessura para o ramo a parte) e pode apagar-se.
2. Resto do item 10 (rotulo da fatia pequena do pie/donut; nome do histograma na legenda: sugestao `Series N` para uma serie sem
   nome quando ha mais de uma, mais um acessor `legendLabel(i)` para testar).
3. Stage B por ordem (12 a 17, 19 a 21), um commit por item com o seu teste. Pontos a decidir: 13 (Menu::onMousePress vira o
   checked_ antes de emitir triggered: sinal `toggled(bool)` e `setAutoToggle(false)` por accao), 15 (popup com widgets: reutilizar
   showPopup com owned=false e o lendFocus), 16 (envolver o IO de WidgetApp::update: `feedMouseMove/Button/Wheel/Key/Text`).
4. Repetir `tools/check.sh` completo e as benches no fim de cada bloco; ver "Por verificar" para as aplicacoes que ainda nao se
   viram. Uma cena nova do bench para os hints de Label e Button e para menus (o `text_width_x100` e sensivel a posicao do codigo:
   ler sempre com a linha da mesma cena na c3c3dc8 antes de a levar a mal).
5. Mudancas de aspecto ou de comportamento por omissao ate agora (para as notas de cada aplicacao): CheckBox e cabecalho de
   Collapsible com visto (77c1ffe); Button mais largo quando o texto medido passa da estimativa e SplitButton 6 pixels mais largo,
   com o hash do editor mudado (37eb3ea); atalho do menu mais a esquerda nos itens com submenu (9074d03); Collapsible::sizeHint
   sem a largura do proprio rect (e1b816c); setFloatPos/setFloatSize actualizam logo os rectangulos (602345d); PianoRoll e
   WaveformView com botoes 0/1/2, e a ZenDAW tem de tirar a traducao em `Roll::onMousePress` (8dea1b8); layout automatico
   (2bf456d, e5ba9a7); ImageButton desactivado esbatido (e8e406e); Chart::formatValue sem %.3g (57941ab); Label com a largura
   medida e o Label que corta com so as reticencias como hint (05b75ea); ScrollView com o conteudo tao largo como pede quando a
   barra horizontal aparece (43743db); polilinhas abertas e linhas de dois pontos com a espessura pedida e nao o dobro (e2138e5:
   icones e rebordos mais finos em todas as aplicacoes).
6. O que a ZenDAW pode apagar quando os itens estiverem feitos: a traducao de botoes do `Roll::onMousePress` (ja), os
   `requestLayout()` a mao em `App::refreshAll`/`showPage` (ja, com o item 9), e depois dos itens 17, 19: `PageRail` e `TileBar`
   (icones da aplicacao e Theme na SideTabBar/ToolBar) e o `RollPane::pushNotes` (sinais com o indice e a velocidade por nota).

## Notas para os itens por fazer (o que se apurou a ler o codigo)

- 3 (feito, 05b75ea): `Label::sizeHint` mede o texto (so cai na estimativa onde nao ha fonte); o Label que corta pede no maximo as
  reticencias; `ComboBox::sizeHint` mede o item mais largo com a fonte do widget e guarda o resultado (largura, fonte e corpo da
  fonte da conta; o `setFont` conta). Falta, se se quiser a regra completa do item 9: o `setItemText`/`setFont` do ComboBox e o
  `setFont` dos outros widgets nao pedem layout (o setFont so marca sujo).
- 6 (a parte do ScrollView feita, 43743db): `ScrollView::layout` mede o conteudo a largura da vista e arruma-o tao largo como pede
  quando a barra horizontal aparece. A espessura das barras esta no ramo igui-round4-scrollbar-theme.
- 9 (feito): a regra esta em `Widget::invalidateLayout()` (comentario em Widget.hpp). Faltam, se se quiser a regra completa,
  os `setText` e afins de outros widgets cujo hint depende do texto (RadioButton, Switch, ComboBox::addItem, Tabs...): cada
  um e uma linha `invalidateLayout();`. Os widgets que mostram e escondem um filho sem que ninguem o posicione (overlays)
  marcam `layoutNeutral_` (membro protegido), como o grabber do SpinSlider; sem isso cada entrada do rato pedia layout.
  O teste de frame ociosa (test_retained_idle) conta as passagens de layout na janela do editor.
- 11 (feito): ver o commit f79a949 e o comentario da classe Menu em MenuWidgets.hpp. Aplicacoes do dono com setSubmenu: coco,
  deform, ZenHuman, ZenStage (src/app/MainWindow.cpp), todas com `setSubmenu(recentMenu_.get())` e o submenu em ct::unique_ptr
  da aplicacao (o ZenStage tambem `clipTransition_->setSubmenu(transitionMenu_.get())`): por isso a posse por omissao nao
  mudou. Nota sobre RTTI/UBSan no README (opcao IGUI_NO_RTTI).

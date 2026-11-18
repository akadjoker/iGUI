# Ronda 6 (ramo igui-round6, a partir de igui-round5 8dfffbc)

Fecha o que as rondas 4 e 5 deixaram aberto, para a biblioteca se poder dar por acabada para as aplicacoes que a usam: o custo por frame das
barras de rolagem, o desenho das notas do PianoRoll, um tratador que tira o proprio widget da arvore, a lista da zen_ui, as barras do
FileDialog e do AssetBrowser, os ids da ToolBar no serializador, o ZenVideo contra esta ponta e o `-fsanitize=float-cast-overflow`. Um item por
commit (o da zen_ui noutro repositorio), cada um com o seu teste: `tests/test_retained_round6.cpp` (alvo `igui_retained_round6`, com o malloc
embrulhado como o da ronda 5; os casos hostis do PianoRoll ficaram no `test_retained_round5.cpp`, onde ja estavam). Regras da ronda: sem
`std::` nem cabecalhos da biblioteca standard em codigo novo (a contagem de `std::` por ficheiro nao subiu em nenhum), nada abrandar, o hash do
desenho nao muda, tudo em scope com MemoryMax=4G nos CPUs 8 a 11, nenhuma janela no ecra (Xvfb privado, HOME e XDG em scratch).

Como construir e correr (a partir da raiz): `IGUI_CONTAINERS_DIR=<ct 34169bc> IGUI_ZEN_PLATFORM_DIR=<zen_plataform db76a13> tools/check.sh --build <dir> --jobs 3 --base 8dfffbc`.

## Estado

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 1 | Custo do `ScrollBar::thickness()` por paint | feito: ListBox, ListWidget, ScrollView, TextEdit, ConsoleWidget e TreeView guardam a espessura no layout | 5f19bdd | round6 testPaintTakesTheBarsRoomFromTheBar |
| 2 | PianoRoll: notas limitadas pelos pixeis do ecra | feito | d7ee8e2 | round6 testPianoRoll* (5), round5 testHostileManyNotes (alargado) |
| 3 | Um tratador que tira o proprio widget da arvore | feito para PianoRoll, ListBox, ListWidget, Slider; Button e Menu ja serviam; regra escrita | 0d97d2d | round6 testHandlerDeletes* (5) |
| 4 | zen_ui: a ultima linha de um listbox por baixo do painel | feito, no repositorio da zen_plataform | 15c4867 (ramo igui-round6-zen-ui da zen_plataform) | test_widgets.c test_listbox_stays_in_panel |
| 5 | Barras do FileDialog (6) e do AssetBrowser (4) | nao seguem o Theme, com a razao no codigo; so comentarios | aa705a1 | |
| 6 | Ids da ToolBar no serializador | feito: `ToolBar::addItemWithId` | 732b6a9 | round6 testToolBar* (2) |
| 7 | ZenVideo contra esta ponta | feito: zero pixeis diferentes nas quatro cenas; a barra do TextEdit 12 -> 8 vista a parte | (nao tem commit: so se mediu) | |
| 8 | `-fsanitize=float-cast-overflow` | feito: 12 avisos da suite (16 com o caso novo), todos corrigidos | f7dc0ac | |

## 1. Espessura das barras: o paint ja nao pergunta ao Theme

A ronda 5 pos `ScrollBar::thickness()` (17 instrucoes: a guarda do `Theme::instance()`, a leitura e o `Math::sane`) no paint do ListBox, do
ListWidget, do ScrollView e do TextEdit, e nas barras de desenho proprio do ConsoleWidget e do TreeView. No ListBox o GCC fazia a conta mesmo
sem barra (a conta sai da escolha por ser sem efeitos): +27 instrucoes por ListBox numa frame ociosa.

- ListBox, ListWidget e ScrollView guardam o espaco que a barra ocupa (a espessura e os 2 pixeis de folga) em `barRoom_`, posto no layout
  (`ListBox::layout`, `ListWidget::layout`, `ScrollView::updateBars`); o paint, o `itemRect` e o clip leem esse valor. A leitura vem DEPOIS de
  pintada a barra (no `if (vbar_ && vbar_->needed()) { vbar_->paint(ctx); viewW -= barRoom_; }`): lida antes, o compilador guardava um registo
  atras da chamada virtual e a frame ficava em 696596 (e com tudo no paint, +5400).
- TextEdit (`layout()` novo), ConsoleWidget (o `layout()` que ja tinha) e TreeView (`layout()` novo) guardam `barW_` da mesma maneira. Antes do
  primeiro layout o valor e 8, a constante de sempre.
- Fica como estava, de proposito: `ThumbnailGrid::metrics()` (pergunta so quando a barra se mostra e corre ao lado um layout de todos os itens),
  `Timeline::layout`, DataGrid e TreeGrid (`colBarW_` ja era assim).
- Mudanca de comportamento: uma espessura nova do Theme toma-se no layout seguinte (`requestLayout()`) tambem no TextEdit, no ConsoleWidget e no
  TreeView (antes seguiam o Theme no paint seguinte; o Theme.hpp ja dizia que era no layout); e no ListBox, ListWidget e ScrollView o conteudo
  e a barra concordam entre uma mudanca do Theme e o layout (o clip seguia o Theme novo e a barra ficava onde o layout a pos). O
  `testScrollBarThicknessIsTheThemes` da ronda 5 faz agora `relayout()` depois de mudar o Theme para os tres widgets; e a unica mudanca a esse teste.
- `sizeof`: ListBox, ListWidget e ScrollView 720, 720 e 672 -> 728, 728 e 680 (um float e o enchimento); TextEdit, ConsoleWidget e TreeView
  (1296, 1000, 1408) e PianoRoll (1224) ficam iguais (o float cabe no enchimento).
- Instrucoes (contador por ptrace passo a passo, `scratchpad/ig6/ic`, uma frame ociosa, CPU 8): 200 ListBox de um item (a linha `widget/listbox_per_widget` do banco): 695996 na 1529d73, 701396 na 8dfffbc, **695996 na ponta**
  (o `ListBox::paint` 175 por widget como na 1529d73); uma janela com ScrollView, ListBox, ListWidget, dois TextEdit, ConsoleWidget e TreeView, todos com a barra a
  mostra: 328249, 328406 e 328262 (`ScrollBar::thickness()` 119 -> 0; as 13 que sobram sao a leitura de um membro em vez de uma constante no ConsoleWidget e no TreeView);
  a frame ociosa da janela do editor do banco, 300 clipes: 973805, 973832 e 973805.
- Teste, round6 `testPaintTakesTheBarsRoomFromTheBar`: com o Theme a 14 e a espessura mudada para 20 sem layout, o realce da linha escolhida do
  ListBox e do ListWidget, o clip do conteudo do ScrollView (a direita e em baixo) e o desenho das barras do TextEdit, do ConsoleWidget e do
  TreeView ficam onde o layout os pos, e o layout seguinte toma os 20. Visto: com o src da 8dfffbc 7 verificacoes falham; com o novo passa.

## 2. PianoRoll: as notas limitadas pelos pixeis do ecra

`paintNotes` desenhava todas as notas que cabiam no tempo e nos tons, seis rectangulos cada (24 vertices): 200000 notas a vista davam 4,8 milhoes
de vertices e 89 MiB de pedidos ao heap numa frame; a faixa de velocidades fazia o mesmo com as suas barras (1,6 milhoes).

- So se olha para as notas cuja linha chega a parte visivel (o clip do desenho, dentro da grelha e acima da faixa) e cujo tempo chega a ela.
- Um rolo com mais notas do que metade das celulas que mostra (uma celula = uma coluna de pixeis de uma linha de tom, ate 4096 colunas)
  escolhe primeiro as notas que chegam a parte visivel (so os indices): se essas ainda sao metade das celulas ou menos (um rolo de cem mil notas com
  duzentas a vista) desenham-se uma a uma, como sempre (`loops/pianoroll_100000_notes_out_of_view`: 305 us contra 304 no banco; a primeira versao, com a
  tabela de celulas logo no inicio, dava 399, +31%); so se forem mais e que desenha o que as notas cobrem: as que comecam na mesma celula sao uma (do primeiro inicio ao ultimo fim, a velocidade mais alta), as celulas
  que se tocam numa linha sao uma corrida, e uma corrida desenha-se como uma nota. No maximo uma nota por cinco pixeis de uma linha; a memoria de
  um desses quadros e uma tabela de 12 bytes por celula (no maximo 520192 celulas, 6 MB), liberta no fim.
- A faixa, com mais de duas notas por coluna de pixeis, desenha uma barra por coluna (a mais alta: as barras tem 3 pixeis, o que se via era as
  barras umas por cima das outras).
- A nota que se arrasta desenha-se sempre como nota, por cima.
- O rolo normal desenha-se como sempre (3000 notas em 1240 x 700 com 60 linhas sao 3000 de 37000 celulas de metade): os quatro hashes de
  `bench/retained/draw.hash` iguais. A decisao e as linhas de tom visiveis ficam numa funcao fora de linha (`notePitchesToDraw`) para o ciclo das
  notas nao perder registos: com tudo no `paintNotes` a frame do rolo de 400 batidas dava +5775 instrucoes, e com so a conta das linhas +994; agora 824751 contra 824939 (8dfffbc) e 824933 (1529d73), e a do rolo de 32 batidas 266955 contra 267008 e 267002.
- Mudanca de desenho, so no caso denso (mais notas do que metade das celulas): as notas do mesmo sitio fundem-se; um rolo de 3000 notas
  pequenas num rolo de 364 pixeis deixa de mostrar o bordo de cada uma. Em rolos mais baixos do que as suas linhas as notas das linhas de fora
  ja nao geram vertices (estavam cortadas).
- Testes: round6 `testPianoRoll*` (cinco), e o hostil da ronda 5 (`testHostileManyNotes`) passa a desenhar frames com 200000 notas a vista (antes
  so eventos): o teto de 200000 vertices e de 64 MiB de heap, a faixa ligada e desligada, a velocidade a mostra, 200000 notas compridas, meia
  batida a vista com uma nota arrastada. Visto: com o src antigo o programa pára no teto (`one frame asked the heap for more than 64 MiB (89 so
  far)`, saida 3); com o novo passa. O banco ganha quatro linhas (`loops/pianoroll_3000_notes_32_beats`, `pianoroll_200000_notes`,
  `pianoroll_200000_notes_lane`, `pianoroll_100000_notes_out_of_view`).

## 3. Um tratador que tira o proprio widget da arvore

O que a biblioteca promete: `Widget::removeChild` apaga o widget quando acaba a fase `update()` ou `paint()` em curso (ou logo, se nao ha nenhuma:
`WidgetApp::deleteLater`), e `WidgetApp::deletions()` diz se algo se apagou durante um sinal (o `Widget::onMousePress` e companhia usam-no). Um
`delete` de um widget que ainda esta no pai nunca serviu (fica um ponteiro solto em `children_`: o `hitTest` rebenta), por isso o caso e `removeChild`.

- Visto: dentro de uma fase a memoria ainda la esta, mas o widget continuava a emitir (um PianoRoll de que o `onNoteAdded` tirou da arvore
  emitia o `onNoteCreated`; o `onNoteRemoved` seguia para `removeNote` e o `onNoteDeleted`); FORA de uma fase (a aplicacao a chamar
  `list->setSelectedIndex()`, `slider->setValue()`) o `removeChild` apaga logo e o widget lia memoria libertada: `ListBox::setSelectedIndex` e
  `ListWidget::setSelectedIndex` (`markDirty` depois do sinal; no teclado o `scrollToItem`), `Slider::setValue` (`fireEvent` e `markDirty`) e
  `Slider::onMousePress`, `PianoRoll::onMouseRelease` e o ciclo de `endLaneGesture`, e o `onNoteRemoved` (`removeNote` depois do sinal).
- `WidgetApp::hasGone(w, seen)` (novo, em linha, sobre o que ja havia): o contador mudou desde `seen` e `w` ja nao esta na arvore que se desenha;
  so e comparado, nunca lido. Os quatro widgets chamam-no a seguir ao sinal e nao vao mais longe (nem emitem nem tocam em membros) quando diz que
  o widget se foi; um tratador que tira OUTRO widget da arvore nao para nada. No PianoRoll o estado do gesto volta ao repouso antes do primeiro
  sinal do `onMouseRelease`. Custo: uma comparacao do contador por sinal.
- Button e Menu ja serviam (visto: a mesma bateria no src antigo passa neles). O Menu nao esta num pai: mostra-se como popup (`exec`) e o dono
  apaga-o; a accao tem a flag `gone_`.
- A regra, em `WidgetApp::hasGone` e nos cabecalhos dos sinais do PianoRoll, ListBox, ListWidget e Slider: tira-se o widget do pai com
  `parent->removeChild(w)`; os widgets que correm um tratador e depois ainda tem que fazer perguntam `hasGone`; os sinais dos outros widgets
  sao emitidos por codigo que pode ainda ler o widget a seguir: ai o tratador pede para se fazer depois dos eventos (`WidgetApp::callDeferred`).
  Nao se leram os outros (ComboBox, SpinBox, TextInput, Timeline, ...).
- Testes: round6 `testHandlerDeletes*` (cinco), cada caso dentro de uma fase (eventos por `feedMouse*`) e fora dela (o widget chamado a mao).
  Visto: sob ASan com o src antigo o programa pára com heap-use-after-free em `PianoRoll::onMouseRelease`, `ListBox::setSelectedIndex` e
  `Slider::setValue`; com o novo passam todos.

## 4. zen_ui: a ultima linha de um listbox por baixo do painel (repositorio zen_plataform)

O `zui_listbox` pos o clip da lista (`ui->clip = clip`) sem o cortar pelo clip em vigor (o do grupo ou da janela em que a lista esta). Uma lista
mais alta do que o espaco do painel (uma celula de uma linha e uma dica de seis linhas) desenhava as linhas que passam do fundo por baixo do painel,
e um clique nessa parte seleccionava a linha. Reproduzido numa janela escondida (Xvfb privado, exemplo de scratch `scratchpad/ig6/dbg/zui_list_shot.c`: um
grupo de 180 de altura, uma celula de 28 e uma dica de 6): antes a linha "Zeta" desenhava-se por baixo do painel; depois nao
(`scratchpad/ig6/shots/zp/list_before.png`, `list_after.png`; 148 pixeis diferentes, todos na linha). Os exemplos `demo_widgets_pixel` e
`demo_dock_pixel` ficam pixel a pixel iguais. O commit esta no ramo `igui-round6-zen-ui` da zen_plataform (15c4867, em cima de db76a13; o zen_ui nao
mudou ate a main beb1db0); teste novo `tests/test_widgets.c test_listbox_stays_in_panel` (antes 4 verificacoes falhavam, agora 307 passam).
Nao se mexeu no iGUI. As quadros de teste do adaptador do iGUI (`test_zen_input`, a ListBox retained de 300 itens) mostram a ultima linha cortada no
fundo da lista, como deve ser (`scratchpad/ig6/shots/zp/zen_list_before.png`): nao e esse o defeito.

## 5. FileDialog (6) e AssetBrowser (4): ficam, com a razao no codigo

Nenhuma das duas e um ScrollBar: e um indicador desenhado por cima do fim das linhas ou dos itens (um polegar de 4 pixeis numa tira de 6, com um quarto da
opacidade, no FileDialog; um polegar de 4 pixeis a 2 do bordo, no AssetBrowser), sem calha, sem espaco reservado (o conteudo corre por baixo) e sem
nada que se prima (a roda e o teclado e que rolam; `hitIndex` e `onMousePress` nao conhecem a barra). A largura de um indicador e o que tapa do
conteudo, nao uma area de pressao: a 8 cobria o fim da coluna da data e mudava o aspecto de omissao de duas janelas que nada pediram. So se acrescentou um comentario
em cada sitio. A alternativa, se o dono quiser: a espessura ser uma fraccao da do Theme (6/8 e 4/8: o mesmo desenho a 8 e a seguir o Theme).

## 6. Ids da ToolBar no serializador

```cpp
int id = bar->addItemWithId(7, ToolBar::ItemType::Toggle, -1, IconId::Gear, "Bold", true);   // 7 se estiver livre, senao o seguinte
json j = WidgetSerializer::save(root);   // cada item sai com o seu "id"; carregado, volta com os mesmos, tambem depois de itens tirados ou arrastados
```

`addItemWithId(id, tipo, celula, icone, dica, marcado)`: fica com o id pedido se e de 1 a 2^30 e nenhum item o tem; senao leva o seguinte e devolve o
que levou; o seguinte do `addButton` e dos outros continua depois do maior. Os ficheiros sem `id` (os de antes) carregam com 1 por diante pela ordem da
lista, como sempre; um `id` que nao e inteiro ou esta fora de 1 a 2^30 conta como em falta; repetido: o primeiro fica com ele. O serializador escreve sempre o
`id` (escrever so os que diferem punha ids a sorte quando um item sem id vem depois de um com). Testes: round6 `testToolBar*` (dois); visto: com o src da ronda 5
6 verificacoes falham (so o teste da API nao compila).

## 7. ZenVideo contra esta ponta

Copias em `scratchpad/ig6/zv` (rsync do projecto sem a pasta de build e sem `.git`, `third_party/iGUI` ligado a cada arvore: a 1529d73 `ig5/base`, a 8dfffbc
`ig5/wt` e a ponta `ig6/wt`; a pasta do dono nao foi tocada). Release com o LTO dele, `-j3`; corridos escondidos por `xvfb-run -a` (Xvfb proprio),
`ZENVIDEO_NULL_AUDIO=1`, `ZEN_SCREENSHOT` e `ZEN_SCREENSHOT_FRAME=60`, HOME, XDG e `ZENVIDEO_DATA` em scratch, com o tamanho da janela posto no
`settings.json` do scratch. Quatro cenas: o ecra de arranque a 1440x860 (o tamanho dos retratos da ronda 4) e a 1000x600, e as mesmas com uma pasta de 32 clipes de
2 s (`ffmpeg testsrc` + `sine`, 160x90; a lista de media com a barra, a timeline cheia). **Zero pixeis diferentes nas quatro cenas, entre a 1529d73, a 8dfffbc e
a ponta** (`ppmdiff`). Nenhuma das cenas mostra um TextEdit com barra (o do painel de propriedades so se ve com um titulo escolhido e o das definicoes depois de um
clique: um ecra sem entrada nao os abre), por isso a barra do TextEdit de 12 para 8 vi-a a parte, num programa de scratch com a rasterizacao por software de
cada arvore (`te_shot.cpp`: um TextEdit de 60 linhas e um ListBox de 40, a 520x220): entre a 1529d73 e a ponta diferem **437 pixeis, todos no TextEdit** (76 no polegar
vertical, que passa de 12 para 8 pixeis de largura, colunas 238 a 241, e 361 no horizontal, 43 a 131 por 148 a 158, que fica 4 pixeis mais baixo); a 8dfffbc e a
ponta sao iguais (0), e o ListBox, ao lado, nao muda. Retratos: `scratchpad/ig6/shots/zv/te_compare_zoom.png` (esquerda 1529d73, direita a ponta, ampliado
3x), `tip_media_1440x860.png`, `tip_start_1000x600.png`. Nao se correu o ZenDAW (outro agente trabalha nele).

## 8. `-fsanitize=float-cast-overflow`

Uma construcao com `-fsanitize=address,undefined,float-cast-overflow` (o grupo nao esta no `undefined` do GCC nem no `check.sh`) e a suite toda com `ctest -V` (o
ctest sem `-V` nao mostra a saida de um teste que passa, e o grupo nao pára o programa): 12 avisos, todos float NaN para int (que em x86 da INT_MIN e o
codigo a seguir tratava por sorte), em ThumbnailGrid, AssetBrowser, Splitter, PowerBar, DigitalSpeed e Chart. `Math::toInt(v, keep, lo, hi)` (novo): NaN da `keep`,
fora do intervalo fica no extremo, dentro corta para zero. O balde do histograma (o unico num ciclo quente) nao usa a funcao: limita a conta em double com dois seleccionadores (minsd e maxsd, sem ramo de dados) antes de a converter;
medido, `chart/hist_100k_rebin` 388,5 contra 380,3 us (6 corridas alternadas, fixo, -2,1%; alinhado -2,0%); uma primeira versao com `if (pos >= 0.0) ...` saiu +27% numa corrida de quatro e trocou-se
(a linha tem 40 a 70% de variacao entre corridas). Depois: a suite outra vez, `46 de 46, 0 avisos em `ctest -V` e nenhum erro do ASan nem do UBSan`. Teste: round6 `testToIntCasts` e
`testWidgetsMeetNumbersThatAreNotNumbers`; visto: com o src antigo e a construcao com float-cast-overflow o caso dispara 16 avisos, com o novo nenhum.
Fica por fazer: os sitios que a suite nao alcanca (uma leitura de `static_cast<int>` de float nos widgets que nao tem teste hostil).


## Verificacoes

Sobre a ponta de codigo f7dc0ac (o commit das notas so acrescenta este ficheiro). Dependencias: containers 34169bc e zen_plataform db76a13 (o iGUI) ; CPUs
(os scopes de MemoryMax=4G, `-j3`, sem janela no ecra: Xvfb proprio, HOME e XDG em scratch).

**`tools/check.sh --base 8dfffbc --jobs 3`** (correu como programa, `scratchpad/ig6/chk3`): `PASS nostd` (nada novo com `std::` nem `#include <...>` em src, include e adapters desde 8dfffbc), `PASS release`
(46 de 46), `PASS asan` (ASan e UBSan, 46 de 46), `PASS hash` (os quatro valores de `bench/retained/draw.hash`, nenhum regravado: editor 87b1fe59ac36adb1, piano roll
7956122198be0670, graficos 957c5363d01cdd4a); "4 stages, 0 failed". O `igui_zen_lifecycle` saiu `Skipped` ("no window here, skipped": o Xvfb da prova nao deu janela) nos
dois estagios e passou ao repetir (visto: 3 corridas seguidas no mesmo build, duas `Skipped` e a terceira `Passed`), como a ronda 4b e a 5 ja notaram.

**Contagem de `std::`** (linhas com `std::`, em 8dfffbc e na ponta, ficheiros mexidos): nenhuma subiu; desceram AssetBrowser.cpp 4 -> 3, AutomotiveWidgets.cpp 27 -> 25 e ThumbnailGrid.cpp
8 -> 7; AudioWidgets.cpp 68 e 68, ScrollWidgets.cpp 21 e 21, InputWidgets.cpp 21 e 21, ToolBar.cpp 12 e 12, WidgetSerializer.cpp 1 e 1, LayoutWidgets.cpp 12 e 12, TextInputWidgets.cpp 66 e 66,
ConsoleWidget.cpp 7 e 7, TreePropertyColorWidgets.cpp 129 e 129, FileDialog.cpp 31 e 31, ChartWidget.cpp 0 e 0; os cabecalhos e os testes novos 0.

**Bancada** (`igui_retained_bench`, 140 linhas: as 136 de antes mais quatro): 8dfffbc (copia em worktree, `scratchpad/ig6/base`) contra a ponta, o MESMO
`bench/retained/RetainedBench.cpp` dos dois lados (com as quatro linhas novas do PianoRoll; conferido: `diff` vazio), construcoes iguais (Release, so o banco, as
mesmas opcoes do CMake, `-ffile-prefix-map`), os quatro binarios dao o mesmo `--hash`, fixo ao CPU 10, corridas alternadas, minimo por linha. A maquina esteve ocupada por
outro agente durante a medicao (cocoshape, zendaw e um `limit_scale` no CPU 11, o irmao do 10: o script que espera por silencio desistiu aos 50 s), por isso a variacao entre
corridas e grande (10 a 70%) e so contam os minimos de varias.

O que se mediu e o que se le (as tabelas completas estao em `scratchpad/ig6/bench/runs`, `cmp.py`, `cmp2.py` e `cmpg.py`):

- **Instrucoes** (contador por ptrace passo a passo, o que nao depende do ruido): em todos os arneses a ponta tem as mesmas ou menos instrucoes que a 8dfffbc, e no ListBox, a
  da 1529d73: 200 ListBox 695996 / 701396 / 695996 (1529d73 / 8dfffbc / ponta); janela de barras 328249 / 328406 / 328262; frame ociosa do editor (300 clipes) 973805 / 973832 /
  973805; rolo de 400 batidas 824933 / 824939 / 824751; rolo de 32 batidas 267002 / 267008 / 266955.
- **Primeira passagem** (12 corridas alternadas, 6 de cada lado, maquina quieta, numa ponta anterior as duas ultimas correcoes do PianoRoll e do histograma): 9 linhas a mais de 3%:
  `editor300/layout_only` -3,5% e `editor3000/layout_only` -3,1% (mais rapidas; o layout so ganhou uma escrita de membro por ListBox/ListWidget/ScrollView),
  `bigtext/doc20000_type` -4,7% e `chart/bars_1000_frame` -5,6% (mais rapidas, codigo nao mexido), `loops/pianoroll_32_beats` +3,3% (ver abaixo), `loops/pianoroll_200000_notes` -91,7% e
  `pianoroll_200000_notes_lane` -90,9% (e o que se queria: de 25 ms e 34 ms por frame para 2,1 e 3,1 ms), e duas linhas que eram defeitos meus e se corrigiram: `pianoroll_100000_notes_out_of_view`
  +31,4% (o desenho denso alocava a tabela de celulas mesmo com poucas notas a vista: agora escolhe primeiro as notas que chegam a parte visivel; focada, 8dfffbc contra ponta, 10 corridas alternadas:
  308,8 e 328,6 us plain (+6,4%), alinhado 0,0%) e `chart/hist_100k_rebin` +40,5% (a primeira versao do balde com um `if` de dados; agora dois seleccionadores sem ramo: -3,0% plain, -1,8% alinhado,
  focada). Todas as outras linhas dentro de 3%.
- **Ponta final** (as duas correcoes feitas): a maquina estava ocupada por outro agente (cocoshape, zendaw e um `limit_scale` no CPU 11, o irmao do 10; nos CPUs todos 86 a 99% de uso quando se
  mediu), a variacao entre corridas de uma linha foi de 10 a 100% e o script que espera por silencio desistiu aos 50 s. Passagem completa, minimos de 16 corridas por lado: a ponta sai +5% a +14% mais lenta
  em quase todas as linhas, e no alinhado (6 corridas por lado) -4% a -10% mais rapida nas mesmas linhas, incluindo linhas em que as instrucoes sao as mesmas (a frame ociosa do editor): e o ruido
  (quem apanhou a corrida quieta), nao o codigo. As corridas focadas, alternando as duas construcoes de segundos em segundos (10 pares plain e 6 alinhados por grupo de linhas), onde o ruido cai: a frame
  ociosa do editor (`editor300/idle_frame*`, 8 linhas) +0,2% a +1,0%; `chart/hist_100k_frame` +0,1% e `hist_100k_rebin` -3,0%; o PianoRoll, as linhas de antes: `pianoroll_32_beats` +6,5% plain e +3,2% alinhado,
  `pianoroll_400_beats` +5,9% e +0,5%, `pianoroll_3000_notes_32_beats` +6,9% e +0,1%, `pianoroll_100000_notes_out_of_view` +6,4% e 0,0%: colocacao do codigo (o alinhado, com as funcoes e os ciclos a 64 bytes, faz
  quase tudo desaparecer, e as instrucoes por frame sao menos: 266955 contra 267008 e 824751 contra 824939), como a ronda 5 ja mostrou para esta linha (+-9% com 400 bytes de NOP num objecto
  que nao tem a ver); `pianoroll_200000_notes` -86,8% e `pianoroll_200000_notes_lane` -87,1% (alinhado -87,5% e -88,2%). O grupo `input/` (15 linhas) tem 75 a 90% de variacao entre as corridas da mesma construcao:
  nao se le.
- Outras corridas focadas (10 pares plain, 6 alinhados): `widget/listbox_per_widget` 0,29 e 0,29 us (0,0%, alinhado 0,0%: o que o item 1 devia dar); `text_width_x100` +1,2% e -0,3% (alinhado +0,3% e -0,2%);
  `timeline300/idle_frame` +1,6% (alinhado -0,3%) e `idle_frame_all_visible` +4,3% (alinhado -1,1%, variacao de 2%: a linha de que as rondas 4 e 5 ja disseram que anda com a colocacao do codigo;
  o Timeline nao foi tocado nesta ronda); `softraster/cached_frame` +5,0% (alinhado -1,5%; nao tocado, e a linha sensivel ao limiar do mmap); `chart/bars_1000_frame` -5,6% (alinhado -1,0%; nao tocado);
  `widget/checkbox_per_widget` +2,2% (alinhado +2,2%: um digito impresso de 0,01 us).
- Nao ha linha de bancada onde o rolo normal fique mais lento por codigo: o ciclo das notas do rolo normal e o mesmo ciclo de antes, com a decisao fora de linha (so a conta das linhas visiveis e uma chamada por frame).


## O que ficou de fora, e o que desconfio

- Item 1: `ThumbnailGrid::metrics()` e `Timeline::layout` ainda perguntam ao Theme (so onde a barra se mostra, ao lado de um layout de todos os itens); as barras do `DataGrid` e do `TreeGrid` ja
  eram do layout.
- Item 2: o limiar (metade das celulas, duas notas por coluna na faixa) e fixo, sem setter; com mais notas do que metade das celulas as notas do mesmo sitio fundem-se (o bordo de cada uma deixa
  de se ver). O desenho denso nao segue a ordem dos indices das notas (so a nota arrastada vai por cima). Uma roll com `viewStart_` e `viewEnd_` enormes tem as colunas contadas ate 4096 (as celulas ficam
  mais largas que um pixel). `onMouseMove` e o press continuam a percorrer todas as notas (O(n), sem desenho).
- Item 3: `hasGone` esta no PianoRoll, ListBox, ListWidget e Slider; nao se leram os outros widgets com sinais proprios (ComboBox, SpinBox, TextInput, Timeline, Tabs, Menu da barra, ...): o que
  fazem depois de um sinal em que um tratador tirou o widget da arvore nao esta provado. Um widget que nunca esteve numa arvore e a que um tratador apaga outro widget conta como tirado (`hasGone`
  compara com a arvore que se desenha). A regra diz `removeChild`; `delete` de um widget que esta num pai nunca foi permitido.
- Item 4: a correcao esta so na zen_plataform (o ramo igui-round6-zen-ui, que o dono tem de integrar la); as outras atribuicoes de clip directo do zen_ui (`zui_group_begin`, `zui_window_begin`:
  `ui->clip = bounds`) tambem nao cortam pelo clip em vigor; nao se mexeu (um grupo dentro de uma janela pequena pode sair da janela). O iGUI nao muda.
- Item 5: ver acima; a alternativa (fraccao do Theme) nao se fez.
- Item 6: so os ids; a folha de icones e as ligacoes dos sinais continuam de fora.
- Item 7: o ZenVideo so se viu no arranque e com 32 clipes importados (nenhum popup, nenhum TextEdit com barra, nenhum clipe escolhido); nao se correu o ZenDAW; o ZenMix, o ZenHuman e as outras
  aplicacoes nao se reconstruiram contra esta ponta.
- Item 8: o `-fsanitize=float-cast-overflow` nao esta no `check.sh` (a suite sem `-V` nao mostra os avisos); acrescenta-lo pede ligar o grupo e um `ctest -V` ou um `UBSAN_OPTIONS` que pare no
  primeiro. So se corrigiram os sitios que a suite alcanca.
- A bancada nao tem linha para a faixa de velocidades com 3000 notas ordinarias, nem para um arrasto de nota num rolo denso.


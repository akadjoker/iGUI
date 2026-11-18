# Ronda 5 (ramo igui-round5, a partir de fonts-gl-r4 1529d73)

Cinco itens pequenos que faltavam no PianoRoll, no serializador e nas barras de rolagem, mais os testes hostis para o codigo
novo. Um item por commit, cada um com o seu teste; os testes estao num ficheiro novo, `tests/test_retained_round5.cpp` (alvo
`igui_retained_round5`, entrada sintetica `WidgetApp::feedMouseMove/Button/Wheel`). Este ficheiro diz, por item, o estado, o
commit, o teste, a API que ficou (com um exemplo de tres linhas) e o que se escolheu onde havia decisao, com a alternativa, para
o dono poder desfazer. Regras da ronda: sem `std::` nem cabecalhos da biblioteca standard em codigo novo (a contagem de `std::` por
ficheiro nao subiu em nenhum), nada abrandar nem custar quando nao se usa, o hash do desenho do bench nao muda, tudo em scope
com MemoryMax=4G nos CPUs 8 a 11 (o bench no 10, com o 11, o irmao, parado), nenhuma janela no ecra (os testes novos sao sem janela:
rasterizador por software).

Como construir e correr (a partir da raiz do repositorio):
`IGUI_CONTAINERS_DIR=<ct em 34169bc> IGUI_ZEN_PLATFORM_DIR=<zen_plataform em db76a13> tools/check.sh --build <dir> --jobs 3 --base 1529d73`.

## Estado

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 1 | PianoRoll: o comprimento de uma nota com o rato | feito | 8aae043 | round5 testPianoRollResizeWithTheMouse, testPianoRollNewNoteTakesItsLengthFromTheDrag, testPianoRollNoteIndexStaysRightWhenANoteBeforeItGoes |
| 2 | PianoRoll: faixa de velocidades | feito | e79542f | round5 testPianoRollVelocityLane* (5) |
| 3 | Testes hostis para o codigo novo | feito (commit depois do 5: ver abaixo); achou e corrigiu um defeito | d06c7d3 | round5 testHostile* (7) |
| 4 | ToolBar no serializador | feito; ids e folha de icones ficam de fora | 71a41a9 | round5 testSerializerKeepsToolBar, testSerializerKeepsTheOrderOfAToolBarAfterADrag, testToolBarLoadsWhatAFileCanHold |
| 5 | Espessura das barras vem do Theme, com 8 por omissao | feito | 18a335e | round5 testScrollBarThicknessIsTheThemes |

A ordem dos commits e 1, 2, 4, 5, 3: a parte da espessura das barras do item 3 (Timeline e todas as barras) so se prova com o codigo
do 5, por isso o 3 vem depois dele.

Como se viu "falha antes, passa depois": para a API nova o teste nao compila sem o src (e o "falha antes" mais honesto); para ver
falhas em tempo de execucao pos-se de volta so o `.cpp` de antes (com o cabecalho novo, e stubs onde o cabecalho declara o que o
`.cpp` antigo nao define) e contaram-se as verificacoes que falham. Os numeros estao nas mensagens de commit
(`git log 1529d73..igui-round5`).

## 1. PianoRoll: o comprimento de uma nota com o rato

```cpp
roll->onNoteResized.connect([&](int i) { model.setLength(i, roll->notes()[i].length); });
// arrastar os ultimos pixeis da direita de uma nota muda-lhe o comprimento; o cursor passa a SizeH por cima
// um clique num sitio vazio continua a por uma nota de 0,25; clique e arrasto da-lhe o comprimento do arrasto
```

- Os ultimos 6 pixeis do lado direito de uma nota (no maximo um terco da largura, para uma nota curta continuar a poder mover-se)
  pegam no fim em vez de mover a nota. O fim segue o ponteiro a partir de onde foi agarrado (guarda-se a diferenca no press: um clique
  no fim sem mexer nao altera nada) e vai para a linha mais proxima da grelha de quartos de tempo; o comprimento nunca fica abaixo
  de um quarto, nem passa de 1e9, nem sai NaN; o inicio e a altura nao mexem.
- `onNoteResized(int)`: ao largar o botao, com o indice, so se o comprimento mudou, depois de `onNoteAdded` (que continua a sair depois
  de qualquer arrasto de nota). Uma nota posta agora diz `onNoteCreated`, com o comprimento que o arrasto lhe deu, nunca `onNoteResized`.
- Numa nota posta com clique e arrasto o resto do gesto da o comprimento e ja nao leva a nota a outra altura ou inicio (antes seguia
  o ponteiro). E a unica mudanca de comportamento do item (pedida).
- Achados de caminho: `removeNote` de um indice abaixo da nota arrastada deixava o arrasto a apontar para outra nota; `yToPitch` fazia
  `static_cast<int>` de um float qualquer; um press ou move com x ou y NaN/infinito agora e ignorado; um press que nao conseguiu por a
  nota (inicio que nao e numero) arrastava a ultima nota que la estava.
- Escolhas: o fim vai para a linha MAIS PROXIMA e nao para a de antes (o `floor` de mover): com `floor` e a diferenca do press guardada,
  um clique no fim com um movimento de um pixel para tras encurtava a nota um passo inteiro. A alternativa e `floor` como mover (e a
  nota de 0,25 que cresce so quando o ponteiro passa a linha). O passo e a zona de 6 pixeis sao constantes (0,25 beat, 6 px), sem
  setter (nao pedido); a alternativa e `setSnap(float)` e `setResizeEdge(float)`.
- Custo: o `onMouseMove` sem botao percorre as notas, e so mede as da linha do ponteiro (uma comparacao simples por nota, O(n)); o press ja percorria todas.
  PianoRoll cresce 48 bytes por instancia (o sinal, 40, mais `dragStartLen_` e `resizing_`).

## 2. PianoRoll: faixa de velocidades

```cpp
roll->setShowVelocityLane(true);  roll->setVelocityLaneHeight(80.0f);        // por omissao desligada, 60 pixeis
roll->onNoteVelocityChanged.connect([&](int i) { model.setVelocity(i, roll->notes()[i].velocity); });
// arrastar na faixa: a altura do ponteiro (topo 1, fundo 0) e a velocidade das notas debaixo dele
```

- A faixa e uma tira por baixo das notas, dentro da area do rolo (no maximo metade da altura do rolo, nenhuma se sobrar menos de 10 pixeis):
  as notas e as linhas de tom param onde ela comeca (as linhas de baixo ficam escondidas: poe-se `setPitchRange` mais curto), a coluna
  das teclas ao lado diz "Vel". Uma barra de 3 pixeis ao inicio de cada nota que esta a vista no tempo, qualquer que seja o tom, da altura
  da velocidade (2 pixeis no minimo), com uma guia a meio; as barras tocadas pelo gesto em curso saem na cor de uma nota arrastada. Segue
  o zoom e o deslocamento porque as barras saem de `beatToX`.
- Rato: o botao esquerdo poe a velocidade das notas cuja barra esta ao alcance do ponteiro (2 pixeis); arrastar pinta cada barra com a
  altura que o ponteiro tinha no sitio dela, seguindo o caminho entre dois eventos (linha recta: um rato rapido nao salta barras). So
  as barras a vista contam (o x do caminho e preso a grelha). Um acorde (notas com o mesmo inicio) leva-as todas. O botao direito na
  faixa nao faz nada; o do meio continua a deslocar a vista; um press na faixa nunca poe nem escolhe uma nota.
- Durante o arrasto: as velocidades em `notes()` sao ja as novas e o rolo repinta; NAO se emite nada (a aplicacao que quer o som ao vivo
  le `notes()`). Ao largar, `onNoteVelocityChanged(int)` uma vez por nota que acabou com outra velocidade da de partida, do indice mais alto
  para o mais baixo (um tratador que apaga a nota que lhe dizem nao desarruma os indices que faltam), cada um confirmado contra a lista.
- Escolhas: o gesto guarda a velocidade de antes por indice de nota (um vector de floats feito no primeiro toque, libertado ao largar) para
  saber o que mudou e para uma volta ao valor de partida nao dizer nada; a velocidade e o float 0..1 do rato, sem quantizar a 127 passos
  (a alternativa e quantizar na aplicacao); as notas fora da gama de tons tem barra (a faixa e do tempo); a alternativa para a area das
  notas era encolher as linhas para caberem acima da faixa em vez de as cortar; a alternativa ao silencio durante o arrasto era um sinal
  `onNoteVelocityDragged(int)` por nota e por movimento, que nao se fez (uma inundacao em 3000 notas).
- Custo: desligada, um ramo em `paint` e outro no press, nenhum desenho a mais (o hash nao mexe); PianoRoll cresce mais 88 bytes por
  instancia (o sinal, o vector, floats e bools; de 1088 para 1224 com o item 1; medido com `sizeof`); PianoNote continua com 16.

## 3. Testes hostis

Seis casos do PianoRoll e um das barras, com o limite proprio de vertices por frame (200000, como o `test_retained_hostile.cpp`) e o teto
de pedidos ao heap por frame (64 MiB, contado por um malloc embrulhado: o alvo liga com `-Wl,--wrap=malloc,calloc,realloc`): setters e
notas com NaN, infinitos, 1e30, 2^24, 2^31; o ponteiro enviado a esses valores a meio de cada gesto (resize, mover, por, faixa, deslocar,
apagar); notas e vistas enormes; tratadores que mexem nas notas a meio do gesto (ver a lista na mensagem do commit); rolos de tamanho 0,
negativo, 36 de largura (so as teclas) e 1e9; 3000 notas a vista e 200000 notas; `Theme::scrollbarWidth` hostil com todos os tipos de
barra na mesma janela (Timeline incluida).

- Achado e corrigido: mover uma nota com o ponteiro a 1e30 em x dava-lhe um inicio de 2,5e29 (codigo de antes; agora `Math::sane` 0 a 1e9).
- Os testes tem dentes: sem as guardas de ponteiro finito, sem a confirmacao do indice em `endLaneGesture` e com a ordem crescente dos sinais da
  faixa falham (cada alteracao desfeita so no src). ASan e UBSan: passa, sem mensagem.
- Nao cobre: um tratador que APAGA o proprio PianoRoll (o codigo de antes tambem nao).
- Os 200000 notas a vista sao so eventos, sem frame: o desenho de notas do rolo (que desenha todas as que estao a vista, sem corte por
  densidade) pede 89 MiB a uma frame com 200000 notas a vista; isso e de antes e nao se mexeu (alternativa: nao desenhar notas de menos de um
  pixel de largura, o que muda o desenho).

## 4. ToolBar no serializador

```cpp
json j = WidgetSerializer::save(root);                       // a ToolBar sai como "ToolBar", com as flags, os tamanhos e os itens
Widget* copy = WidgetSerializer::load(j, parent);            // e volta igual
```

Guarda: `coloredIcons` (so quando true), `bottomBorder` (so quando false), `draggable` (so quando false), `buttonSize`, `iconPadding`, `spacing`,
e a lista `items` pela ordem em que estao desenhados (um arrasto troca-a): tipo (Button, Toggle, Separator, Spacer), `cell` ou `icon` (so os
IconId da biblioteca sobrevivem, como na SideTabBar), `tooltip`, `enabled`, `checked`, `visible` e `draggable` do item (so os que nao sao os de
omissao). Ao carregar, os tamanhos ficam presos ao que uma barra usa. A ToolBar ganhou getters publicos so de leitura: `itemCount`, `itemIdAt`,
`itemType`, `itemCell`, `itemIcon`, `itemTooltip`.

Fica de fora: a folha de icones (imagem ou textura e o tamanho da celula: a aplicacao chama `setImageGrid`/`loadImageGrid` depois de carregar), as
imagens registadas em tempo de execucao, as ligacoes a `buttonClicked`/`toggleChanged`, e os IDS dos itens (ao carregar dao-se de 1 por diante pela
ordem da barra; uma barra a que se tirou um item ou que se reordenou tem ids que nao voltam iguais; a aplicacao acha os itens por `itemByTooltip`
ou pelo indice). Alternativa: guardar o id e repo-lo, com um `ToolBar::addItemWithId` novo (nao feito).

## 5. Espessura das barras: Theme::scrollbarWidth

```cpp
Theme::instance().scrollbarWidth = 14.0f;  app.requestLayout();     // todas as barras da biblioteca passam a 14
float t = ScrollBar::thickness();                                    // as barras de uma aplicacao seguem o Theme
// ScrollBar::kBarThickness continua 8 (comentado como obsoleto): o codigo que o le compila e le 8
```

- `Theme::scrollbarWidth` vale 8 por omissao e nas cinco predefinicoes (dark, light, xp, macos, macos-dark); `ScrollBar::thickness()` devolve-o preso a
  4..100 (NaN da 8). Seguem-no: ScrollView (as duas barras), ListBox, ListWidget, DataGrid, TreeGrid, ThumbnailGrid, Timeline, e (alem do que o ramo
  lateral 7a48f94 tocava) TreeView, ConsoleWidget e TextEdit. Do ramo lateral: ThumbnailGrid, grelhas, Timeline, as duas provas que assumiam 8.
  Ao contrario do ramo: as grelhas guardam a espessura do layout (`colBarW_`, na chave da cache de `ensureColumns`) por isso uma espessura nova se
  toma sem redimensionar; e o valor por omissao e 8 e nao 12, por isso nada muda de aspecto.
- UNICA mudanca de aspecto por omissao: a barra do TextEdit, que era 12 (10 e 16 nos temas macos e xp), passa a 8 como as outras (e o que sai de pedir 8
  em todas as predefinicoes). O hash do desenho nao a ve.
- Ficam de fora, de proposito, as barras finas de desenho proprio: FileDialog (6) e AssetBrowser (4). Mudar a espessura com widgets no ecra pede
  `requestLayout()` (como mudar `fontSize`); o Theme nao avisa ninguem.
- Aplicacoes do dono que desenham barras proprias com `ScrollBar::kBarThickness` (ZenMix: SongList e AboutPanel; ZenHuman: a reserva do AssetGrid em
  AppearancePanel) continuam com 8 e iguais; para seguirem o Theme passam a ler `ScrollBar::thickness()`.

## Verificacoes

Sobre a ponta de codigo d06c7d3 (o commit das notas so acrescenta este ficheiro). CPUs 8 a 11 (`taskset -c 8-11`), cada comando num scope de
MemoryMax=4G, `-j3`, sem janela no ecra, dependencias containers 34169bc e zen_plataform db76a13.

**`tools/check.sh --base 1529d73 --build <dir> --jobs 3`** (correu como programa): `PASS nostd` (nada novo com `std::` ou `#include <...>` em src,
include e adapters desde 1529d73), `PASS release` (45 de 45, 26 s), `PASS asan` (ASan e UBSan, 45 de 45, 220 s), `PASS hash` (os quatro valores de
`bench/retained/draw.hash`, nenhum regravado: editor 87b1fe59ac36adb1, piano roll 7956122198be0670, graficos 957c5363d01cdd4a); "4 stages, 0 failed".
Na 1529d73 eram 44 testes; o 45o e `igui_retained_round5`. O `igui_zen_lifecycle` saiu uma vez `Skipped` num `ctest` solto (xvfb-run que nao arrancou)
e passou ao repetir.

**Contagem de `std::`** (linhas com `std::`, em 1529d73 e na ponta, ficheiros mexidos): nenhuma subiu, e nenhuma linha nova de src, include ou adapters tem
`std::` nem `#include <...>`: AudioWidgets.cpp 68 e 68, AudioWidgets.hpp 0 e 0, ConsoleWidget.cpp 7 e 7, DataWidgets.cpp 39 e 39, DataWidgets.hpp 0 e 0,
ScrollWidgets.cpp 21 e 21, ScrollWidgets.hpp 2 e 2, TextInputWidgets.cpp 66 e 66, ThumbnailGrid.cpp 8 e 8, Timeline.cpp 85 e 85, ToolBar.cpp 12 e 12,
ToolBar.hpp 0 e 0, Theme.hpp 0 e 0, TreePropertyColorWidgets.cpp 129 e 129, WidgetSerializer.cpp 1 e 1, test_original_widgets.cpp 54 e 54,
test_retained_grid.cpp 0 e 0, test_retained_round5.cpp 0 (novo).

**Bancada** (`igui_retained_bench`, 136 linhas): 1529d73 (copia em worktree) contra d06c7d3, o MESMO `bench/retained/RetainedBench.cpp` dos dois lados
(conferido: `diff` vazio; o bench nao foi mexido), as duas construcoes iguais (Release, so o bench, as mesmas opcoes do CMake, `-ffile-prefix-map`), os
quatro binarios dao o mesmo `--hash`; fixo ao CPU 10 com o 11 (o irmao SMT, pelo `lscpu`) parado (um script espera que os CPUs 8, 9 e 11 estejam
quase parados antes de cada corrida), corridas alternadas, minimo por linha.
- Normal: duas passagens, 6 e 8 corridas alternadas de cada lado, minimos das 14. A primeira passagem apanhou a maquina ocupada (variacao entre corridas
  de 20 a 40% de cada lado, 78 linhas a mais de 3% com os dois sinais e sem padrao); por isso repetiu-se e contam as 14. Linhas a mais de 3%: seis.
  `timeline300/idle_frame_all_visible` -4,7% e `timeline3000/idle_frame_all_visible` -3,9% (mais rapidas; alinhado -0,4% e +0,5%); `widget/listbox_per_widget`
  0,28 -> 0,29 us (+3,6%, mas e um digito impresso, 0,01 us; repetida so ela, 16 corridas: a 1529d73 da 0,28 em 10 e a ponta 0,29 em 10; alinhada o minimo e 0,28 dos dois lados, em 6 e 5 corridas; ver a leitura); `bigtext/doc20000_type` +5,2%
  (variacao 33 a 39%; alinhado +1,2%); `loops/pianoroll_400_beats` +4,0% (repetida so ela, 12 corridas alternadas: +3,7% com variacao de 1 a 2,5%);
  `chart/bars_1000_frame` +3,9% (variacao 21 e 80%; alinhado +0,5%; codigo do Chart nao mexido).
- Alinhado (`-falign-functions=64 -falign-loops=64` nos dois lados, 6 corridas): quatro linhas a mais de 3%: `editor3000/layout_only` +3,0% (repetida so ela, 12
  corridas: editor300 +3,0% e editor3000 +3,5%), `widget/collapsible_per_widget` 0,44 -> 0,46 (um digito; codigo nao mexido), `widget/listbox_per_widget`
  0,28 -> 0,29 (um digito) e `chart/hist_100k_rebin` -5,8% (mais rapida; variacao 40 a 56%). `loops/pianoroll_400_beats` alinhada: 0,0% (e 0,0% nas 12 repetidas).
- Leitura (as duas que contam, `loops/pianoroll_400_beats` e `editor*/layout_only`, sao colocacao do codigo e nao codigo): (1) instrucoes de uma frame do
  rolo de 400 batidas (um contador por ptrace passo a passo: o `perf` nao deixa): 824933 na 1529d73 e 824939 na ponta (+6, 0,0007%); instrucoes de uma
  passagem de layout da arvore do editor: 83371 e 83410 (+39, 0,05%: as duas chamadas de `ScrollBar::thickness()` do ListBox e do Timeline, uns 3 ns cada);
  o tempo da `layout_only` mexe 1,4 a 3,5%, 100 a 200 ns, que 39 instrucoes nao dao. (2) a 1529d73 sozinha com 16, 32, 48, 64, 96 e 128 bytes de NOP
  acrescentados a BasicWidgets.o (um objecto sem relacao): o minimo de `editor300/layout_only` anda entre 6,29 e 6,55 us (4,1%) e o de `editor3000/layout_only`
  entre 6,25 e 6,54 (4,6%; 6 corridas de cada variante, a 1529d73 sem NOP da 6,40 nas duas), e a ponta (6,50 nas duas) cai dentro dos dois intervalos. (3) a comparacao de bytes das funcoes (`funcdiff`, as duas construcoes normais): 1533
  funcoes com bytes diferentes (o que vem depois do primeiro objecto mexido anda uns 0x30 e os enderecos relativos mudam), das quais 52 tem outro
  numero de instrucoes, todas em unidades de compilacao que a ronda tocou: as funcoes que mudaram (PianoRoll: construtor, addNote, removeNote, clearNotes,
  onMousePress/Move/Release, paint, yToPitch; DataGrid e TreeGrid: `ensureColumns` e `paint`; ScrollView, ListBox e ListWidget: `layout`, `paint`, `itemRect`,
  `placeRow`, `updateBars`; `ScrollBar::sizeHint`; `Timeline::layout`; `TreeView::scrollbar`; `ConsoleWidget::paintScrollbar`; os getters da ToolBar) e outras que o GCC
  decidiu de outra maneira nas mesmas unidades (os destrutores de Fader, Knob, LinearKnob, ADSRWidget e WaveformView e as instanciacoes de `Signal::emit` em
  AudioWidgets.cpp, o lambda de ordenacao do DataGrid, `TextEdit::mergeWithPrevLine`, `Timeline::envelopeKeyAt` e `insertTrack`). Na frame ociosa do editor o que
  muda de codigo e o `ListBox::paint` (+16 instrucoes) e a chamada a `thickness()` no `TextEdit::paint`; no Timeline e nos graficos nada. Nao ha abrandamento
  real: o custo do que se acrescentou e `ScrollBar::thickness()` (uma chamada, uns 3 ns, onde a barra se usa) e, no PianoRoll com a faixa desligada, um
  ramo por frame.
- A terceira linha a ler e `widget/listbox_per_widget` (200 ListBox de um item, uma frame ociosa): a barra e precisa em todos, por isso o `ListBox::paint` chama
  `ScrollBar::thickness()`: o contador de instrucoes da frame da 695996 na 1529d73 e 701396 na ponta, +5400, +27 por ListBox (+0,8%): 17 da chamada (200 chamadas) e 10 do
  proprio `paint` (histograma das instrucoes por funcao, pelo mesmo contador); no tempo 0,28 contra 0,29 us normal (16 corridas) e 0,28 contra 0,28 alinhado (16 corridas): o custo real e
  esse, uns 2 ns por ListBox com a barra precisa, e o resto e colocacao. Nao se mexeu: a alternativa era o `paint` ler a largura que o layout deu a barra
  (`vbar_->rect().w`) em vez de chamar `thickness()`, o que tambem o faria coerente com o layout se o Theme mudar sem layout; nao se fez porque o `placeRow` do
  ListWidget corre antes do primeiro layout e mudava a largura das linhas nesse tempo.
- Mais rapido ou igual: nenhuma linha de frame (editor, timeline, entrada, grelhas, graficos) ficou a mais de 3% mais lenta nas duas construcoes ao mesmo
  tempo; as alocacoes por iteracao sao iguais em todas as linhas. Tabelas completas: `scratchpad/ig5/bench/runs` (`cmp.py`).

**Nao se correu**: as aplicacoes do dono (ZenDAW, ZenVideo, ZenMix, ZenHuman...) contra esta ponta; `-fsanitize=float-cast-overflow`; a bancada nao tem linhas para a faixa de
velocidades ligada nem para o resize (nao se mexeu em `RetainedBench.cpp` para a fonte ser a mesma dos dois lados).


## O que ficou de fora, e o que desconfio

- Item 1: o passo (0,25) e a zona (6 pixeis) nao se configuram; o fim vai para a linha mais proxima (ver a escolha acima) e nao ha tecla para
  desligar a grelha.
- Item 2: nao ha sinal durante o arrasto; a velocidade nao se quantiza; com a faixa ligada as linhas de baixo ficam escondidas (a faixa
  corta as notas, nao as encolhe); a faixa mostra as notas fora da gama de tons; a altura minima e 10 pixeis e o maximo metade do rolo.
- Item 3: um tratador que apague o proprio PianoRoll no meio de um sinal nao se prova (o codigo de antes ja depende de o rolo existir depois de
  `onNoteAdded.emit`); a coordenada NaN do rato chegou ao PianoRoll sem rebentar nada no WidgetApp (o teste passa sob ASan e UBSan com
  `-fno-sanitize-recover`), mas nao se leu o WidgetApp inteiro a procura de contas com ela; `-fsanitize=float-cast-overflow` nao esta no
  `check.sh` (nem se correu).
- Item 4: a folha de icones, as imagens registadas, as ligacoes dos sinais e os ids dos itens (ver acima).
- Item 5: TextEdit com a barra a 8 em vez de 12 (e 10 e 16) por omissao; FileDialog (6) e AssetBrowser (4) nao seguem o Theme; apps com barras
  proprias ficam a 8 ate lerem `ScrollBar::thickness()`.
- Nao se reconstruiram as aplicacoes do dono (ZenDAW, ZenVideo, ZenMix...) contra esta ponta, nem se viram os ecras: so se acrescentou API, os
  hashes do desenho nao mudam, e as mudancas de comportamento por omissao sao as duas acima (PianoRoll: clique e arrasto; TextEdit: 8).
- O `igui_zen_lifecycle` saiu uma vez `Skipped` num `ctest` solto (o `xvfb-run` nao arrancou) e passou ao repetir, como a ronda 4b ja notou.
- Cada commit intermedio foi construido e testado (release, com todos os testes) quando se fez; o `check.sh` completo so se correu na ponta.


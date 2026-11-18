# Ronda 4, segunda metade (ramo igui-round4b, em cima de igui-round4 b4b4e01)

Os itens 12 a 17 e 19 a 21 da lista de `docs/ROUND4.md` (o 18 ja estava feito), mais o que se pediu depois da primeira volta:
a receita do hint nos setters novos, o Chart a guardar double a serio e os restos pequenos. Um item por commit (o 19 em dois:
Timeline e PianoRoll), cada um com o seu teste. Este ficheiro diz, por item, o estado, o commit, o teste, a API que ficou (com
um exemplo de tres linhas), e o que se escolheu onde as notas deixavam a decisao em aberto, com a alternativa, para o dono poder
desfazer. Regras da ronda: sem `std::` nem cabecalhos da biblioteca standard em codigo novo (a contagem de `std::` por ficheiro
nao subiu em nenhum), nada abrandar, tudo em scope com MemoryMax=4G, `taskset` nos CPUs 6, 7 e 9 a 11 (o 8 e de outro agente)
e o bench a CPU 7, nenhum teste abre janela (os testes novos sao todos sem janela: a entrada sintetica do item 16 e o
rasterizador por software).

O ramo foi refeito sobre a igui-round4 (b4b4e01) com `git rebase`: sem conflitos, e nenhum teste meu teve de mudar (as
posicoes em pixeis que afirmam nao andaram com o novo `pathStroke` nem com os hints medidos). O hash do desenho do editor
passou a ser o de la, 87b1fe59ac36adb1; o piano roll continua 7956122198be0670. `bench/retained/draw.hash` ganhou uma quarta
linha, dos graficos (ver o item 20).

Os testes novos estao em dois ficheiros NOVOS, para o rebase nao ter conflitos: `tests/test_retained_menus4b.cpp` (alvo
`igui_retained_menus4b`, itens 12 a 15) e `tests/test_retained_round4b.cpp` (alvo `igui_retained_round4b`, itens 16 a 21 e o
resto); mais um bloco em `tests/test_themes.cpp`. No `CMakeLists.txt` so dois blocos de `add_executable`, a seguir ao
`igui_retained_menus`.

Como construir e correr (a partir da raiz do repositorio):
`IGUI_CONTAINERS_DIR=<ct em 34169bc> IGUI_ZEN_PLATFORM_DIR=<zen_plataform em db76a13> tools/check.sh --build <dir> --jobs 3 --base b4b4e01`.

## Estado

| # | Item | Estado | Commit | Teste |
|---|------|--------|--------|-------|
| 12 | MenuAction com icone | feito | b012872 | menus4b testMenuActionIcon |
| 13 | Menu: toggled e marcar sozinho | feito | 9d6b0f5 | menus4b testMenuToggled, testMenuToggledHandlerRebuildsMenu |
| 14 | Menu::aboutToShow | feito | b403974 | menus4b testMenuAboutToShow, testSubmenuAboutToShow, testSubmenuAboutToShowRebuildsParent |
| 15 | Popup com widgets (PopupPanel) | feito; o `closed` passou a sair na update seguinte | bdd09bc, 6323e7e | menus4b testPopupPanel* (4), testPopupPanelClosedHandlerOpensAPopup |
| 16 | Entrada sintetica no WidgetApp | feito | 48e32af | round4b testSyntheticInput |
| 17 | SideTabBar/ToolBar: icones da aplicacao e Theme | feito; predefinicoes do Theme com os 4 valores (6e957db); serializador da SideTabBar (e1e26f1; a ToolBar nao esta no serializador) | 47aa7ab, 6e957db, e1e26f1 | round4b testSideTabBarColoredIcons, testToolBarColoredIconsAndTheme, testSerializerKeepsSideTabBarColoredIcons; themes (contrastes) |
| 19a | Timeline: regua de compassos | feito | c544523 | round4b testTimelineBarRuler, testTimelineBarRulerHostile |
| 19b | PianoRoll: velocidade e sinais com o indice | feito (sem editar a velocidade com o rato nem mudar o comprimento) | 8520e3e | round4b testPianoRollVelocityAndSignals, testPianoRollShowVelocity |
| 20 | Chart: titulos dos eixos; dados double | titulos 6cbaacb; double a serio 4339293 (guarda e desenha double); o tooltip de uma serie sem nome diz "Series N" a6d319a; linhas do Chart na bancada e hash dos graficos 4c8925c | 6cbaacb, 4c8925c, 4339293, a6d319a | round4b testChartAxisTitles, testChartDoubleData, testChartTimestamps, testChartLargeCounts, testChartNonFiniteData, testChartTooltipText, testChartTooltipNamesUnnamedSeries |
| 21 | Slider: leitura do valor | feito; so pede layout quando o hint muda (970d2fb) | 60cdb41, 970d2fb | round4b testSliderValueReadout, testSliderReadoutAsksForLayoutOnlyWhenTheHintChanges |

Como se viu "falha antes, passa depois": para a API nova que nao existia, o teste nao compila sem o src (isso e o "falha antes"
mais honesto); para ver falhas em tempo de execucao pos-se de volta so o ficheiro `.cpp` de antes (ou desligou-se a chamada nova,
ou arredondou-se a entrada do Chart para float) com o cabecalho novo, e contaram-se as verificacoes que falham. Os numeros estao
em cada mensagem de commit (`git log b4b4e01..igui-round4b`).

## 12. MenuAction com icone

```cpp
MenuAction* a = menu->addAction("Open");
a->setIcon(IconId::Folder);          // ou o IconId de uma imagem da aplicacao (registerImageIcon)
a->setShortcut("Ctrl+O");
```

O icone vai na coluna da esquerda do item (a dos 22 pixeis do visto, que ja contava para a largura: o menu nao muda de tamanho),
com 16 pixeis e a cor do texto do item (a de desactivado, a de destaque). Uma accao marcavel e marcada com icone mostra uma
moldura a volta do icone onde as outras tem o visto.
Escolhas: so `IconId` (a convencao do resto da biblioteca: ToolBar, SideTabBar, TreeNode; as imagens da aplicacao ja entram por
`registerImageIcon`). Alternativa: `setImage(TextureHandle, Rect uv)` por accao, mais 24 bytes por accao; e icones a cores no
menu (hoje saem na cor do texto), como o `setColoredIcons` do item 17.

## 13. Menu: toggled e marcar sozinho

```cpp
act->toggled.connect([](bool on) { /* o clique virou o estado */ });
act->setAutoToggle(false);           // o clique so emite triggered; a aplicacao decide
act->setChecked(true);               // silencioso, como o CheckBox
```

Por omissao nada muda (autoToggle true). O clique vira o estado, emite `toggled(novo)` e depois `triggered` (que ve o estado
novo, como sempre). Com dois sinais seguidos um tratador do primeiro pode limpar o menu ou apaga-lo e a accao com ele: a
accao guarda um aviso (`gone_`, posto a true no destrutor) e o `triggered` nao sai numa accao que ja nao existe.
Escolhas: `toggled` so nos cliques (o `setChecked` e calado: sem laco entre o modelo e o menu); com `setAutoToggle(false)` nao ha
`toggled` (a aplicacao le `isChecked()` no `triggered` e poe o contrario). Alternativa: `toggled` tambem no `setChecked`
(como o Qt), ou `toggled(estadoPedido)` mesmo sem virar.

## 14. Menu::aboutToShow

```cpp
recent->aboutToShow.connect([&]() { recent->clearActions(); for (auto& f: files) recent->addAction(f); });
```

Emitido no `Menu::exec` (clique no titulo de uma MenuBar, menu de contexto) antes de se calcular o tamanho, e quando um submenu
vai abrir (uma vez por abertura). O tratador pode refazer os itens do proprio menu; nao deve mexer no menu que o contem (se
mexer, o menu nao abre o submenu e nada rebenta). Alternativa: tambem um `aboutToHide`.

## 15. PopupPanel

```cpp
auto* panel = new PopupPanel(LayoutDir::Vertical);    // da aplicacao: feito uma vez, mostrado muitas
panel->createChild<Slider>(); panel->setFocusTarget(panel->createChild<TextInput>());
panel->openBelow(button);                              // ou panel->open(x, y)
```

Um `BoxLayout` que se mostra como popup (`showPopup(panel, dono, false)`: a aplicacao e dona), arruma-se ao abrir no tamanho
dos filhos e dentro da janela, desenha fundo e rebordo do Theme, fecha com Escape, com um clique de fora ou com outro popup,
toma o foco ao abrir (`lendFocus`: ao fechar o foco volta ao que o tinha) e emite `closed` (uma vez por abertura).
Para os widgets la dentro funcionarem foi preciso mexer no `WidgetApp`: `Widget::popupHoldsWidgets()` (virtual, false por
omissao; so o PopupPanel diz true) faz com que o clique num widget do popup que aceita o foco lho de
(`dispatchMousePress`) e que o Tab/Shift+Tab andem so pelos widgets do popup (`focusNeighbor`). Nenhum outro popup muda.
**`closed` (mudou na segunda volta):** primeiro saia sincrono de dentro do `resetPopupState`; o teste de um handler que abre um
popup (pedido depois) mostrou que isso estraga um handler que reabre o painel (o `retirePopup` faz a seguir
`notifyWidgetRemoved(painel)`, que tira o painel reaberto do lugar de popup activo). Agora o `resetPopupState` pede uma chamada
diferida (`callDeferred`) e o `closed` sai na update seguinte, depois dos eventos; para a chamada diferida sobreviver, o
`WidgetApp::retirePopup` passou a chamar o `notifyWidgetRemoved(popup)` ANTES do `resetPopupState` (so o Menu, o Dialog e o
PopupPanel o redefinem, e nenhum depende da ordem). Consequencias: o `closed` chega uma frame depois de o painel sair; se o
painel for apagado antes, nao sai; um popup aberto pelo handler vem depois do que a aplicacao abriu, por isso e o popup.
Alternativa: o sinal logo no `close()` (hoje sai um decimo de segundo depois do fecho, quando o painel desaparece de facto).

## 16. Entrada sintetica

```cpp
app.feedMouseMove(x, y);  app.feedMouseButton(0, true);  app.feedMouseButton(0, false);   // um clique
app.feedKey(Key::Return, true); app.feedKey(Key::Return, false);
app.feedText("Musica");   // UTF-8
```

Cada chamada entrega UM evento ja, por `update()` com um IO feito de proposito (o estado do ponteiro e dos botoes que o update
viu da ultima vez, o tamanho da janela, o evento, os modificadores dados so para esse evento, e tempo zero). Mesmo caminho
que um backend. Depois de cada evento o ponteiro e os botoes sao postos tambem no `GetIO()` (senao a frame seguinte moveria o
ponteiro para a posicao antiga do IO global: ha uma verificacao que o apanha). O layout e o da ultima `paint()`.
Escolhas: entrega imediata, sem frame (a alternativa, mais pequena, era so por os eventos no `GetIO()` e deixar a proxima
`update` consumi-los; mas entao um teste ou uma ferramenta sem janela teria de bombear frames a cada passo, e a ordem com os
eventos reais do backend era menos clara); como o tempo nao passa, dois cliques sem frame no meio sao duplo clique (documentado
e testado). Nao ha `feedClick(x, y)` nem `feedDrop`: nao pedidos.

## 17. SideTabBar e ToolBar com icones da aplicacao e Theme

```cpp
IconId tile = registerImageIcon(tex, uv);              // ja existia
bar->setColoredIcons(true);  bar->setBottomBorder(false);   // ToolBar; a SideTabBar so tem setColoredIcons
Theme::instance().toolbarChecked = accent;                  // + toolbarCheckedDot, toolbarIcon, toolbarIconDisabled
```

As imagens da aplicacao ja entravam em qualquer widget que leva um `IconId` (`registerImageIcon`), mas saiam tingidas com a
cor do estado (uma imagem a cores saia escura). `setColoredIcons(true)` desenha-as como sao e o estado ve-se no quanto
esbatidas estao (SideTabBar: pagina aberta ou com o rato inteira, parada 200, desactivada 80; ToolBar: toggle desligado e sem
o rato 190, desactivado 80). A ToolBar ganha `setBottomBorder(bool)` (a linha da base era fixa) e o Theme ganha
`toolbarChecked`, `toolbarCheckedDot` (alfa 0: sem bolinha), `toolbarIcon`, `toolbarIconDisabled`, com por omissao os valores
que estavam fixos no desenho. **Segunda volta:** as predefinicoes claras definem os quatro (a light, a macos, a macos-dark e a
xp; a dark fica com os de sempre): na light o icone da ToolBar era o (220,220,225) da escura sobre um painel claro, com contraste
de 1.3; o serializador guarda e le o `coloredIcons` da SideTabBar (guardado so quando e true) como ja guardava os outros
atributos dela; a ToolBar nao esta no serializador, por isso as duas flags dela ficam de fora.
Escolhas: um interruptor por widget, nao um por Theme (o Theme global nao sabe qual das barras tem icones a cores).
Alternativas: um `IconStyle` por icone; os valores de `esbater` no Theme.

## 19a. Timeline: regua de compassos

```cpp
timeline->setBarRuler(120.0f, 4);     // 120 bpm, 4 tempos por compasso; o tempo da timeline continua em segundos
timeline->clearBarRuler();            // volta aos segundos ou fotogramas
if (timeline->barRuler()) { /* ... */ }
```

Numero do compasso (de 1) em cada linha, de 2 em 2, 4 em 4... quando ficam a menos de 36 pixeis; tracos pequenos para os
compassos de permeio e para os tempos (se houver 8 pixeis por tempo), tempos numerados "compasso.tempo" com 40. Andamento 1 a
999, batidas 1 a 64; andamento invalido desliga. So a regua muda (clips, keyframes e cursor ficam em segundos). Os tracos
de segundos passaram, sem mudar uma linha, para `paintTimeTicks`; os de compassos usam o `stepLimit`/`pixelCap` do
`GridSteps.hpp` (o numero de tracos tem limite nos pixeis).
Escolhas: andamento fixo em vez de um mapa tempo-compasso (mudancas de andamento); alternativa: `setBarMap(ct::Function<float(float)>)`.
O snap continua a ser em fotogramas (sem snap aos tempos).

## 19b. PianoRoll: velocidade e sinais com o indice

```cpp
roll->addNote(60, 0.0f, 1.0f, 0.9f);  roll->setNoteVelocity(0, 0.5f);  roll->setShowVelocity(true);
roll->onNoteCreated.connect([&](int i) { model.add(roll->notes()[i]); });
roll->onNoteMoved.connect([&](int i) { ... });  roll->onNoteDeleted.connect([&](int i, const PianoNote& n) { ... });
```

`PianoNote` ganha `velocity` (0..1, 0.8 por omissao; `PianoNote{p, s, d}` continua a compilar; passa de 12 para 16 bytes).
`onNoteCreated(i)` (nota posta num sitio vazio, ao largar o botao), `onNoteMoved(i)` (arrastada para outra altura ou inicio; um
clique que a deixa onde estava nao conta), `onNoteDeleted(i, nota)` (depois de sair). Os sinais de antes (`onNoteAdded`,
`onNoteRemoved`, `onNoteClicked`) saem como saiam e pela mesma ordem.
Escolhas: sinais novos em vez de mudar as assinaturas dos velhos (varias aplicacoes ligam-se a eles); a velocidade em float
0..1 (a do ZenDAW) e nao 0..127. NAO feito: editar a velocidade com o rato (alternativa: uma faixa de velocidades por baixo, ou
Alt+arrastar) nem o comprimento de uma nota (so mover, como antes); `setShowVelocity` mostra a velocidade pela opacidade.

## 20. Chart: titulos dos eixos e dados em double

```cpp
chart->setXAxisTitle("Time (s)");  chart->setYAxisTitle("Level (dB)");     // barras, histograma, linhas, pontos
ct::Vector<double> t, level; chart->plot(t, level, "signal");               // tambem addSeries, bars, pie, hist...
double x = chart->xD(0, 37);  String tip = chart->tooltipText(chart->hoveredItem());
```

Titulos: tiram uma linha ao `plotRect()` cada; o do y vai por cima do eixo, ao bordo esquerdo (nao ha texto rodado).

**Double (feito a serio na segunda volta, commit "Chart: guarda e desenha double").** `ChartSeries::values/xs/samples` sao
`ct::Vector<double>`; a conta dos eixos (`niceAxis`, `buildAxis`, marcas), o histograma (intervalo, extremos, contagem), os totais, as
percentagens e os valores impressos fazem-se em double, e so se passa a float para o pixel, depois de tirar o inicio do eixo
(`(v - xlo) * escala`, em double). A API de float fica: mesmos nomes e assinaturas, `value()`, `x()`, `total()`, `axisMin()`...
devolvem float (arredondado), `binEdges()` um espelho em float; ganha `valueD()`, `xD()`, `totalD()`, `axisMinD/MaxD/StepD`,
`xAxisMinD/MaxD/StepD`, `binEdgesD()`, `formatValueD()`, e as chamadas com dados aceitam `ct::Vector<double>` (ou de qualquer tipo
numerico: templates que so entram se o vector nao for de float, para as listas entre chavetas e os vectores de float nao ficarem
ambiguos). Um vector de float e alargado a double ao ser copiado para a serie, sem copia pelo meio. Mais: `tooltipText(item)`,
`formatX(x)` (o mais curto de 6 a 15 algarismos que volta ao mesmo numero: `1700000000.001`; um float alargado, `0.1f`, sai `0.1`
como o `%g` de antes), `xTickText(v)`, `yTickText(v)`; `decimalsFor` (as casas dos passos) e agora relativa ao passo e vai a nove
casas (um passo de 0.0001 ou menos tinha zero casas: todas as marcas "0"; o intervalo de um histograma de 0.00007 saia "0-0").
NaN e infinitos nos dados passam como passavam (gravado do grafico de float antes, conferido depois: os mesmos eixos, a mesma
imagem, nenhuma excepcao; os pontos nao finitos NAO sao saltados, nem antes nem agora).

Memoria: uma serie de N pontos guardava 4N bytes de valores, mais 4N de xs numa linha ou pontos, mais 4N de amostras num
histograma; agora 8N, 8N e 8N. 100000 pontos de uma linha: 0.8 MB e agora 1.6 MB. O que o grafico guarda da ultima pintura
(`points_`, 16 bytes por ponto) fica igual.

Velocidade (bancada, `chart/*`, 6 corridas alternadas a CPU 7, a mesma fonte do bench dos dois lados):

| linha | antes (float) us | agora (double) us | delta |
|-------|------------------|-------------------|-------|
| line_100k_frame (100000 pontos, uma frame) | 5435.6 | 5400.4 | -0.6% |
| line_100k_hover (a frame e a procura do ponto) | 5437.4 | 5402.5 | -0.6% |
| scatter_20k_frame | 20394.4 | 20515.9 | +0.6% |
| bars_1000_frame | 235.7 | 245.9 | +4.3% (outras corridas: -1.0% e -1.4% no binario normal, +6.7% e +3.4% no alinhado: depende da colocacao do codigo) |
| hist_100k_frame | 17.64 | 17.59 | -0.3% |
| hist_100k_rebin (100000 amostras em 40 casas) | 375.5 | 387.1 | +3.1% (alinhado +2.9%; 10 corridas): REAL, a divisao e a leitura das amostras em double, o dobro dos bytes (uma tentativa com o inverso da largura ficou pior, +10%, e voltou-se atras) |
| line_100_frame / bars_12_frame / pie_12_frame | 13.84 / 16.97 / 36.54 | 13.84 / 16.99 / 36.60 | 0.0 / +0.1 / +0.2% |
| line_100k_set_values / set_xs (dar 100000 numeros de float a uma serie) | 7.2 / 8.0 | 20.4 / 20.3 | +180% (REAL, sem alocar: alargar cada float a double e escrever o dobro dos bytes; uma chamada com 100000 pontos custa +13 us, uma vez por mudanca de dados e nao por frame) |


O hash do desenho dos graficos (linha nova de `bench/retained/draw.hash`, que a bancada ganhou antes de mexer no Chart) passou de
299a905a6a593356 para 957c5363d01cdd4a: dos 20430 vertices das 16 cenas mudam 484, nas cenas 3 e 11 (a linha sobre os indices, nos
dois tamanhos) e 14 (o pie pequeno), ate 3.1e-05 pixeis, sem mudar cor, uv, indices nem comandos. A causa e so a aritmetica de float
do pixel (o rateio `(v-xlo)/(xhi-xlo)*largura` e o varrimento `v/soma*2pi` dos sectores): pondo essas duas contas outra vez em
float o hash volta ao de antes (conferido). Noutras 300 cenas aleatorias da API de float (barras, linhas, pontos, histogramas,
pie, donut; valores de 1e-2 a 1e6) o numero de vertices, de indices e de comandos e igual em todas; 223 diferem em posicoes (212 ate
3e-05 pixeis; 11 em 1 pixel: a linha da grelha ou o rotulo que cai na fronteira de um `floor()`, onde a conta de float dava 180.99998
e a de double da 181 exacto, ou a casa decimal do rotulo de um extremo de histograma num empate de arredondamento). Com valores
abaixo de 1e-2 (passos abaixo de 1e-4) mudam os rotulos por causa do `decimalsFor`. Os hashes do editor e do piano roll nao mexem.
DECISAO PARA O DONO: se preferir o desenho de float identico bit a bit ao de antes, isso obriga a manter o pixel em float para dados
de float (e perde-se a exactidao nos de double); eu fiz o contrario (double sempre).

## 21. Slider: leitura do valor

```cpp
slider->setShowValue(true);  slider->setValueSuffix(" dB");  slider->setValueDecimals(1);
String s = slider->valueText();      // "-6.0 dB"
```

O texto fica a direita de um slider horizontal (por baixo de um vertical); a pista encurta pelo espaco do texto (o do maior
entre o do minimo e o do maximo, para a pista nao mexer com o valor); o rato mapeia para a pista sem o texto; um slider curto
de mais nao o mostra. Escolhas: casas e sufixo (como o Chart: `setValueDecimals`, `setValueSuffix`). Alternativa: um formatador
`ct::Function<String(float)>` (ganho linear para dB).
**Receita do hint (segunda volta):** o Slider guarda o hint (`HintMemo`) e os setters da leitura (`setShowValue`,
`setValueDecimals`, `setValueSuffix`, e `setRange` com a leitura ligada) so pedem layout quando o `sizeHint()` muda; o valor, mudado
a cada frame por codigo ou pelo rato, nunca pede layout (o espaco da leitura e o do texto do minimo e do maximo). Testado com um
contador de passagens de layout.

## Verificacoes (sobre a ponta de codigo 6323e7e; o commit das notas so acrescenta este ficheiro)

**`tools/check.sh` como programa correu desta vez** (`--base b4b4e01 --jobs 3`, `taskset -c 6,7,9-11`): `PASS nostd`, `PASS release`
(42 de 42), `PASS asan` (ASan e UBSan, 42 de 42) e `PASS hash` (o `draw.hash` com as quatro linhas); "4 stages, 0 failed".
(Na primeira volta o ambiente tinha recusado o script e as fases tinham sido feitas a mao.) Uma vez, num `ctest` solto, o
`igui_zen_lifecycle` saiu "Skipped" (o xvfb-run nao arrancou) e passou ao repetir.

Contagem de `std::` (linhas com `std::`, antes e depois, ficheiros mexidos desde b4b4e01): nenhum subiu (ChartWidget.hpp/.cpp 0/0,
InputWidgets 0/0 e 21/21, MenuWidgets 0/0 e 3/3, WidgetApp.cpp 32/32, Widget.hpp 13/13, WidgetApp.hpp 5/5, Timeline 2/2 e 85/85,
AudioWidgets 0/0 e 68/68, ToolBar.cpp 12/12, Theme.hpp 0/0, WidgetSerializer.cpp 1/1...).

### Benchmark

`igui_retained_bench`: b4b4e01 (com o mesmo `bench/retained/RetainedBench.cpp` da ponta; so sem as duas chamadas
`setXAxisTitle/setYAxisTitle` das cenas de `--hash`, que nao existem la, e que nao entram nas medidas) contra a ponta, as duas
construcoes com a mesma configuracao (Release, so o bench), `taskset -c 7`, 6 corridas alternadas, minimo por linha; 136 linhas.
Normal: acima de +3%: `editor300/3000 layout_only` +6.5% e +6.2% (0.4 us de 6.3), `label_sizehints` +18.7% (0.96 -> 1.14 us),
`widget/listbox_per_widget` +3.6%, `chart/hist_100k_rebin` +3.1%, `chart/line_100k_set_values` e `set_xs` +173%. Alinhado
(`-falign-functions=64 -falign-loops=64` nos dois lados, 6 corridas): `layout_only` -6.4% e -6.1%, `label_sizehints` -1.0% e -2.1%,
`listbox` -6.7%: o sinal troca com o alinhamento, era colocacao de codigo, nao codigo; mas aparecem so no alinhado
`timeline300/3000 idle_frame_all_visible` +3.7% e +7.7% (101.1 -> 104.9 e 451.0 -> 485.8 us; no normal +1.1% e -0.9%) e
`chart/bars_1000_frame` +6.4% (no normal -1.4%; repetido com 10 corridas dos dois: -1.0% normal, +6.7% alinhado): e o mesmo
sinal que a igui-round4 ja viu e explicou nas `timeline*/idle_frame_all_visible` (so no binario alinhado, o normal anda a 0%), por isso
leio-o como colocacao e nao como custo; nao o provei por bisecao. Reais, e causa: `set_values`/`set_xs` do Chart (+13 us por 100000
pontos: o alargamento a double e o dobro dos bytes) e `hist_100k_rebin` (+3.0%, normal e alinhado: a divisao e a leitura das
amostras em double). Todas as linhas de frame (editor, timeline, entrada, grelhas, graficos) ficam a menos de 1.5%; as alocacoes por
iteracao sao iguais em todas as linhas. Mais rapidas (normal): `bigtext/doc20000_type` -4.4%, `loops/pianoroll_400_beats` -4.2%.
Tabelas completas: `scratchpad/ig4b/bb/final_table.txt` (normal) e `finalal_table.txt` (alinhado).

### ZenDAW e ZenVideo contra a ponta

Copias em `scratchpad/ig4b/apps` (rsync dos projectos do dono sem as pastas de build, `third_party/iGUI` trocado pela arvore deste
ramo; nada escrito nas pastas dos projectos nem nas definicoes do dono: HOME, XDG e os dados de cada um em scratch). Compilam e
ligam sem tocar no codigo das aplicacoes (`zendaw` e `zenvideo`, Release, `-j3`). Corridos escondidos com `ZEN_SCREENSHOT` e
`ZEN_SCREENSHOT_FRAME=60`: ZenDAW por `tools/run_hidden.sh --size 1280x800 -- ./zendaw --null-audio` (a copia do script com o
`taskset` fora do CPU 8), ZenVideo por `xvfb-run -a` com `ZENVIDEO_NULL_AUDIO=1`. Comparados byte a byte com
`scratchpad/shots/zendaw_final.ppm`, `zendaw_house_final.ppm` e `zenvideo_final.ppm`: **0 pixeis diferentes nos tres** (1280x800,
1280x800, 1440x860), como se esperava: nenhuma das duas usa a API nova, e nao abre nenhum popup nem mostra graficos no arranque.

## O que ficou de fora, e o que desconfio

- Item 19b: a velocidade nao se edita com o rato, e o comprimento da nota continua a nao se poder mudar.
- Item 17: a ToolBar nao esta no serializador, por isso `setColoredIcons` e `setBottomBorder` dela nao se guardam.
- Item 16: o layout da entrada sintetica e o da ultima `paint()`; um teste que faca `requestLayout()` a seguir a por um filho do
  stage a mao ve-o reposto a janela toda (e o comportamento do item 9): os testes novos usam `Harness::frame()`.
- Chart: os pontos NaN/infinitos NAO sao saltados (nem antes, nem agora): saem como vertices nao finitos; e o que o grafico de float
  fazia, conferido, mas e um defeito. `decimalsFor` mudou para os passos abaixo de 1e-4 (antes zero casas); o desenho de float mexe
  em 484 de 20430 vertices das cenas do hash por arredondamento (ver o item 20). Uma serie de 100000 pontos custa o dobro da
  memoria.
- O `closed` do PopupPanel sai na update seguinte (ver o item 15).
- O teste hostil da regua em compassos limita os vertices por frame, nao tem teto de alocacao proprio (correu nos scopes de 4 GB).
- So testei a ponta (e cada commit quando o fiz, antes do rebase e da reordenacao): os commits intermedios de agora nao foram
  construidos um a um.
- As aplicacoes: so se viram os tres ecras de arranque; nenhum abre popups, graficos, sliders com leitura nem barras de icones a cores.

## Para o rebase com a igui-round4

Ja esta feito: o ramo e filho de b4b4e01 (o ultimo commit de codigo de la), sem conflitos. Os ficheiros partilhados que este ramo
muda em relacao a ela: `CMakeLists.txt` (dois blocos de teste), `Widget.hpp` (um virtual), `WidgetApp.*`, `Theme.hpp`,
`MenuWidgets.*`, `ToolBar.*`, `SideTabBar.*`, `Timeline.*`, `AudioWidgets.*`, `ChartWidget.*`, `InputWidgets.*`,
`WidgetSerializer.cpp`, `bench/retained/RetainedBench.cpp` e `draw.hash` (uma linha nova).


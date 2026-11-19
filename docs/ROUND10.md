# Ronda 10: o callback de desenho nativo, as pequenas pecas dos widgets e o grafico interactivo

Os pedidos das equipas das aplicacoes estao em `docs/specs/chart.md` e `docs/specs/draw-callback.md` (guardados como chegaram). Esta ronda tem quatro partes: o callback de
desenho nativo (parte 1), as pequenas pecas dos widgets (parte 2, com o que a equipa do ZenVideo juntou), o grafico interactivo `PlotChart` (parte 3) e, se sobrasse, as perdas que a
ronda 9 deixou (parte 4). Tudo o que esta aqui foi corrido; o que nao se fez esta na ultima seccao.

## 1. O callback de desenho nativo

Documento proprio, com o exemplo completo e as restricoes do web: **`docs/DRAW-CALLBACK.md`**. Resumo: `DrawList::addNativeCallback(fn, user, rect, restoreState)` poe na lista um comando que o
renderizador OpenGL do adaptador `zen` corre no seu lugar da ordem do desenho, com o recorte do widget ja posto como scissor, o estado GL limpo na entrada (programa 0, vertex array proprio e vazio,
sem buffer, unidade 0 sem textura) e reposto na saida (a lista exacta esta no documento); recebe o tamanho do alvo em pixeis, o rectangulo do widget e o recorte em pixeis (origem em cima a esquerda, y para
baixo) e tambem como o GL os quer (origem em baixo a esquerda), a escala de pixeis, a projeccao do iGUI (unidades logicas e pixeis do alvo), a contagem de amostras e a etiqueta do backend (GL 3.3 core ou
GLES3/WebGL2). `NativeDrawAvailable()` (antes de pintar) diz se o renderizador corre callbacks: verdadeira com OpenGL logo depois de `App::create`, falsa com o rasteriador por software e num teste sem
renderizador; ai o comando fica na lista e o rasteriador salta-o. A fusao de comandos da ronda 9 nunca atravessa um callback. O callback so corre em frames que se pintam e nao marca nada sujo; o widget
`NativeView` traz o callback e um desenho de reserva e pergunta ao renderizador quando pinta.

Testes: `igui_native_callback` (sem GL: disponibilidade, o desenho de reserva, o lugar na lista, nada fundido, recortes vazios, a transformacao, o rasteriador que salta, o parado) e
`igui_zen_native_callback_gl` (num Xvfb privado, llvmpipe: 0 chamadas em 100 frames paradas e 1 por `requestRepaint`, o que a funcao recebe, **um triangulo conhecido lido do GL: certo dentro do
rectangulo e nada fora**, o recorte de um `ScrollView`, a ordem entre dois widgets que se sobrepoem, o estado GL estragado de proposito por uma funcao e os widgets desenhados depois **pixel a pixel como
sem callback**, e as amostras); com o restauro do iGUI desligado de proposito o teste falha com 8819 pixeis diferentes (controlo negativo).

## 2. As pequenas pecas dos widgets

### 2.1 TreeView

- **Reticencias.** O texto de uma linha que nao cabe termina em "..." (antes o painel cortava-o). O texto mede-se uma vez e guarda-se (`ElideMemos`: uma memoria por linha a vista, que so vale enquanto o
  texto, o espaco e o tipo de letra sao os mesmos): **a frame parada de uma arvore com textos cortados nao faz nenhuma alocacao** (20 frames, 0), e muda a largura, o tamanho do texto ou o tema, a linha pinta
  o que pintaria uma arvore feita de novo (o teste compara os pixeis com os de uma arvore cujo texto ja vem cortado ao mesmo espaco).
- **Segundo texto.** `TreeNode::setSecondaryText(text)`: fraco (`Theme::textSecondary`, 4,5 ou mais sobre o painel nos cinco presets: medido), alinhado a direita, com o seu espaco primeiro (no maximo
  metade do que sobra da linha); o texto da linha cede com reticencias e nunca lhe toca.
- **Cor do icone por no.** `TreeNode::setIconColor(cor)` / `clearIconColor()`: por omissao o icone segue `Theme::iconColor`, por isso o icone de uma folha de sprites muda de cor com o tema sem
  reconstruir linhas; uma cor dada ganha. O mesmo em `TreeGrid::Node::iconColor` e no `Outliner`. **A `ListBox`, a `DataGrid` e as celulas de tabela nao tem icones** (so texto): nada a tingir.
- Os setters de `TreeNode` (`setText`, `setDimmed`, os novos) marcam a vista suja: antes uma mudanca de texto de um no nao pintava uma frame numa janela que salta frames iguais.

### 2.2 TreeGrid

`Signal<Node*, int> nodeActivated` (no, coluna): duplo clique e Enter **numa grelha que nao edita** (`setReadOnly(true)`, uma coluna ou um no so de leitura). Numa celula editavel o duplo clique edita como
sempre e nao activa. O no e a identidade da linha (nao muda quando as linhas se ordenam ou se abrem); o tratador pode tirar a grelha da arvore (testado sob ASan).

### 2.3 FileDialog, modo Save

- **Substituir um ficheiro**: ja existia a pergunta (o primeiro Enter pergunta, o botao passa a "Replace", o segundo grava); verificado, e corrigido: se o nome muda depois da pergunta o botao volta a
  "Save" e a pergunta esquece-se (antes o nome novo era gravado com o botao a dizer "Replace").
- **Extensao**: um nome sem extensao leva `setDefaultExtension` ou, sem ela, a primeira do filtro (`"*.png;*.jpg"` da ".png"); `setAppendExtension(false)` desliga. A pergunta de substituir e feita sobre o nome
  com a extensao ("exists" com filtro `*.txt` pergunta por `exists.txt`).
- **Pasta lembrada por id** (opt-in): `setRememberFolder(id)`: o dialogo abre na pasta em que um dialogo do mesmo id foi aceite (se ainda existe) e lembra aquela em que e aceite; a memoria e a do
  processo, `FileDialog::lastFolder(id)` / `setLastFolder(id, path)` deixam a aplicacao guarda-la nas suas definicoes.

### 2.4 Texto seleccionado das listas (as sobras da ronda 9)

`ListBox`, `ListWidget` (as etiquetas da linha seleccionada, por `PaintContext::rowText`), a lista do `ComboBox` e a lista do editor (completar, paleta) pintam o texto da linha seleccionada com
`Theme::selectionText`, como as arvores: **no escuro era `textColor` sobre a selecao, 3,8:1; agora 4,9:1 ou mais** (antes e depois, lado a lado, nos cinco presets: `docs/img/round10/list-selection-<preset>.png`, esquerda como era, direita como e). O escuro desenha diferente so nas linhas seleccionadas destas listas:
`kDarkGolden` de `tests/test_retained_round9.cpp` passou de `b5c94cffdf3657d9` a `f05be51a369160b1` (os mesmos 6354 vertices, uma cor de texto mudada); o teste deixou de ter a excepcao de 3,5 para a `ListBox`
no escuro. O `bench/retained/draw.hash` nao mudou (a janela do editor do bench nao tem uma lista seleccionada).

### 2.5 O que a equipa do ZenVideo juntou

1. **O desenho do escuro depois da ronda 9** (pergunta da coordenacao: a causa, o que a `main` desenha hoje, o teste que o fixa). Causa dos dois: a ronda 9 passou os literais do dock para `Theme::dock` (`DockColors`, commit `003321c`) e fez o `DockPanel` pintar com ele (`012e3ba`); o escuro ficou com os valores de sempre (`handle` 50,50,53, `border` 60,60,65, `leafBg` 38,38,42, `accent` 120,160,220), o macos-dark herda os do macOS (azul do sistema). **Escuro: a `main` desenha o aspecto ANTIGO** (pixeis iguais; teste `kDockSplitGoldenDark` em `testGoldens()` de `tests/test_retained_round9.cpp`). **macos-dark: desenha o aspecto NOVO, de proposito** (teste `kDockSplitGoldenMacDark`, na mesma funcao). (a) *O separador entre dois paineis do dock:* **nao se reproduz**. A janela do dock com divisao (3 paineis, divisor a 40%, rectangulos inteiros e
   fraccionarios, o separador sob o rato) desenhada no escuro pela biblioteca de antes da ronda 9 (`0d86927`, construida a parte) e pela de agora da **os mesmos pixeis** (soma de controlo `d5b9fa6f2817d9b3`
   nos dois); os campos `Theme::dock` do escuro (`handle` 50,50,53, `border` 60,60,65, `leafBg` 38,38,42, ...) sao os literais de antes. As cores 54,54,59 e 41,41,46 nao sao nenhuma do dock: sao
   60,60,65 (a moldura de uma folha, desenhada ha muito) a cobrir 0,73 e 0,14 de dois pixeis sobre 38,38,42, ou seja uma linha de um pixel numa posicao fraccionaria; pedimos-lhes o rectangulo do dock e
   o renderizador (GL ou software) se o vem noutra posicao. Fica o guarda: `kDockSplitGoldenDark` no teste (os pixeis de um dock dividido no escuro, os de antes da ronda 9).
   (b) *`macOSDark`: a linha da aba do dock `54,59,71 -> 40,56,75` e as 1,7 mil diferencas pequenas*: **e deliberado** e fica. O `Theme::macOSDark()` e o preset "macOS escuro" (azul do sistema 10,132,255, fundo
   40,40,42); antes da ronda 9 o dock ignorava-o (usava o literal do escuro, 120,160,220) e a janela ignorava-lhe o fundo (30,30,30 fixo, `WidgetApp::bgColor`): agora o dock segue `dock.accent` do preset (o azul
   do sistema) e a janela o `bgColor` do preset. Medido na janela do dock: 8000 pixeis passam de 30,30,30 a 40,40,42 (o fundo) e 143 de 120,160,220 a 10,132,255 (a linha da aba), mais nada. Os dois
   estavam em `ROUND9.md` 1.2/1.5 ("macos-dark: look novo, sem a regra do escuro exacto"). A janela de 50 widgets de teste **ja tinha** um dock dividido (o painel de propriedades) e so o escuro: **faltavam os outros
   quatro presets**: `kPresetGolden` tem agora a soma de controlo do desenho da janela em cada preset, e `kDockSplitGoldenMacDark` os pixeis do dock dividido em macos-dark.
2. **Switch.** `Theme::switchOn`, `switchOff` e `switchThumb` (ja existia) em todos os presets (o escuro como estava: 80,160,80 e 70,70,75): o claro 0,120,215 e 130,130,138, macOS o azul do sistema e
   140,140,148, macOS escuro 10,132,255 e 110,110,116, XP 49,106,197 e 128,124,110. `setOnColor` / `setOffColor` continuam a ganhar. O polegar tem 3:1 ou mais contra as duas pistas (so a pista ligada do escuro, 2,4:1,
   fica como sempre foi; medido no teste), e a pista ligada difere da desligada (distancia de cor 80 ou mais).
3. **Popups nos presets que nao sao o escuro.** O teste da ronda 9 abre, em cada preset, o menu, a lista do `ComboBox`, a dica, o popup de cor, o do calendario, o menu de contexto e a lista de completar do editor, e
   verifica que nao ha fundo escuro nos presets claros e que o texto tem 4,5 de contraste. Achou e corrigiu: o texto das ligacoes do calendario (mes e ano) e dos segmentos do `TimePicker`
   com `focusColor` (3,7 a 4,3 no claro, passa a `app.richLink`, 6:1; no escuro como era), o dia e o ano seleccionados do calendario (branco sobre o acento a 200 de alfa: 3,1 a 4,0; passa a branco
   sobre o acento escurecido e opaco nos presets claros), `menuShortcutText` do macOS (4,06 -> 5,1), o texto seleccionado e o pormenor da lista de completar (1,4:1 no XP), e **um erro de posicao que ninguem
   tinha visto: o texto das listas e das barras do editor (completar, paleta de comandos, procurar, ajuda de assinatura) era desenhado com a linha de base no topo da linha, um ascendente acima dela; o
   realce da seleccao ficava a meio da linha de baixo**. Imagens: `docs/img/round10/popups-<preset>.png`.
4. **Cor do icone por linha**: em 2.1 (`TreeNode`, `TreeGrid::Node`, `Outliner`).
5. **So as cores de um preset** (sem as medidas): `Theme::applyColors(preset)` (e `applyMetrics`, `sameMetricsAs`), e `WidgetApp::setTheme(theme, ThemeParts)` / `setThemePreset(nome, ThemeParts)` com
   `ThemeParts::All` (omissao: como sempre), `Colors` ou `Metrics`. Com `Colors`: as cores e os aspectos (gradientes, blocos, caixas) do preset, as medidas da aplicacao; `themeChanged` **uma vez** e **nenhuma
   arrumacao**; o teste compara o rectangulo de todos os widgets da janela de 50 widgets antes e depois de `setThemePreset("macos", ThemeParts::Colors)` (iguais) e mostra que com o preset inteiro
   alguma muda (as medidas do macOS), e que o contador de arrumacoes nao anda.
6. **A espera do adaptador** esta numa funcao so: `ig::zen::App::idleWait(int ms)` em `adapters/zen/ZenApp.cpp` (chamada por `pump()`); e la que se troca o `time_sleep` por
   `platform_wait_events` quando a zen_plataform o tiver (nao se chamou nesta ronda).

### 2.6 O custo de linhas longas pelo `PaintContext` num GL

`igui_plot_bench --only=polyline` (16 series de 4000 pontos, 1400x900, `ctx.drawPolyline` de 1,5 px, a janela de uma Xvfb privada; o GL e o llvmpipe, por isso o "submit" e o de um GL por software
e **nao** o de uma GPU; a coluna do GUI e a que a biblioteca gasta e vale em qualquer lado): resultados (a maquina era partilhada com outras sessoes, carga de 3 a 8):

| 16 series x 4000 pontos, 1400x900 | GUI (pintar para a lista) | submit + apresentar | vertices / indices | alocacoes por frame |
|---|---|---|---|---|
| GL (llvmpipe) | 2,33 ms | 65 ms (um GL por software: nao e uma GPU) | 256 024 / 1 151 748, 2 comandos | 0 |
| rasteriador por software | 2,26 ms | 14 ms (a rasterizacao) | os mesmos | 0 |

O custo do lado da biblioteca (2,3 ms) e o das linhas desenhadas a cada frame pelo `PaintContext` (a equipa mediu 4,7 a 6,3 ms no seu): e o que a camada guardada do `PlotChart` (3.3) tira de cada frame.

## 3. O grafico interactivo: `PlotChart`

### 3.1 Um widget novo, e porque

**`PlotChart`** (`include/igui/widgets/PlotChart.hpp`, `src/widgets/PlotChart.cpp`) e um widget novo: o `Chart` existente fica como estava. O `Chart` guarda uma **copia** dos valores de cada serie
(`ChartSeries::values`, `xs`, vectores que as aplicacoes leem), modela categorias, tartes, barras e histogramas, e a sua legenda e um botao que liga e desliga. O grafico pedido precisa do contrario:
dados **da aplicacao, sem copia** (um ponteiro e uma revisao), um x por serie, um milhao de pontos, um piramide de minimos e maximos, gestos e sinais; mudar `ChartSeries` partia quem o usa. Uma
extensao do `Chart` tambem nao poupava codigo: nada do desenho nem da entrada se reaproveitava.

### 3.2 A API

```cpp
auto* chart = parent->createChild<PlotChart>();
PlotSeriesInfo info;  info.id = 7;  info.name = "speed";  info.unit = "m/s";  info.yAxis = 0;       // o id e estavel; a cor (alfa 0) e a da paleta do tema, e fica
chart->setSeries(info);
chart->setSeriesData(7, x, y, count, revision);          // SEM copia: os vectores sao da aplicacao ate ao proximo setSeriesData deste id; x ascendente, NaN em y e uma falha
chart->setSeriesDataCopy(8, x, y, count);                // COM copia: o grafico e dono (a aplicacao fica livre dos vectores ja)
chart->fit();                                            // tudo; y Auto (ajustado ao x visivel), Full (todos os dados) ou Manual
chart->viewChanged.connect([](double x0, double x1, double y0, double y1, bool byUser) { /* a aplicacao empurra os pontos novos */ });
chart->hovered.connect([](double x, double y, int serie, int ponto, bool dentro) { ... });
chart->rangeSelected.connect([](double x0, double x1) { ... });        // Shift + arrastar; rangeChanged(x0, x1, done) durante; rangeCleared (Esc ou clique)
chart->plotClicked.connect([](double x, double y, int botao, int modificadores) { ... });   // o clique e comunicado, o grafico nao o gasta numa selecao
chart->setMarker(1, 20.0, cor, "In");   chart->setBand(1, 20.0, 60.0);   chart->setClock(t, true);   // sobreposicao: nunca refaz a camada das series
chart->setStats(linhas, colunas);                         // a tabela de estatisticas por baixo, dada pela aplicacao
PlotAxisLink link;  link.add(chart1);  link.add(chart2);  // os eixos x seguem-se (um gesto de uma e aplicado a outra sem ser "byUser": nada entra em ciclo)
```

- **Gestos** (todos configuraveis com `PlotBindings`: botao e modificadores exactos de cada um): roda = zoom em x em volta do ponteiro (o dado debaixo do ponteiro nao se mexe); Ctrl + roda = zoom em y
  (y passa a Manual, `yModeChanged`); Shift + roda = mover um decimo da vista por entalhe; arrastar (botao esquerdo, e o do meio) = mover em x; Shift + arrastar (ou `setRangeTool(true)` com um arrastar
  simples) = escolher um intervalo; duplo clique = tudo (y Auto); Esc (com o foco) ou um clique limpam o intervalo; clicar numa entrada da legenda mostra ou esconde a serie (`setLegendAutoToggle`) e a cruz
  (`setLegendRemovable`) pede para a remover (`legendEntryRemoveClicked`; a serie e da aplicacao, o grafico so avisa). `setFollowX(t, true)` mantem o relogio na vista.
- **`viewChanged(x0, x1, y0, y1, byUser)`** dispara **durante** o gesto, em cada movimento; `setViewX`, `setViewY`, `zoomX`, `panX`, `fit` mudam a vista **sem** `byUser`. Um tratador pode substituir os dados
  (`setSeriesData`, `removeSeries`, `clearSeries`) e pode tirar o grafico da arvore: o grafico emite sempre como ultima coisa que faz e pergunta `emitGone` (a protecao da biblioteca); testado em cada sinal, nos dois modos,
  sob ASan.
- **Dados**: uma serie tem o seu x (`double`, segundos, ascendente); NaN em y e uma falha (e `breakDx` parte a linha quando dois pontos estao mais longe do que isso); `minMax = true` diz que os pontos sao pares
  (minimo, maximo) de intervalos e desenha-se como envolvente; `yAxis = 1` poe a serie no segundo eixo y (com a sua escala); **XY**: `xSource = id` faz o x da serie ser o valor de outra (o desenho e uma
  linha, sem reducao: minimo, e o `fit()` toma o x dos valores da serie de x). Uma `revision` igual (com os mesmos ponteiros e contagem) nao custa nada. **Regra da vida dos vectores** (sem copia): ficam validos e
  **sem mudar** ate ao proximo `setSeriesData` do id, `removeSeries`, `clearSeries` ou ao fim do grafico; dados mudados no sitio pedem uma revisao nova. Num build de depuracao o grafico guarda uma impressao
  digital de 16 amostras e **para** (`__builtin_trap` com a mensagem) se, a cada pintura, os vectores ja nao a tiverem com a mesma revisao.
- **Eixos**: o tempo em segundos como `double`, passos bonitos (1, 2, 5 vezes uma potencia de dez, do nanossegundo ate ao minuto; depois 1, 2, 5, 10, 15, 30 s, minutos, 1, 2, 3, 6, 12 h, dias), rotulos com as casas
  decimais que o passo pede (`0.000123` ao microssegundo), `m:ss`, `h:mm:ss`, `h:mm`, `Nd hh:mm`; um formatador da aplicacao (`setXTickFormatter`, `setYTickFormatter`); os rotulos que se tocariam sao
  rareados ate nenhum tocar. Intervalo de x: `setExtent` (o "tudo" da aplicacao, porque as series que ela empurra sao janelas) ou a extensao dos dados.
- **Leitura**: o cursor vertical, um ponto por serie na amostra mais proxima **em x** (sem contar as falhas), a hora do cursor no cabecalho e, na legenda, o valor desse ponto por serie (`%.6g`, sem alocar); ou o texto
  da aplicacao (`setReadout`). Estatisticas: `setStats(linhas, colunas)`.

### 3.3 Como e rapido

- **A camada** (fundo, grelha, linhas) e geometria que o grafico **guarda** (`ig::retained::Mesh`) e so refaz quando o que a faz muda: a revisao de uma serie, a vista, o intervalo de y, o tamanho, a escala de pixeis, o
  tema; um contador (`layerRebuilds()`) prova que um rato a passar, um relogio, um marcador, uma banda ou um intervalo **nao** a refazem e que cada mudanca real a refaz **uma vez**. O que se mexe em cada
  frame (rato, relogio, marcadores, bandas, intervalo, texto) e uma sobreposicao.
- **`Mesh` desenhada por referencia** (parte nova da biblioteca): `DrawList::addMesh(mesh, dx, dy)`. Onde o renderizador guarda malhas (`IO::retainedMeshes`: o OpenGL do adaptador `zen`) o comando leva so um
  ponteiro e o renderizador mantem a geometria na GPU, enviada quando a `revision()` da malha muda, desenhada de novo sem mais custo nas frames seguintes, e libertada quando a malha morre; onde nao (o
  rasteriador por software, o SDL, um teste sem renderizador) a lista copia os vertices para si (memcpy de umas centenas de KB). A malha e dos widgets: qualquer widget que tenha geometria cara a pode usar.
- **As colunas de uma serie de x uniforme** (um sinal amostrado: o x e `x[0] + i * passo` com um erro pequeno, verificado em 65 sitios quando os dados entram) acham-se por formula, corrigida por `lowerBoundFrom` (exacta
  de qualquer comeco: uma formula errada custa velocidade, nunca um valor); e o que cada coluna vai ler nas amostras do bordo (um bloco de 32 de cada lado, que numa serie de um milhao de pontos nao esta na cache) pede-se
  seis colunas antes com `__builtin_prefetch`. Sem isto 16 x 1 M de pontos custavam 13,5 ms por frame de mover; os minimos e maximos correm em quatro cadeias independentes (um minimo e uma cadeia dependente).
- **Reducao a uma coluna de pixeis**: uma serie com mais pontos na vista do que pixeis desenha-se como uma banda (o minimo e o maximo de cada coluna, **nunca perde um extremo**: o teste compara, em
  numeros, a banda de cada coluna com as amostras da coluna, em varias vistas, com picos de uma so amostra e uma falha); com menos, como linha com juntas. A reducao le o piramide de blocos de 32 amostras e
  de pares de blocos, construido **uma vez por revisao**, por fatias com um orcamento de 1 M de amostras por frame (2,3 ms por milhao, medido; a biblioteca nao tem trabalhadores): enquanto nao esta pronto a vista usa uma amostragem a
  passo (e o grafico pede frames); pronto, consulta em O(log n). `pyramidProgress(id)` diz onde vai.
- **Testes e numeros**: 3.5 e 3.6.

### 3.4 Cores

`Theme::plot` (`PlotColors`: fundo, grelha, linha dos eixos, cursor, banda, margem da banda, relogio e uma paleta de 8 cores de series) em todos os presets; o texto e o do tema (`textColor`, `textSecondary`).
O teste mede nos cinco presets: cada cor da paleta tem 3:1 ou mais contra o fundo do grafico (uma linha e um grafico), as oito distinguem-se, a grelha ve-se, o cursor e o relogio 3:1 ou mais, o texto dos
eixos e da legenda 4,5 ou mais contra o painel, e o texto de uma bandeira de marcador (preto ou branco, o que ler melhor) 4,5 ou mais sobre cada cor; uma mudanca de tema refaz a camada com as cores novas.
Uma serie sem cor propria fica com a sua casa da paleta enquanto outras vem e vao.

### 3.5 Os numeros contra os alvos

`igui_plot_bench --only=chart` (16 series de 4000 pontos e de 1 M de pontos, 1400x900, com os dados da aplicacao (sem copia) e com a copia do grafico; a janela de uma Xvfb privada com o llvmpipe: o "submit" e o de um GL por software e **nao** o de uma
GPU, por isso o que conta e a coluna do GUI, que e o que a biblioteca gasta). Medido numa maquina partilhada com outras sessoes (carga de 3 a 8): o ruido e de uns 20 por cento, e por isso para o caso de 1 M
dao-se os extremos de varias corridas. As alocacoes por frame sao as de um `malloc` contado.

| | alvo | medido | |
|---|---|---|---|
| GL, 16 x 4000, parado (pintado a cada frame, so para medir) | -- | 0,06 ms | |
| GL, 16 x 4000, relogio + rato a mover (a sobreposicao) | 0,10 a 0,12 ms (o que a equipa mediu com a sua textura guardada) | **0,08 a 0,11 ms**, 0 reconstrucoes, 0 alocacoes, 0 bytes de malha enviados | cumprido |
| GL, 16 x 4000, mover / zoom (uma reconstrucao da camada por frame) | 0,6 a 1,0 ms por reconstrucao (idem) | **0,85 a 0,96 ms** | cumprido |
| idle, 16 x 4000 (NewFrame, update, needsFrame, Render) | nada pintado | 0,13 us por frame, 0 de 20000 frames pedem pintura, 0 alocacoes | cumprido |
| GL, 16 x 1 M, mover / zoom | abaixo de 5 ms | **3,2 a 4,8 ms** (seis corridas: 3,4 a 4,8; a copia propria dos dados 3,1 a 3,2) | cumprido, com pouca folga na pior corrida |
| GL, 16 x 1 M, relogio + rato | -- | 0,10 a 0,12 ms | |
| software, 16 x 4000, mover / zoom | abaixo de 1,5 ms | **0,82 a 0,84 ms** | cumprido |
| software, 16 x 1 M, mover / zoom | abaixo de 5 ms | **3,1 ms** | cumprido |
| software, 16 x 4000, relogio + rato | 0,2 ms | **0,34 ms** | **nao se chegou** (ver 4) |

O que a equipa de aplicacao mediu com o grafico dela (a mesma carga): parado 0,10 ms com uma textura guardada contra 5,6 ms por `PaintContext`; reproduzir 0,10 a 0,12 contra 4,7 a 6,3; mover 0,18 a 0,28 mais 0,6 a 1,0
por reconstrucao contra 2,3 a 2,8. O `PlotChart` esta no lado bom de todas, e **sem nenhuma textura**: a camada e geometria na GPU (GL) ou copiada para a lista (software).

A construcao do piramide de 16 series de 1 M leva 16 frames (2,3 ms de cada uma, uma serie por frame) na thread da interface; durante esses frames as series ainda nao prontas desenham-se por amostragem a passo e o
grafico pede frames.

### 3.6 Testes, demo e imagens

- `igui_plot_chart` (a biblioteca de testes do grafico, em Release e sob ASan/UBSan): o piramide contra uma procura bruta (muitos intervalos, falhas, valores iguais, blocos pequenos, construcao por fatias), os
  passos dos eixos de microssegundos a dias e os rotulos, os gestos com os valores em unidades dos dados (zoom em volta do ponteiro, mover, Ctrl + roda, Shift + arrastar, duplo clique, Esc), as ligacoes
  configuraveis dos gestos, os eixos ligados (e que sair da arvore tira o grafico da ligacao), o hover (a amostra mais proxima, falhas, um ponto por serie, o segundo eixo), a reducao que nunca perde um extremo
  (numeros, varias vistas, picos de uma amostra, falha), **quando a camada se refaz** (contadores: o rato, o relogio, marcadores, bandas e intervalo nao a refazem; cada mudanca real refaz uma vez), series, modos de y,
  dois eixos, XY, os cinco presets (contraste, paleta distinta, tema muda: camada refeita), idle (0 alocacoes, 0 frames pedidas), tratadores que destroem o grafico ou trocam os dados dentro de cada sinal, e 16 series de
  1 M de pontos.
- `igui_zen_plot_gl` (num Xvfb privado, llvmpipe): o grafico no GL real: a camada vai por referencia e esta uma malha na GPU; os pixeis lidos do GL (a linha na sua cor e na altura que o valor diz, o fundo do
  grafico com a cor do tema); um relogio e um rato a mover nao enviam nenhum byte da malha; dados novos ou outra vista enviam-na uma vez; a malha de um grafico destruido e libertada pelo renderizador.
- Demo: `igui_zen_plot` (`examples/zen_plot/main.cpp`): quatro canais de 200 000 pontos, um segundo grafico ligado ao primeiro, estatisticas do intervalo escolhido, T muda de preset, C empurra dados novos.
- Imagens em `docs/img/round10/`: `plot-<preset>.png` (cinco presets, dois eixos y, marcadores, legenda com valores, cursor), `plot-dark-milliseconds.png` (zoom a milissegundos: rotulos `24.01`, `24.02`...),
  `plot-light-range.png` (intervalo escolhido com as estatisticas por serie), `popups-<preset>.png` e `list-selection-<preset>.png` (parte 2). Saem dos testes: `ROUND10_SHOTS=<pasta>`, convertidas com
  `tools/ppm2png.py`.

## 4. O que nao se fez, e porque

- **A parte 4 (as perdas que a ronda 9 deixou) nao se tocou**: o heap do GL (46 contra 42 MB), os 200 bytes por linha do `TextEdit`, os rectangulos sujos do rasteriador por software e os
  vertices por widget do ImGui ficam como estavam em `ROUND9.md` seccao 3. A conta do cuidado com a quota era a das partes 1 a 3, que eram a ronda.
- **Software: a camada do grafico nao e uma imagem guardada** (o pedido dizia "uma imagem em cache da camada" para o rasteriador por software): e a mesma malha, **copiada para a lista a cada frame que se pinta**:
  o relogio a mover custa 0,34 ms do lado do GUI (alvo 0,2; 2,3 MB de vertices e indices copiados) e o rasteriador gasta mais 47 ms a desenhar as bandas, por isso a copia nao e o que se nota nessa frame. Uma imagem
  pedia um rasterizador de linhas no widget e um carregamento de textura de 4 MB a cada reconstrucao, que poria o mover acima dos 1,5 ms; deixou-se, e o numero diz o que custa. (Um `SoftRaster` que desenhasse a
  `Mesh` por referencia, como o GL, tiraria a copia: pedido de seguimento, nao feito.)
- **A espera por eventos da zen_plataform** (ainda nao esta na `main` dela): `App::idleWait` e o unico sitio.
- **O piramide de um milhao de pontos e construido por fatias na thread da interface** (1 M de amostras por frame, 2,3 ms): a biblioteca nao tem trabalhadores. Enquanto nao esta pronto o grafico desenha uma amostra a passo
  (aproximada) e pede frames; a reconstrucao da camada que acompanha o fim de cada piramide e uma frame a mais por serie (nao e "uma por mudanca" nesse periodo).
- **O XY** e minimo: uma linha (sem reducao, com um passo de 1 em cada 20000 pontos) com o x tirado dos valores de outra serie; a vista e a do eixo x do grafico.

## 5. Pedidos a zen_plataform e ao containers

- **zen_plataform**: nada novo (a espera por eventos que `ROUND9.md` 2.3 pediu esta a caminho; `App::idleWait` esta pronta para a receber, e nao se chamou `platform_wait_events`).
- **containers (`ct`)**: continua o `Vector::resize_for_overwrite(n)` pedido em `ROUND9.md` 2.1 (um redimensionar que nao escreve zeros: o `appendIndices` do desenho e `Mesh` do grafico escrevem tudo a seguir).
  Nada mais.

## 7. A area de transferencia dos campos de texto

Pedido da equipa da aplicacao ("nem Ctrl+V nem o rato"): colar so funcionava com Ctrl+V e, em varios campos, nem isso. Agora **um so sitio** decide o que cada tecla e o menu fazem:
`include/igui/widgets/TextClip.hpp` e `src/widgets/TextClip.cpp` (`TextClipTarget`: o que o widget diz que pode, e `textclip::onKey` / `onMouse` / `publishSelection`); os campos
nao tem nenhuma tecla de copiar ou colar escrita a mao. Uma aplicacao que faca o seu proprio campo deriva de `TextClipTarget` e chama as duas funcoes.

- **Teclas** (junto de Ctrl+V, Ctrl+C e Ctrl+X): **Shift+Insert** cola, **Ctrl+Insert** copia, **Shift+Delete** corta (`Key::Insert` e novo; o Delete simples e o Insert simples ficam como eram). Ctrl+A selecciona tudo.
  **Cmd nos presets do macOS: nao**: o `KeyEvent` do iGUI nao tem a tecla de comando (`Super`); a zen_plataform tem `KEYMOD_SUPER` e acrescentar um `meta` ao `IO::KeyEvent` e ao `KeyEvent` e uma
  mudanca pequena (ZenApp.cpp, `addKeyEvent`), nao feita aqui.
- **Regras**: so de leitura copia mas nao corta nem cola (a tecla e gasta: Shift+Delete nao apaga); **uma palavra-passe nao copia nem corta** (o `Ctrl+C` copiava a palavra-passe: corrigido), e cola;
  um campo de uma linha deixa cair as quebras de linha do que cola; `NumberOnly` recusa uma colagem com letras (como com Ctrl+V); colar sobre uma seleccao substitui-a.
- **Menu do botao direito**: Cut, Copy, Paste, Delete, uma linha, Select All (os atalhos escritos); cada item **desactivado** onde nao se aplica (sem seleccao: Cut, Copy e Delete; so de leitura: tudo menos Copy e
  Select All; palavra-passe: Cut e Copy; area de transferencia vazia: Paste; sem texto: Select All). E um `Menu` feito para aquele clique, do popup (`Menu::execOwned`), com o campo como dono: **um campo que
  sai da janela com o menu aberto fecha-o** (`WidgetApp::notifyWidgetRemoved`, o protector de apagamentos da biblioteca), e um tratador de `textChanged` que tira o campo ao colar ou apagar pelo menu nao
  deixa nada aberto (testado sob ASan). Tema como os outros popups: imagens `docs/img/round10/textmenu-<preset>.png` (esquerda: campo com seleccao, todos os itens; direita: palavra-passe seleccionada,
  Cut e Copy desactivados). Um clique direito dentro da seleccao mantem-na; fora, poe o cursor ali. O widget diz `ownsContextMenu()` (novo, em `Widget`): o menu de um pai nao tapa o do campo, e um
  `setContextMenu` posto no proprio campo continua a ganhar.
- **Botao do meio**: cola no ponto do clique a seleccao **PRIMARY**; seleccionar com o rato (ao largar) ou Ctrl+A / "Select All" poe a seleccao nela (uma palavra-passe nunca).
  **Onde esta o gancho do PRIMARY**: `IO::getPrimaryText` / `IO::setPrimaryText` (`include/igui/widgets/Retained.hpp`, ponteiros para funcoes como os da area de transferencia), lidos por
  `WidgetApp::getPrimaryText()` (**sem PRIMARY cai na area de transferencia normal**: o botao do meio cola) e `WidgetApp::setPrimaryText()` (sem PRIMARY nao faz nada); no adaptador zen,
  **`ig::zen::App::setPrimaryHooks(get, set)`** (`adapters/zen/include/igui_zen/ZenApp.hpp`; `create()` em `ZenApp.cpp` leva-os ao IO). Hoje nao ha nenhum, por isso o botao do meio cola a area de
  transferencia. Quando a zen_plataforma tiver `clipboard_get_primary` / `clipboard_set_primary` (e a pergunta de suporte) a ligacao e uma linha onde a aplicacao arranca:
  `zen.setPrimaryHooks([] { return ig::retained::String(clipboard_get_primary()); }, [](const char* t) { clipboard_set_primary(t); });` (ou dentro de `create()`, ao lado de `io.getClipboardText`).
- **Que campos**: `TextInput` (e com ele o campo de texto do `SpinSlider`, o `FileDialog`, o seletor de cor...), `TextEdit` e o `CodeEditor` (cortar e colar sao **um passo de undo**: `replaceRange`; o undo
  e o redo restauram o que a colagem substituiu), `SearchBar`: teclas, menu e botao do meio. Sem seleccao (uma linha com um cursor): Copy, Cut e Delete trabalham na linha toda, Paste entra no cursor
  (`LineClipTarget`). Os que so tem **teclas** (sem menu nem botao do meio, o rato destes widgets ja faz outras coisas): a linha de entrada da `ConsoleWidget` (Ctrl+C continua a copiar a linha seleccionada
  do registo: a linha copia-se com Ctrl+Insert), o editor de celulas da `DataGrid`, da `TreeGrid` e da `PropertyGrid` (que antes nao colava nada), e as **barras do `CodeEditor`** (procurar, substituir, ir
  para a linha: o Ctrl+V ia para o texto do editor, por baixo da barra; agora cola na barra, e a barra "ir para a linha" so toma os digitos).
- **Testes** (`igui_retained_textclip`, entrada injectada): cada atalho e cada item do menu em `TextInput`, `TextEdit` e `CodeEditor`; a seleccao substituida ao colar; undo e redo no `CodeEditor`; so de leitura
  recusa cortar e colar e deixa copiar; palavra-passe recusa copiar e cortar; so numeros; o menu com cada combinacao de estados; clique fora fecha o menu; o campo que sai com o menu aberto e o
  tratador que o tira; o botao do meio com e sem PRIMARY (a area de transferencia nao e tocada com PRIMARY); a seleccao pelo rato, o duplo clique e Ctrl+A como PRIMARY; `SearchBar`, a linha da consola, uma
  celula da `DataGrid` e as barras do editor. Nao ha teste da `TreeGrid` nem da `PropertyGrid` (a mesma linha de codigo que a `DataGrid`) nem das teclas com varios cursores do `CodeEditor` (a colagem
  vai para o cursor principal, como com Ctrl+V).
- **Nao feito**: Escape nao fecha o menu (nenhum menu da biblioteca tem teclado: fecha com um clique fora); `meta`/Cmd (acima).

## 6. Verificacao

Instrucoes por cena do banco de comparacao (`tools/bench_compare.sh`, so contagem de instrucoes, rasteriador por software, antes = `origin/main` `fd8863d`; depois = esta ronda inteira, com a parte 3 e a area de transferencia da seccao 7):

| cena | antes | depois | |
|---|---|---|---|
| A (parado / rato) | 3 912 / 7 645 | 3 979 / 7 518 | +67 (+1,7 por cento, ~4 mil instrucoes no total) / -127 |
| B (parado / rato / clique) | 1 730 241 / 1 756 509 / 1 806 692 | 1 730 468 / 1 756 570 / 1 806 451 | +0,01 / 0,00 / -0,01 por cento |
| C (parado / rato / roda) | 209 487 / 214 100 / 215 342 | 209 668 / 213 840 / 215 356 | +0,09 / -0,12 / 0,00 por cento |
| D (arvore: parado / rato / roda) | 131 325 / 135 656 / 137 576 | 132 572 / 136 737 / 138 794 | +0,9 por cento (a arvore pinta reticencias e o segundo texto) |
| E (editor: parado / rato / roda / escrever) | 818 554 / 822 694 / 823 570 / 3 750 363 | 818 685 / 822 632 / 823 527 / 3 750 032 | +0,02 / -0,01 / -0,01 / -0,01 por cento |
| F (parado / rato) | 1 911 986 / 1 917 086 | 1 912 303 / 1 917 237 | +0,02 / +0,01 por cento |

Duas coisas custaram instrucoes a meio da ronda e foram corrigidas: um chamador novo de `DrawList::mapPoint` e de `commands_.push_back` (para o callback e para a malha) fez o compilador deixar de os pôr em linha dentro de
`addLine`/`addCmd` (+1,3 por cento na cena F, +38 na A): o callback e a malha usam agora uma funcao propria e `resize`; e a `TreeView` media o texto de todas as linhas visiveis em cada frame: so mede o que talvez nao caiba e guarda a largura no no.
O `bench/retained/draw.hash` (o editor e o piano roll no escuro) **nao mudou**.

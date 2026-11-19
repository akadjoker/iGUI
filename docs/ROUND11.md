# Ronda 11: o iGUI nas dependencias de agora

O iGUI era construido e testado com versoes velhas das duas dependencias. Esta ronda passa-o para as `main` de agora, usa o que elas trouxeram e continua nas perdas medidas. Tudo o que esta aqui foi
corrido; o que nao se fez esta na ultima seccao.

## 1. Versoes

| | antes (ate `bd3d3c1`) | agora |
|---|---|---|
| containers (`ct`), o submodulo `third_party/containers` | `34169bc` | **`63793ca`** (commit proprio: o ponteiro do submodulo) |
| zen_plataform (um checkout ao lado, `IGUI_ZEN_PLATFORM_DIR`, por omissao `../zen_plataform`; **nao e submodulo**) | `db76a13` | **`a62dd5c`** |

A zen_plataform nao e fixada por submodulo: o `CMakeLists.txt` le o `platform.h` do checkout e para com uma mensagem se lhe faltar `platform_wait_events` ou `clipboard_has_primary_support`
("precisa de zen_plataform a62dd5c ou mais nova"), em vez de um erro de compilador; o `README.md` diz as duas versoes testadas.

## 2. O que a mudanca de versoes partiu (o mesmo commit `bd3d3c1`, so com as dependencias novas)

`tools/check.sh` sobre o `origin/main` (`bd3d3c1`) sem tocar numa linha, com a ct `63793ca` e a zen_plataform `743ea31` (a `a62dd5c` so juntou a seleccao PRIMARY e a regra do texto): o estagio `hash` **passa** (o
`bench/retained/draw.hash` nao se mexe com as dependencias novas), o estagio `asan` (Debug) **passa 59 de 59**, o estagio `release` falhava **11 de 59**:

| teste | falha | causa |
|---|---|---|
| `igui_symbols`, `igui_widget_gallery`, `igui_item_status`, `igui_numeric_input`, `igui_windows`, `igui_code_outline`, `igui_chart`, `igui_text_sizes`, `igui_code_editor_keys`, `igui_original_widgets`, `igui_zen_sidetabs` (11) | SIGSEGV, `Assertion` ou falhas de leitura, so em Release | **contrato mais estrito da ct**: o `HashMap` e o `HashSet` guardam uma versao da tabela quando `CT_DEBUG_ITERATORS` esta ligado, e a ct liga-o sozinha quando `NDEBUG` nao esta definido (`ct/detail/utils.hpp`); tem de ser igual em todos os ficheiros do programa. Estes testes sao feitos de `assert()` e tiravam o `NDEBUG` (`-UNDEBUG`) com a biblioteca construida com ele: o teste e a biblioteca discordavam do tamanho de cada classe com um `HashSet` (o `SyntaxHighlighter` tem tres) e rebentava-se no destrutor |

Correccao: `tests/KeepAsserts.hpp`, forcado (`-include`) na frente de cada um destes alvos, le a ct **com o `NDEBUG` da biblioteca** e so depois deixa o `assert()` voltar. Nenhuma linha da biblioteca ou do adaptador teve de mudar:
a compilacao da biblioteca, do adaptador e dos programas de comparacao deu **zero erros e zero avisos** com as dependencias novas. A regra fica no `README.md` para as aplicacoes (todos os ficheiros de um programa
concordam sobre `NDEBUG`).

**O banco de comparacao (`tools/bench_compare.sh`) tambem tinha esta armadilha**: construido com `-O2` sem `NDEBUG` (as flags da ronda 10), com a ct nova ficava com `CT_DEBUG_ITERATORS` (os `HashMap` e `HashSet` maiores, com
iteradores verificados): as contagens eram as de um build de depuracao (o `TreeView` ficava 16 bytes maior). As flags passaram a `-O2 -DNDEBUG` (comentario do script) e **todas as medidas desta ronda sao assim**; a de
antes da mudanca (as velhas) foi feita sem `NDEBUG` mas a biblioteca nao tem `assert()` e a ct velha nao tinha o modo, por isso as contagens de uma frame parada sao as mesmas com ou sem ele.

### 2.1 Antes e depois, so a mudanca de versoes (o mesmo commit `bd3d3c1`) e a ronda

Instrucoes de espaco de utilizador do lado do GUI de uma frame (`tools/bench_compare.sh`, o `icount`: nao depende da maquina), as tres colunas com o mesmo compilador e as mesmas flags (`-O2 -DNDEBUG`): **antes** = `bd3d3c1` com a ct `34169bc`
e a zen_plataform `db76a13`; **so a mudanca** = o mesmo `bd3d3c1` sem tocar numa linha, com a ct `63793ca` e a zen_plataform `a62dd5c`; **depois** = o ramo desta ronda (ate ao fim da parte 1). Uma frame de cada, porque as contagens nao variam.

Por software (em GL as contagens andam com a mesma percentagem: B parado 1 730 707, 1 709 188 e 1 681 449):

| cena / fase | antes | so a mudanca | depois | mudanca (a mudanca / a ronda) |
|---|---:|---:|---:|---|
| A hover | 7 518 | 7 297 | 7 351 | -2.94% / +0.74% |
| B idle | 1 730 482 | 1 708 933 | 1 681 154 | -1.25% / -1.63% |
| B hover | 1 756 584 | 1 735 052 | 1 707 273 | -1.23% / -1.60% |
| B click | 1 806 592 | 1 784 689 | 1 756 881 | -1.21% / -1.56% |
| C idle | 209 668 | 209 280 | 205 579 | -0.19% / -1.77% |
| C hover | 213 867 | 213 469 | 209 768 | -0.19% / -1.73% |
| C scroll | 215 357 | 214 839 | 211 113 | -0.24% / -1.73% |
| D idle | 132 599 | 132 387 | 129 866 | -0.16% / -1.90% |
| D hover | 136 737 | 136 611 | 134 063 | -0.09% / -1.87% |
| D scroll | 138 767 | 138 457 | 135 938 | -0.22% / -1.82% |
| D (frame que fecha tudo) | 154 689 | 153 984 | 151 540 | -0.46% / -1.59% |
| D (frame que abre tudo: 10 000 nos) | 581 679 | 611 539 | 608 989 | +5.13% / -0.42% |
| E idle | 818 673 | 820 319 | 805 915 | +0.20% / -1.76% |
| E hover | 822 647 | 824 256 | 809 879 | +0.20% / -1.74% |
| E scroll | 823 522 | 825 001 | 810 611 | +0.18% / -1.74% |
| E typing | 3 750 013 | 8 527 701 | 8 512 490 | +127.40% / -0.18% |
| F idle | 1 912 303 | 1 916 596 | 1 883 917 | +0.22% / -1.71% |
| F hover | 1 917 237 | 1 921 520 | 1 888 868 | +0.22% / -1.70% |


**O que e da mudanca de versoes e o que e da ronda:**

- **So a mudanca**: B, C, D menos 0,1 a 1,2 por cento (a ct, sem mexer no iGUI); E e F mais 0,2 por cento (so deslocamentos de codigo); A menos 3 por cento. **Duas contagens sobem e nao sao do iGUI**:
  (i) **E, escrever: 3,75 M para 8,53 M (software) e para 9,21 M (GL)**: a zen_plataform abriu o metodo de entrada interno do Xlib no arranque, e agora abre-o na **primeira tecla** (`COMPARACAO_CORE.md`: "a abertura
  do metodo de entrada interno, uma vez"); a medida de escrever e uma frame com a primeira tecla do processo, e os 4,8 M sao a abertura (perfil `ICPROF`: 2,9 M em `libX11` e 2,0 M em `libc`, nada em codigo do iGUI). Paga-se uma vez;
  (ii) **D, a frame que abre os 10 000 nos: 581 679 para 611 539 (+5,1 por cento)**: o `TreeView::flattenNode` (449 400 para 479 400 instrucoes) gasta 3 a mais por no; o codigo e igual, o compilador guarda um
  registo a mais (`push %r15`) com os cabecalhos novos da ct (a funcao e a mesma: `flatRows_.push_back({ n, depth })` e um ciclo sobre os filhos); **atribuido a ct, o iGUI nao o recuperou**.
- **A memoria e o arranque, so a mudanca** (da zen_plataform: o metodo de entrada e o GLX ja nao se tocam no arranque): PSS por software **26,6 para 12,6 MB** (cena A) e 31,4 para 14,4 (F); em GL 53,8 para 51,3 (A) e 71,6 para 57,9 (F);
  CPU do arranque por software 70 para 29 ms (A). A memoria heap por software desce 0,6 MB (a ct).
- **A ronda** (`append_uninitialized` e `resize_for_overwrite` na pintura, e os rotulos pelas conversoes da ct): **menos 1,6 a 1,9 por cento em todas as cenas**: B parado 1 708 933 para 1 681 154 (**-27 779**),
  F parado 1 916 596 para 1 883 917 (**-32 679**), C, D, E igual (-1,8 por cento). A cena A sobe 54 instrucoes (+0,74 por cento: a `Window::lastFrame` por janela e `idleSeconds` em `update()`). O estagio `hash` **nao mudou**
  com a mudanca nem com a ronda: `bench/retained/draw.hash` e igual.

Os outros comportamentos da ct que a ronda foi ver, um a um:

| comportamento | o que o iGUI fazia | o que se fez |
|---|---|---|
| `String::to_int` satura | `PropertyGrid::commitEdit` (`to_int`, `to_float`), `CodeEditor` ir-para-a-linha (`atoi`: indefinido acima do int) | ver 3; o ir-para-a-linha usa `to_int()` que satura e depois `min(linha, ultima)` |
| `to_float`/`to_double` sem locale e so com '.' | `strtod` (do locale do programa) em `SpinSlider`, `PropertyGrid`, ordenacao da `DataGrid` e da `TreeGrid`, `Chart::formatX`, `Gui.cpp` (campo numerico do imediato); `snprintf("%f")` em ~60 sitios | ver 3 |
| `HashMap` por cima de 128 bytes fora da tabela | `Gui.hpp` guarda estados de widgets em `HashMap<WidgetId, ...>` (`TableOrderState`, `ColorPickerState`...) | lido: ninguem guarda uma referencia a um valor pequeno de um `HashMap` por cima de uma insercao que se saiba; **nao se passou a depender da estabilidade dos grandes** |
| `CT_DEBUG_ITERATORS` | nenhum `HashMap`/`HashSet` e percorrido enquanto se apaga ou insere nele (lido: os ciclos sobre mapas de `Gui.cpp` so leem) | o estagio `asan` e Debug, por isso corre com `CT_DEBUG_ITERATORS` ligado: 59 de 59 sem abortar |
| `erase_if` | nao ha apagar-durante-ciclo em mapas | nada |
| destruicao de janelas diferida pela plataforma dentro de ganchos | `ZenApp::destroyWindow` corre no fim de `pump()` (nao dentro de um gancho) e a janela nao se usa depois do `window_destroy` (`w->platform = nullptr; delete w`) | `igui_zen_lifecycle` (pais fechados com dialogos abertos, janelas criadas e destruidas 8 vezes) passa, em Release e sob ASan |
| `key_mods` guardado | o adaptador ja nao o chamava (le `key_down`) | nada |
| nomes das threads (`thread_create` -> `create_thread`...) | o iGUI nao usa a API de threads | nada (o teste novo usa os nomes novos) |
| regra do texto (Ctrl ou Super nao escrevem) | ver 5 | nada a corrigir |

## 3. Numeros escritos por pessoas: uma regra

**A regra** (`include/igui/widgets/NumberText.hpp`, `numtext::`), para todo o campo numerico:

- **le**: espacos a volta, sinal, algarismos com **no maximo um separador decimal, '.' ou ','** (`1.5` e `1,5` sao 1,5; `.5`, `5.`, `,5`), expoente (`1e3`, `2,5E-2`), qualquer que seja o locale do programa. O separador e
  *so* um ponto decimal: `1,000` e 1, e um texto com **dois separadores** (`1,000.5`, `1.2.3`) **nao se le** (pode ler-se de duas maneiras: o campo fica com o valor que tinha). Nao se aceita `inf`, `nan`,
  hexadecimal, uma unidade ou outro texto atras, nem mais de 100 bytes. O arredondamento e o do double mais proximo, exacto (`ct::parse_double`);
- **fora dos limites**: o numero escrito fica no limite do campo (`numtext::parseClamped` / `parseInteger`): nunca da a volta (`4294967297` num campo de 0 a 50 e 50, nao 1) e nao se recusa; o campo mostra o limite;
  um inteiro corta-se para zero (`12,7` e 12);
- **mostra**: sempre '.', qualquer que seja o locale (`numtext::fixedInto`, `generalInto`, `cformat`, `shortest`). `fixedInto` escreve **o mesmo texto que `snprintf("%.*f")`** (comparado com 400 000 valores ao acaso e os casos
  de borda: iguais) sem o `snprintf` para o que um rotulo e (0 a 9 casas, menos de 1e15), e `generalInto` o de `"%.*g"` (`ct::detail::write_general`); `cformat` e um `snprintf` no locale "C" (`uselocale`, ou `_vsnprintf_l`)
  para o resto.

Onde se aplica: `SpinSlider` (o campo de escrever: `evaluateInputText`; antes trocava `,` por `.` e lia com `strtod`, que num locale de virgula parava no ponto: `1.5` dava 1), `PropertyGrid` (as linhas `Vec2/3/4`; as de `Float` e `Int` escrevem-se no
`SpinSlider` que elas contem), a ordenacao numerica da `DataGrid` e da `TreeGrid` (`numtext::leadingNumber`: o que o `strtod` lia, mas no locale "C" e, de caminho, sem `strtod` em cada comparacao; **so '.'** conta nas
celulas: e texto da aplicacao, e `1,234` pode ser milhares), `Chart::formatX` (comparava com `strtod`: num locale de virgula nunca dava o valor de volta), o ir-para-a-linha do `CodeEditor` e o campo numerico do core imediato
(`Gui.cpp`: `strtod` -> `ct::parse_double`). **Os campos de cor, data e hora nao leem numeros com `to_int`/`strtod`** (a `ColorPicker` mostra o hexadecimal e as componentes vao por `SpinSlider`; `DatePicker` e `TimePicker`
tem os seus segmentos por teclas de algarismos): nada a mudar.

Mostrar: o `Slider`, o `SpinBox`, o `SpinSlider`, os rotulos do `PlotChart`, as barras e legendas do `Chart`, `AudioWidgets`, `GizmoWidgets`, `AutomotiveWidgets`, o tamanho de ficheiros, a linha da consola: 58 chamadas de `snprintf` com
conversao de numeros passaram a `numtext::cformat`, e as dos rotulos do `PlotChart`, do `Slider`, do `SpinBox` e de `Range::numberText` a `fixedInto`/`generalInto`. **Nao feito**: o core imediato (`Gui.cpp`, `iGUI.cpp`,
`FileDialog.cpp`) mostra numeros com o `snprintf` e o formato que a aplicacao deu (`"%.3f"`): num locale de virgula escreve virgula; a regra de ler ja la esta (`parse_double`).

**Testes** (`igui_retained_round11`, `tests/test_retained_round11.cpp`): a regra de ler (50 textos), os limites, `fixedInto` e `generalInto` contra o `snprintf`, e **com o locale `pt_PT.UTF-8`** (instalado nesta maquina;
`de_DE`/`pt_BR` se nao, e o teste diz que saltou se nao ha nenhum): `1,5`, `1.5`, `0.1`, dois separadores, o que cada funcao escreve, `cformat` com uma virgula num `%s`, uma celula que comeca por `10.5`, os rotulos do
grafico, o `Slider`; e o `SpinSlider` real (clique, escrever, Enter) em C e em pt_PT: `1.5`, `1,5`, `-2,25`, `0,1`, `1e1`, `500` (o maximo), `-500,5` (o minimo), `1e300`, 20 algarismos, `4294967297` e `2147483648` (o maximo,
nao um valor que da a volta) e `1,000.5` (o valor fica).

## 4. O que a plataforma trouxe, e o que se usou

### 4.1 Parado: `platform_wait_events` e `platform_wake` (a)

`App::idleWait(int ms)` (o unico sitio onde o adaptador dorme) chama `platform_wait_events(ms / 1000.0)`; `ms < 0` e **sem limite**. `pump()` pergunta a cada janela que nao pintou quanto falta para precisar de uma frame
(`WidgetApp::nextFrameInMs()`: a mira que pisca, a dica que vence, um `requestFrameIn`; -1 quando nada espera o tempo), toma o menor, e espera por um evento de qualquer janela, por um `platform_wake()` ou por esse tempo. A
espera pode voltar mais cedo (um sinal, uma tecla premida, trabalho do `backend`): `pump()` da outra volta e pergunta aos widgets quanto falta. Antes: `time_sleep` de 8 ms no maximo.

- **`requestFrameFromAnyThread()` acorda o ciclo**: poe a marca e chama `IO::wakeLoop` (novo; o adaptador poe `platform_wake()`, que e para qualquer thread a qualquer hora); e **a unica funcao da biblioteca que se chama de
  outra thread** (escrever os dados primeiro; documentado em `WidgetApp.hpp`). `App::wake()` e o mesmo sem a marca, para um anfitriao. Temporizadores (`requestFrameIn`, a mira, as dicas) nao precisam de thread: entram no tempo da espera.
- **O relogio tem de continuar a andar enquanto o ciclo dorme.** O `io.deltaTime` do adaptador e cortado a 0,1 s (uma animacao nao salta depois de um bloqueio); o relogio da aplicacao (a mira, o duplo clique, o atraso
  das dicas) somava esse `deltaTime`: com esperas de meio segundo o relogio andava 0,1 s por espera e **um clique 5 s depois do primeiro era duplo clique**. `IO::idleSeconds` (novo) e o que o ciclo dormiu: entra no
  relogio e no cronometro da dica, e **nao** nas animacoes (`deltaTime` fica o da frame). Teste: `igui_retained_round11` (um duplo clique, 30 s de sono, um clique: sao dois cliques simples) e `igui_zen_idle` (o relogio
  conta os 3 s de espera inteiros).
- **O relogio de cada janela e o seu** (`Window::lastFrame`): antes `dt` era `agora - a ultima frame da janela principal`, que para as outras janelas eram microssegundos (a mira de uma janela secundaria nunca piscava).
- **`pump()` pode bloquear**: um programa que chama `pump()` num ciclo seu e conta passagens, ou que espera por uma condicao posta por uma thread sem `wake()`, ficava a andar de 8 em 8 ms e fica agora parado ate haver
  um evento. **`App::setMaxIdleWait(ms)`** (novo; -1 por omissao) limita a espera: 8 e o que o ciclo fazia. Os testes de janelas que contam passagens (`igui_zen_lifecycle`, `_input`, `_sidetabs`, `_plot_gl`,
  `_native_callback_gl`, `_screenshot_gl`) passaram a pedir 4 ms; o `cmp_igui` pede 50 ms (e a espera exacta ate ao fim da corrida na fase `idlecpu`).
- Com a janela que mostra as estatisticas (`Config::showStats`) a espera nao passa de 1 s (o titulo escreve-se uma vez por segundo).

**Medidas** (`tools/bench_compare.sh`, cena B, janela do formulario, nada a acontecer, 10 s, a mira nao pisca; maquina partilhada com carga de 7 a 13 em 12 CPUs):

| | antes (o ciclo de 8 ms, `bd3d3c1`, ct e zp novas) | agora |
|---|---:|---:|
| por software: CPU | 0,7 % | **0,0 %** |
| por software: acordares por segundo | 123 | **0,1** (um no fim dos 10 s, o do proprio banco) |
| OpenGL (llvmpipe): CPU | 0,7 % | **0,0 %** |
| OpenGL: acordares por segundo | 123 | **0,1** |
| latencia de um clique, por software (mediana / minimo, 40 cliques) | 33,1 / 31,1 ms | 32,4 / 27,4 ms |
| latencia de um clique, GL (mediana / minimo) | 16,4 / 15,0 ms | **10,6 / 8,4 ms** |
| frames para mostrar o clique | 1 | 1 |

(Os tempos foram medidos com prioridade baixa (`nice 15`) numa maquina partilhada: valem pela ordem de grandeza; **o que nao depende da maquina sao as contagens**: acordares por segundo (do `getrusage`, uma
contagem) e frames. A latencia por software e a frame do rasterizador (uns 30 ms) mais o que sobra; em GL o ganho e o do sono de ate 8 ms que ja nao existe. Os 123 acordares por segundo de antes eram 8 ms de sono mais a frame;
a ronda 9 mediu 118 e 119 e a ronda 10 123.)

Teste `igui_zen_idle` (janela real numa Xvfb privada, rasterizador por software): 3 s sem nada: **1 passagem** do ciclo e nenhuma frame pintada, o relogio andou 3,0 s; com a mira a piscar numa caixa de texto: **6 frames
pintadas em 3 s** (uma por meio segundo) e 13 passagens (duas por cada meia volta: a que pinta e a que adormece); uma thread que chama `requestFrameFromAnyThread` aos 0,4 s: a frame pintada **1,4 ms** depois; um movimento do ponteiro feito pela extensao XTEST de outra
ligacao: a frame pintada **1,3 ms** depois do movimento (sem os 8 ms de sono). Antes de os 8 ms irem embora, este ultimo era ate 8 ms mais.

### 4.2 A seleccao PRIMARY (b)

A `a62dd5c` traz `clipboard_has_primary_support`, `clipboard_get_primary` e `clipboard_set_primary`. `ZenApp::create()` liga-os sozinho a `IO::getPrimaryText` / `setPrimaryText` quando o primeiro e verdadeiro (X11 sempre,
um compositor Wayland que a ofereca, o `fake`), salvo se a aplicacao pos os seus com `setPrimaryHooks` antes de `create()` (agora tambem aceite antes: guarda-os). Sem PRIMARY (Windows, web, Android) fica o que era: o botao do
meio cola a area de transferencia normal. Nenhuma aplicacao tem de escrever codigo. **Nao tem teste proprio nesta ronda**: o caminho da biblioteca (`igui_retained_textclip`: o botao do meio com e sem PRIMARY, por ganchos) ja o
cobria, e o do adaptador e uma linha; a zen_plataform testa o seu lado.

### 4.3 `resize_for_overwrite` / `append_uninitialized` (c)

`DrawList::appendVertices` e `appendIndices` (a pintura dos widgets), a fusao de listas, o `QuadBatch`, a camada de envelopes do `PlotChart` e a copia dos dados de uma serie (`setSeriesDataCopy`) escrevem tudo o que
pediram a seguir ao `resize`: passam a nao o preencher com zeros. Cada chamador de `appendVertices` / `appendIndices` foi lido (19) e o `draw.hash` e todos os douradados (`kDarkGolden`, `kPresetGolden`...) nao mudaram.
**Nao se tocou** o `allocIndices` do core imediato (7 chamadores, alguns escrevem condicionalmente).

### 4.4 Numeros nos caminhos quentes (d)

Ver 3: os rotulos dos eixos do `PlotChart` (`formatTimeInto`, `formatValueInto`, a leitura do cursor `%.6g`), o `Slider`, o `SpinBox` e `Range::numberText` passam pelas conversoes da `ct` (`write_general`), com o **mesmo
texto** (provado contra o `snprintf`); o resto (58 sitios) por `cformat`. O texto de edicao de um `Vec` da `PropertyGrid` continua a ser `%.9g` (so com '.' garantido).

### 4.5 A tecla Super (e)

`IO::KeyEvent::super`, `IO::keySuper` e `KeyEvent::super` do widget (campos novos; `IO::addKeyEvent(..., bool super = false)`); o adaptador preenche-os de `KEYMOD_SUPER` e de `KEY_LEFT_SUPER`/`KEY_RIGHT_SUPER`. **Nenhum widget age
sobre ela ainda**: o `TextClip` e os atalhos do menu poderao usar Cmd nos presets do macOS.

### 4.6 A regra do texto da `a62dd5c` (Ctrl ou Super nao escrevem)

Lido: o adaptador so passa `EVENT_CHAR` com `c >= 32 && c != 127` (nunca passou Ctrl+letra, que vinha como 1..26), e **nenhum widget decide um atalho pelo caracter** (todos os atalhos sao `onKeyPress` com a tecla e os
modificadores: `TextClip`, o `CodeEditor`, a `ConsoleWidget`, o `PlotChart`). Os que dependiam do texto de Ctrl+algarismo ou Ctrl+espaco (que antes escreviam um `1` ou um espaco no campo) agora nao escrevem nada: e o
pretendido. Shift+Insert, Ctrl+Insert e Shift+Delete ja chegam como `Key::Insert` / `Key::Delete` com o modificador (`sdlKey` mapeia `KEY_INSERT`; `TextClip::onKey` trata-os): sem mudancas.

## 5. Defeitos e pedidos

- **zen_plataform**: nada partido. Um pedido: o `README`/`platform.h` poderia dizer que **a abertura do metodo de entrada na primeira tecla** custa 4,8 M de instrucoes no X11 com `LC_ALL=C` (so a primeira); o banco do iGUI mede uma frame com
  a primeira tecla e le-a como "escrever +127 por cento" (esta ronda explica-o; um aquecimento no `cmp_igui` com uma tecla antes de medir resolveria do lado de ca, nao se fez).
- **ct (containers)**: (1) **`CT_DEBUG_ITERATORS` nao se desliga**: com `NDEBUG` indefinido liga-se sempre (`detail/utils.hpp`), por isso um ficheiro de teste que tire o `NDEBUG` para manter o `assert` (como os do iGUI)
  nao se pode alinhar com uma biblioteca de Release; pedido: honrar `CT_DEBUG_ITERATORS=0` definido pelo utilizador (ou `CT_NO_DEBUG_ITERATORS`), e o iGUI deixaria o `KeepAsserts.hpp`. (2) `Vector::push_back` no
  `TreeView::flattenNode` gasta 3 instrucoes a mais por no com os cabecalhos novos (ver 2.1): nao e um defeito, e um custo medido, para quem quiser olhar para o codigo gerado.
- **iGUI, achados pelo caminho**: o relogio de uma janela secundaria andava a ritmo de microssegundos (a mira e as dicas de uma janela que nao e a principal nunca contavam): corrigido (`Window::lastFrame`); `atoi` do ir-para-a-linha
  era indefinido acima do int: corrigido; `Chart::formatX` comparava com `strtod` (errado num locale de virgula): corrigido.

## 6. O que nao se fez

- **O core imediato** (`Gui.cpp`, `iGUI.cpp`, `FileDialog.cpp`) mostra numeros com `snprintf` e o formato que a aplicacao deu: num locale de virgula escreve virgula. So se corrigiu a leitura (`parse_double`).
- **Os campos de cor, data e hora** nao foram tocados (nao leem numeros com `to_int`/`strtod`).
- **PRIMARY**: so ligado; nao ha teste do adaptador com `clipboard_has_primary_support()` (a zen_plataform testa o seu lado).
- **Um aquecimento do banco para a abertura do metodo de entrada** (ver 5).

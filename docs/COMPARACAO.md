# iGUI retido contra Dear ImGui, FLTK e GTK 3, e mais casos limite

Pedido do dono: numeros do iGUI retido (`ig::retained`) contra outras bibliotecas, onde o iGUI tem de ganhar ou empatar em velocidade e memoria
(uma perda corrige-se ou explica-se com provas), e mais testes de casos limite. Este documento diz o que se mediu e como, as tabelas, onde o iGUI
perde e porque, o que se mudou na biblioteca (com as instrucoes antes e depois) e os bugs que os casos limite acharam.

Tudo o que esta aqui foi corrido; o que nao se correu esta na seccao 8. As tabelas completas (uma por medida, as bibliotecas em colunas, as cenas
em linhas) estao no fim (seccao 9) e saem de `tools/bench_compare.sh`.

## 1. Resumo

Onde o iGUI ganha ou empata (instrucoes de utilizador do lado do GUI de uma frame, tabela 4; memoria por item, tabela 2):

- **Lista de 100000 linhas (C)**, uma frame com a roda do rato: 243 mil instrucoes contra 270 mil do ImGui, 828 mil do FLTK e 12,9 milhoes do GTK; memoria 33 bytes por
  linha contra 76 (FLTK) e 152 (GTK).
- **Arvore de 10000 nos (D)**: 163 mil instrucoes numa frame parada contra 21,2 milhoes do ImGui (o modo imediato refaz os 10100 nos todas as frames); com a
  roda 613 mil contra 21 milhoes (ImGui), 31 (FLTK) e 6,1 (GTK); "expandir tudo" e a frame seguinte 0,4 ms contra 4,2 ms (ImGui), 3,9 ms (FLTK), 2,5 ms (GTK).
- **Editor de texto com 5 MB (E)**: 1,0 milhao numa frame parada contra 4,7 milhoes (ImGui); com a roda 1,0 contra 4,8 (ImGui), 4,7 (FLTK) e 2,9 (GTK); escrever uma tecla 3,9 contra 7,3 (ImGui), 4,4 (FLTK) e 11,3 (GTK) milhoes.
- **Latencia de entrada**: 2 frames contra 3 do ImGui (o FLTK e o GTK fazem um redesenho por evento).
- **CPU parado a correr o ciclo da biblioteca (10 s)**: 90% (rasteriador por software) e 222% (OpenGL por software) contra 400% do ImGui, no mesmo ciclo sem espera.

Onde o iGUI perde (seccao 4, cada uma com a causa e a prova):

1. **Cenas densas de widgets (B: 200 widgets, F: 50 janelas)**: 2,71 e 2,96 milhoes de instrucoes por frame contra 0,67 e 0,75 milhoes do ImGui (4,1x e 3,9x); 281 e 401 chamadas
   de desenho contra 1 e 100. Feito: -6,7% e -5,5% (arcos e bordas), desenho identico. O resto e geometria (4,3x os vertices) e fica como proposta.
2. **Uma frame parada e o ciclo parado**: o FLTK gasta 942 instrucoes e o GTK 30 mil (cena A), e os dois 0% de CPU; o iGUI redesenha tudo todas as frames e nunca espera.
3. **Memoria de base**: 43 MB de PSS numa janela vazia (por software) e 55 MB (GL) contra 1,6 MB (FLTK), 23 MB (GTK) e 46 MB (ImGui, GL).
4. **Arranque**: 110 a 140 ms contra 6 a 40 ms do FLTK; igual ao ImGui.
5. **Memoria do editor de texto**: 428 bytes por linha de 55 contra 60 (FLTK), 88 (ImGui), 192 (GTK).
6. **Binario**: 2,0 MB sem simbolos contra 0,78 (ImGui) e 0,85 (FLTK); 760 KB sao a fonte embebida.
7. **Rasteriador por software**: 15 a 47 ms de CPU por frame que muda numa janela de 1000x700, contra 0,25 a 0,9 ms do FLTK (mais o servidor X) e 1,7 a 3,7 ms do GTK.

**Depois da ronda 9** (`docs/ROUND9.md`): as perdas 1, 2, 3 e 5 foram atacadas e a seccao 10 tem os numeros de antes e depois; a 4, a 6 e a 7 nao se tocaram. Em duas palavras: B 2,71 -> 1,72 milhoes de
instrucoes e 281 -> 1 chamadas de desenho (1,10x o ImGui no mesmo aspecto), F 2,96 -> 1,82 milhoes e 401 -> 1 (1,198x); PSS por software 42,5 -> 24,9 MB; CPU parado 70% -> 0,8% (por software) e 173% -> 0,7% (GL);
editor de texto 428 -> 200 bytes por linha; clique em GL 2 -> 1 frame. **Nao se atingiu**: memoria de base em GL abaixo do ImGui, menos de 100 bytes por linha, 1 a 2 acordares por segundo (118 a 119).

Bugs achados pelos casos limite (seccao 6, cada um com o teste que falhou antes e passa agora): `TextEdit` com tamanhos enormes (conversao de float fora do intervalo),
`Timeline` (um grupo de clips depois de o tratador a tirar da arvore: leitura de memoria libertada), `PropertyGrid` (tres: `commitEdit`, `spinChanged`, `spinStep`),
`CodeEditor` (uma linha de 64 KB com uma palavra repetida: segundos por frame), `BoxLayout` (100 chamadas ao heap por frame com 50 janelas).

## 2. Metodo

### 2.1 As cenas

A mesma cena em cada biblioteca (janela de 1000x700 em (0,0), sem gestor de janelas, sem decoracao):

| Cena | O que e |
|---|---|
| A | janela vazia |
| B | formulario de 200 widgets: 40 etiquetas, 40 botoes, 40 caixas, 40 deslizadores, 40 campos de texto, numa grelha de 8 x 25 celulas de 120 x 26 |
| C | lista de 100000 linhas ("Row 000123  item text"), com a roda |
| D | arvore de 100 grupos de 99 nos (10000 nos), expandida; "expandir tudo" e "recolher tudo" |
| E | editor de texto com 91000 linhas de 55 bytes (5 005 000 bytes), a roda e teclas |
| F | 50 janelas ou paineis de 260 x 120 sobrepostos em cascata, cada um com titulo, uma etiqueta, um botao e uma caixa |

Cada biblioteca com o seu widget de serie e o seu jeito de arrumar (nao e igual, e e dito aqui):

| | iGUI (retido) | Dear ImGui 1.92.5 | FLTK 1.4.4 | GTK 3.24.41 |
|---|---|---|---|---|
| B | `GridLayout` de 8 colunas | posicao de cada celula com `SetCursorPos` | posicoes absolutas | `GtkFixed` com posicoes absolutas |
| C | `ListBox` com 100000 `addItem` | `ListClipper` e um `Selectable` por linha | `Fl_Browser` com 100000 `add` | `GtkTreeView` + `GtkListStore`, `fixed_height_mode` |
| D | `TreeView` com 10100 `TreeNode` | 100 `TreeNodeEx` com 99 folhas cada, todas chamadas todas as frames (o idioma do modo imediato) | `Fl_Tree` | `GtkTreeView` + `GtkTreeStore` |
| E | `TextEdit` (nao o `CodeEditor`) | `InputTextMultiline` com um buffer de 5 MB | `Fl_Text_Editor` | `GtkTextView` |
| F | 50 `FloatWindow` com um `BoxLayout` | 50 `Begin`/`End` | 50 `Fl_Group` (paineis, nao janelas do X) | 50 `GtkFrame` num `GtkFixed` (nao janelas do X) |

O ImGui usa o tipo de letra de serie (13 px) e o iGUI o seu (16 px); o iGUI desenha cantos de 4 px com borda de 1 px, o ImGui de serie desenha cantos quadrados sem
borda (ver 4.1 para a comparacao com o mesmo desenho). O ImGui nao guarda dados (a lista, a arvore e o texto vivem na aplicacao): os "bytes por item" dele so
contam o que ele proprio reserva, e a aplicacao de teste gera as linhas ao vivo; o iGUI, o FLTK e o GTK guardam os dados no widget.

### 2.2 As medidas e como se tomaram

- **Arranque**: do inicio de `main()` ate ao fim da primeira frame apresentada (tempo de relogio e CPU do processo, todas as threads). O melhor de 6 corridas (a maquina
  esteve com carga 11 a 25: um arranque so se alonga com a carga). Instrucoes de arranque: **nao se contaram** (ver a seccao 8: o contador por passo nao chega a uma biblioteca de GL a arrancar).
- **Memoria**: RSS e PSS (`/proc/self/smaps_rollup`) depois da primeira frame e de 8 frames paradas; heap por um `LD_PRELOAD` (`bench/compare/malloc_count.c`)
  que conta todas as chamadas `malloc`/`calloc`/`realloc`/`memalign`/`free` do processo (o `operator new` tambem), em duas contagens: o processo todo e so a thread da GUI.
  Bytes por widget = heap da cena B menos heap da cena A, a dividir por 200; o mesmo para a linha (C), o no (D), a linha de texto (E), a janela (F). Chamadas e bytes
  para construir a cena, e chamadas ao heap por frame (so a thread da GUI, so o lado do GUI da frame).
- **CPU por frame**: o **lado do GUI** de uma frame e tudo o que a biblioteca faz para ter a frame pronta antes de a entregar ao desenho: ler os eventos, os widgets, a
  arrumacao, a pintura, a lista de desenho feita. Para o iGUI e uma sonda de quatro pontos no `ig::zen::App` (`setFrameProbe`: antes dos eventos, depois do `Render()`,
  depois do rasteriador ou do OpenGL, depois da apresentacao); para o ImGui e do `PollEvent` ao `ImGui::Render()`. Para o FLTK e o GTK nao ha essa fronteira (desenham eles
  mesmos, o GTK ate com o cairo no proprio processo): a medida e "tratar o evento e o redesenho que ele pede" (`Fl::wait(0)`, ou as iteracoes do ciclo da GLib ate a
  frame clock pintar). Cinco numeros por fase: **instrucoes** do lado do GUI (abaixo), **tempo de CPU** da thread nesse lado, **tempo de CPU** da frame inteira na thread
  (com o rasteriador, o envio ao GL e a apresentacao), **tempo de CPU do processo** por frame (todas as threads: o OpenGL por software rasteriza nas suas) e **tempo de CPU
  do servidor X** por frame (o que o FLTK deixa ao servidor desenhar). Mais as chamadas de desenho e os vertices que o iGUI e o ImGui entregam.
  As fases: parada (nada mudou), rato a passar sobre os widgets, roda, um clique, uma tecla escrita, "expandir tudo". Os eventos sao reais, do X, feitos pela extensao XTEST
  por uma segunda ligacao ao mesmo servidor.
- **Instrucoes**: os contadores do `perf` estao fechados (`kernel.perf_event_paranoid=4`), por isso conta-se por passo: o programa liga a bandeira de traco do processador no
  inicio do trecho, um tratador de `SIGTRAP` conta cada instrucao e desliga-a no fim (`bench/compare/cmp_common.h`, `ICOUNT=1`). So as instrucoes de utilizador da thread que
  corre o trecho (uma chamada ao sistema conta uma). Confirmou-se contra o contador por `ptrace` que as rondas anteriores usaram: 2 899 124 e 2 899 133 numa frame do iGUI.
  Uma frame custa segundos a contar (uns 300 mil por segundo), por isso conta-se uma frame de cada fase e nao um arranque. **Nao depende do ruido da maquina**; os
  tempos dependem (ver 2.3). Em GTK e FLTK o passo lento deixa os temporizadores (a frame clock do GTK) disparar dentro do trecho: as contagens do GTK incluem pinturas que
  uma corrida a velocidade real tambem faria, mas nao so a de um evento; leem-se como "um evento e o redesenho", nao como uma frame parada.
- **Parado**: cada biblioteca com o seu ciclo natural 10 s, a janela com a cena B e nada a acontecer, com a sincronia vertical ligada: `Fl::run()` (um temporizador de 10 s),
  `gtk_main()`, e para o iGUI e o ImGui o ciclo de exemplo (`pump()` / `PollEvent`+`SwapWindow`, sem esperas). CPU do processo / tempo de relogio, mudancas de contexto voluntarias
  por segundo (acordares) e frames por segundo.
- **Latencia**: um clique injectado numa caixa da cena B ate a frame que a mostra mudada estar apresentada: em frames (as dos ciclos que fazem frames) e em milissegundos.
- **Binario**: a menor aplicacao de cada biblioteca (`bench/compare/min_*.cpp`: uma janela com uma etiqueta), sem simbolos; e as bibliotecas.

O iGUI corre com o rasteriador por software (`iGUI soft`, a janela por pixeis) e com o OpenGL (`iGUI GL`); o ImGui com o OpenGL 3 do SDL2; as de GL com o llvmpipe do Mesa
(25.2.8, OpenGL 4.5), e o FLTK e o GTK pelo X11 (o GTK com cairo em memoria). Tudo compilado com -O2 (a biblioteca, o ImGui, as cenas; o FLTK com -O2 tambem; o GTK e o Mesa
sao os do sistema).

### 2.3 A maquina e o que nao e igual

AMD Ryzen 5 5500U (6 nucleos, 12 threads), 19 GB, Linux 7.0.0-43, g++ 13.3, Xvfb privado com `-noreset` (um por corrida, invisivel, em `xvfb-run -a`; nunca o ecra do utilizador; HOME e
XDG_* num directorio temporario; sem som). Cada comando num escopo `systemd-run --user --scope` com `MemoryMax=4G`. A maquina esteve partilhada com outros agentes: carga 8 a 25
durante as corridas. **Os tempos sao por isso mais fiaveis nas razoes grandes do que nas decimas**; as instrucoes nao tem esse problema. O arranque e o CPU parado variam mais (o
CPU parado foi de 90% a 36% para o iGUI por software entre as duas corridas, porque a carga mudou as frames por segundo; os numeros desta seccao 1 sao da corrida mais calma).

O que nao e comparar o mesmo:

- **Modo imediato contra retido.** O ImGui refaz a interface todas as frames por desenho; o FLTK e o GTK so redesenham o que mudou. O iGUI e retido mas repinta tudo todas as
  frames ("as aplicacoes correm continuamente e redesenham a interface em cada frame", `tests/test_retained_idle.cpp`): tem o estado dos widgets como o FLTK e o custo por frame de um ciclo continuo como o ImGui.
- **O FLTK desenha no servidor X.** O trabalho de rasterizar de um cliente Xlib e do processo Xvfb, que esta numa linha propria de cada tabela ("CPU do servidor X por frame"). O GTK 3
  rasteriza com o cairo no proprio processo (por isso as contagens dele incluem a rasterizacao).
- **Mais ou menos que a outra.** O `FloatWindow` do iGUI arrasta, fecha e redimensiona; o `Fl_Group` e o `GtkFrame` do teste nao. O `TreeView` do iGUI faz arrastar e multi-seleccao; o
  `ListClipper` do ImGui nao tem nada disso. O GTK faz acessibilidade, IME e temas; o iGUI e o ImGui nao.
- **Estilo.** O iGUI desenha cantos de 4 px e uma borda de 1 px; ver 4.1 para a comparacao com o ImGui com o mesmo desenho.
- **Qt e nao instalado**, e nao se construiu: fica de fora (o pedido era so as que estivessem instaladas). O Dear ImGui estava em `/media/projectos/projects/cpp/imgui-1.92.5` (so lido) e o
  FLTK 1.4.4 veio do tarball oficial (github.com/fltk/fltk, anonimo), construido num directorio da area de trabalho; nada se instalou no sistema nem se pos no repositorio.

## 3. Os numeros antes e depois da biblioteca mudar

Instrucoes de utilizador do lado do GUI de uma frame do iGUI (o mesmo programa, as mesmas cenas; "antes" e a biblioteca de origin/main 81ab5c0 mais a sonda; "depois" e esta arvore):

| cena | fase | antes | depois | variacao |
|---|---|---:|---:|---:|
| A | idle | 6 990 | 6 990 | +0.0% |
| A | hover | 9 837 | 9 837 | +0.0% |
| B | idle | 2 899 158 | 2 705 611 | -6.7% |
| B | hover | 2 924 536 | 2 731 043 | -6.6% |
| B | click | 2 975 456 | 2 782 073 | -6.5% |
| C | idle | 241 326 | 238 664 | -1.1% |
| C | hover | 244 803 | 241 900 | -1.2% |
| C | scroll | 246 656 | 243 643 | -1.2% |
| D | idle | 163 915 | 162 862 | -0.6% |
| D | hover | 167 412 | 166 332 | -0.6% |
| D | scroll | 614 017 | 612 769 | -0.2% |
| D | collapse_all_call | 543 | 543 | +0.0% |
| D | expand_all_call | 543 | 543 | +0.0% |
| E | idle | 1 005 306 | 1 003 532 | -0.2% |
| E | hover | 1 008 773 | 1 006 945 | -0.2% |
| E | scroll | 1 010 291 | 1 008 517 | -0.2% |
| E | typing | 3 945 421 | 3 943 922 | -0.0% |
| F | idle | 3 129 207 | 2 957 230 | -5.5% |
| F | hover | 3 133 852 | 2 961 473 | -5.5% |

Nada ficou mais lento (a coluna da variacao nunca e positiva); `igui_retained_bench --hash` e os seis checksums de `draw_hash` (um por cena, de A a F, do `cmp_igui`) iguais
antes e depois: a mudanca so desenha mais depressa, desenha o mesmo (`bench/retained/draw.hash`: 87b1fe59ac36adb1, 87b1fe59ac36adb1, 7956122198be0670, 957c5363d01cdd4a).

Chamadas ao heap por frame parada do iGUI na cena F: 103 antes, 3 depois (as 3 que ficam sao a `XkbGetState` da zen_plataform: ver 4.2).

## 4. Onde o iGUI perde, porque, e o que se fez

### 4.1 Cenas densas de widgets (B e F): 4x as instrucoes do ImGui

*(Ronda 9: (a), (b) e (c) abaixo fizeram-se, e mais; B 2,71 -> 1,72 milhoes, F 2,96 -> 1,82, 1 chamada de desenho em B e em F: seccao 10.1.)*

**Prova.** Frame parada da cena B, instrucoes por funcao (o contador por passo com o endereco de cada instrucao, `ICPROF=`; antes da mudanca): `DrawList::fillConvexPath` 26,5%,
`pathStroke` 16,1%, `sin`/`cos`/`sqrt` da `libm` 10,7%, `addText` 10,5%, `appendVertices` (o `resize` poe a zero o que a seguir se escreve) 7,2%, `pushCornerArc` 6,6%,
`findGlyph` 3,4%, `appendIndices` 2,9%, `calcTextSize` 2,5%, `addCmd` 2,1%. Ou seja, 70% e o desenho de cantos redondos: cada botao e um preenchimento e uma borda de
24 pontos, com franja anti-alias (2 vertices por ponto no preenchimento, 4 na borda), 125 vertices por widget: 24 932 vertices contra 5 732 do ImGui.

O custo por vertice e igual: iGUI 108 instrucoes por vertice (2,71 milhoes / 24 932), ImGui 117 (0,667 milhoes / 5 732). A diferenca e a quantidade de geometria que o desenho de serie
pede. Com o mesmo desenho (cantos de 4 e borda de 1 px nos dois: `cmp_imgui --rounded`) o ImGui faz 1 567 238 instrucoes e 16 452 vertices (95 por vertice): o iGUI e 1,7x. Com o iGUI de cantos
quadrados (`cmp_igui --flat`: 1 768 811 instrucoes, 13 092 vertices, porque a borda continua a desenhar-se) e o ImGui de serie (666 786, 5 732) e 2,7x.

**Feito** (commit "DrawList retido: ..."): os cossenos e senos de um arco de canto calculam-se uma vez por (canto, numero de segmentos) e guardam-se (a mesma expressao, float a float);
a direccao unitaria de cada aresta calcula-se uma vez e nao duas (o contorno de um preenchimento e de um traco tinha-a calculada para cada um dos dois pontos que a tocam); a
raiz quadrada do limite da ponta so se tira abaixo de d2 = 0,3 (acima nao ha nada a limitar). B: 2 899 158 -> 2 705 611 instrucoes (-6,7%); F: -5,5%; C, D, E: -0,2% a -1,2%. Os
checksums do desenho iguais.

**Nao feito, com a estimativa:**
(a) segmentos por canto pelo erro, como o modo imediato (`cornerSegments` da 5 para um raio 4; a flecha de 3 segmentos e 0,14 px): menos 1/3 dos pontos de cada canto, estimativa -20% a -25% das
instrucoes de B e F e -8 mil vertices. Mexe no `draw.hash` e no desenho em menos de 0,1 px: decisao do dono.
(b) fundir comandos de desenho: 281 comandos para 200 widgets (cada texto fecha um clip), 401 em F; no llvmpipe isso custa 15,7 ms de CPU da thread principal por frame contra 0,9 ms do ImGui (1
chamada; primeira corrida); num GPU real e barato. Pede os limites da geometria de cada comando (cerca de 4 instrucoes por indice: +14% do lado do GUI), por isso nao se fez sem uma medida em GPU.
(c) deixar de por a zero os vertices antes de os escrever (7%): precisa de `DrawVertex` com construtor ou de um `resize` sem inicializacao no `ct`.

### 4.2 Uma frame parada, e o ciclo parado

*(Ronda 9: feito do lado da biblioteca, 70% -> 0,8% de CPU parado; faltam os acordares por segundo, que pedem uma espera por eventos na zen_plataform: seccao 10.4 e `docs/ROUND9.md` 2.3.)*

O FLTK e o GTK nao fazem nada sem eventos: 942 instrucoes (FLTK), 30 mil (GTK, cena A; nas outras o passo lento deixa disparar a pintura da frame clock) e 0,0% de CPU, 0,1 a 0,6 acordares por segundo (o temporizador de 10 s). O iGUI repinta e entrega tudo todas as frames
(2,7 milhoes em B) e o `App::run`/`pump` nunca espera: 36% a 90% de CPU (por software) e 100% a 222% (GL) para nada, 440 a 1500 acordares por segundo (o ImGui 2000 a 7000). O ImGui e igual de ciclo
(115% a 400%). Com um monitor real a sincronia vertical limitava isto a 60 frames por segundo; no Xvfb nao ha vblank e o ciclo gira.

Causa: nao ha estado de "nada mudou" na biblioteca (a pintura e sempre completa), e a zen_plataform so tem `window_begin_frame`/`poll_event` e `app_run`, nenhuma espera por eventos com tempo limite.
**Nao feito:** pede um sinal de "esta frame e igual a anterior" no `WidgetApp` (as animacoes, o cursor a piscar, as dicas e os videos dizem quando precisam de frame) e uma espera
na plataforma (a zen_plataform, que nao se mexeu). Ganho esperado: 0% de CPU parado e 1 a 2 acordares por segundo, como o FLTK. Tambem: o rasteriador por software, numa frame
igual, ja salta o rasterizar mas apresenta os 2,8 MB ao servidor X (1,5 ms de CPU do servidor por frame parada, tabela 4, linha do servidor X).

Os 3 acessos ao heap por frame parada e a pergunta ao servidor X que cada frame faz (um `XkbGetState`, que espera a resposta) vem da zen_plataform (`key_mods` em `backend_lock_state`): visto
com `MC_BIG`/`CMP_TRACE_ALLOCS` (o `malloc_count` escreve a pilha de cada chamada ao heap de uma frame). Fora do que se pode mexer aqui.

### 4.3 Memoria de base

*(Ronda 9: por software 42,5 -> 24,9 MB; em GL o heap 55,0 -> 46,0 MB, ainda 4 MB acima do ImGui: seccao 10.2.)*

PSS numa janela vazia: iGUI por software 43 MB, GL 55 MB, ImGui 46 MB (GL), GTK 23 MB, FLTK 1,6 MB. Chamadas ao heap de 500 KB ou mais no arranque do iGUI (por software): 11,2 MB (o buffer
do rasteriador com 2x2 amostras: 1000x700x4 bytes x 4), 4 paginas de 4,2 MB (o atlas de serie e a pagina dinamica de glifos 1024x1024 RGBA, e a copia de cada uma na textura do rasteriador),
2 x 2,8 MB (a imagem da janela). Medido por opcao (`cmp_igui`): `Config::defaultDynamicPage=false` (so o tamanho de glifo cozido) tira 8,7 MB de PSS (por software) e 7,8 MB (GL: 48,8 MB, empate
com o ImGui); `samples=1` tira 11,4 MB e corta o custo do rasteriador a 3 vezes (C com roda: 20,9 -> 6,0 ms; B com o rato: 45 -> 16 ms). **Nao feito** (sao decisoes de aparencia e de
arquitectura): pagina dinamica de 1 byte por pixel (R8) e so criada quando se pede um tamanho nao cozido (-3 a -8 MB), e sem a copia do atlas no rasteriador.

### 4.4 Arranque

110 a 140 ms (a janela, o atlas, o rasteriador) contra 6 a 40 ms do FLTK; o ImGui 107 a 130 (SDL e o contexto GL). Empate com o ImGui, perda contra o FLTK e (por pouco) o GTK em A. O
`startup_init_ms` do iGUI e do ImGui e igual (110 contra 107): nao ha um sitio do iGUI que domine. Nao se contaram instrucoes de arranque (seccao 8).

### 4.5 Editor de texto: 428 bytes por linha

*(Ronda 9: 428 -> 200 bytes por linha, sem o `wrapCache_`; o alvo de 100 nao se atingiu: seccao 10.3.)*

O `TextEdit` guarda cada linha num `String` (32 bytes e um bloco de heap) e, por cima, `wrapRows_` (um `Vector` de linhas visuais por linha), `lineWidths_`, `rowStarts_` e um
`wrapCache_` (`HashMap<String, WrapLine>`: **uma segunda copia de cada linha distinta** como chave, com as linhas visuais) que sobrevive entre reconstrucoes. 273 mil chamadas ao heap e 37,8 MB
alocados para 5 MB de texto. **Nao feito:** sem quebra de linha (o padrao) o `wrapCache_` nao poupa nada que uma remedicao nao refaca em 10 ms; deixar de o guardar e deixar a linha visual
unica implicita e uma proposta de -150 bytes por linha (de 428 para uns 270; o FLTK tem 60). Mexe no caminho de escrever (o `staleWrapLines_`), por isso pede as instrucoes por tecla dos dois lados.

### 4.6 Binario

`min_igui` 2 035 520 bytes sem simbolos, 16 bibliotecas dinamicas (3,2 MB alem da libc, X11 e afins); ImGui 780 984 (50 bibliotecas, 15,0 MB: SDL2, GL...), FLTK 845 304 (24, 3,9 MB), GTK 14 472
(67, 29,2 MB). Em estatico: `libigui_widgets.a` 5,7 MB, `libigui_core.a` 0,34 MB, `libigui_zen.a` 0,10 MB, `libplatform.a` 1,0 MB; `libfltk.a` 3,4 MB; os objectos do ImGui 1,25 MB. O maior simbolo do iGUI e a fonte
embebida (`DejaVuSans_ttf_data`, 760 KB, 37% do binario). Perda para o ImGui e o FLTK, por escolha (a fonte vai dentro); a tabela de simbolos (`nm --size-sort`) e a prova.

### 4.7 Rasteriador por software

Cada frame que muda rasteriza a janela toda (2x2 amostras): 15 a 47 ms de CPU da thread (C com a roda 18 ms, D 15 ms, B com o rato 45 ms, E a escrever 47 ms) contra 0,25 a 0,9 ms do FLTK (mais o servidor X) e 1,7 a 3,7 ms do GTK e 0,7 a 1,6 ms de CPU de thread do iGUI GL
(mais as threads do llvmpipe). Corta-se com menos amostras (4x menos com `samples=1`); **nao feito**: rasterizar so o rectangulo que mudou, que e o que o FLTK e o GTK fazem.

## 5. O que se mudou na biblioteca e nos programas

Ver `git log` do ramo `igui-compare`; em resumo:

- `ig::zen::App::setFrameProbe` (adapters/zen): quatro testes de um ponteiro nulo por frame, para as medidas.
- `src/widgets/Retained.cpp`: os arcos e as direccoes das arestas, 4.1 (e `Retained.hpp`: um buffer de trabalho).
- `src/widgets/Widget.cpp`: `BoxLayout::layout` deixa de alocar duas vezes por chamada (dois vectores por nivel de aninhamento, que ficam entre frames): uma janela flutuante arruma o seu conteudo
  todas as frames, 50 janelas faziam 100 chamadas ao heap por frame parada.
- `src/widgets/TextInputWidgets.cpp`, `Timeline.cpp`, `TreePropertyColorWidgets.cpp`, `CodeEditorTools.cpp`: os bugs da seccao 6.
- `bench/compare/`, `tools/bench_compare.sh`, `tools/bench_compare_table.py`: os programas e o guiao das medidas (opcao `IGUI_BUILD_COMPARE`, desligada por defeito, **fora da regra sem `std`**:
  nem o `cmake/CheckNoStd.cmake` nem o `tools/check.sh` olham para `bench/`; o `cmp_igui.cpp` nao usa a biblioteca standard mas os dos outros usam o C++ deles).

## 6. Casos limite: os testes e o que acharam

`tests/test_retained_round8.cpp` (alvo `igui_retained_round8_tests`, corre no `check.sh` em Release e sob ASan+UBSan+float-cast-overflow), com o corredor de casos da ronda 7 passado para `tests/VictimHarness.hpp`:

1. **Um tratador que tira o proprio widget a meio do sinal**, nos sinais que as rondas 5 a 7 deixaram: `GradientEditor.onChanged` (4 formas), `CurveEditor.onKeyAdded/onKeyRemoved/onKeyChanged`,
   `SpinSlider.grabbed` e `valueFocusEntered`, `Timeline.onClipEdited` (um clip e um grupo de tres) e `onKeyframeDropped` (um grupo de tres), `ConsoleWidget.onCommand` e `entryActivated`,
   `SymbolOutline.activated`, `ChoiceDialog.chosen`, `InputBox.accepted` e `cancelled`, `MessageBox.result`, `PropertyGrid` (o callback de uma linha float, int e string).
2. **Tamanho zero e enorme**: 22 tipos de widget com rectangulos de 0x0, 0xH, Wx0, 1x1, 0,001, 3x3000, 1e7, 1e30, 3e38 e negativo, arrumados, pintados, com o rato a passar, um clique, a roda e uma tecla; e a janela
   com 0x0, 1x1, 0xH, Wx0 e 100000x100000.
3. **0 e 1 milhao de itens**: `ListBox` (1 000 000 `addItem`, seleccao, teclas, roda, remover, limpar), `TreeView` (1 000 grupos de 999 nos, expandir e recolher, limpar), `ComboBox`.
4. **Texto vazio, UTF-8 invalido e 64 KB**: 15 textos (vazio, bytes de continuacao soltos, sequencias cortadas de 2, 3 e 4 bytes, substitutos, sobrelongas, acima de U+10FFFF, 0xFF, controlos e um NUL, 64 KB de letras, de
   palavras curtas, de bytes invalidos, de euros e de 8192 linhas) em `Label`, `Button`, `CheckBox` (e a sua dica), `TextInput`, `TextEdit`, `CodeEditor`, `ListBox`, `ComboBox`, `TreeView`,
   `Collapsible`, `ConsoleWidget` (registo e pesquisa) e o titulo de um `FloatWindow`, com escrita, apagar, desfazer e janelas estreitas.
5. **Mostrar, esconder e tirar a pressa**: 600 passos ao acaso (criar, esconder, tirar e fazer noutro pai, reordenar, rato, clique, trocar de estagio) sobre quatro paineis com os 22 tipos.
6. **Escala de pixeis do atlas** 0,5 / 1 / 2 / 3 mudada com a interface em pe (e o tamanho do ecra a acompanha), com rato, clique, roda e texto.

O que se viu falhar antes e passar depois (a biblioteca de origin/main com o teste de agora, um processo por caso, sob ASan):

| bug | caso da ronda 8 | com a biblioteca de origin/main | agora |
|---|---|---|---|
| `TextEdit`/`CodeEditor` pintam as barras de rolar com um float fora de um int | `sizes of every widget` | UBSan float-cast-overflow: `TextInputWidgets.cpp:1792: inf is outside the range of representable values of type 'int'` (o programa para) | passa (41 s) |
| `Timeline::onMouseRelease` continua a emitir depois de o tratador a tirar | `Timeline.onClipEdited by dragging a group of three clips` | ASan heap-use-after-free em `Signal<int,int>::emit` chamado de `Timeline::onMouseRelease` (`Timeline.cpp:1232`), memoria libertada por `Widget::removeChild` | passa |
| `PropertyGrid::commitEdit` | `PropertyGrid.onChange callback of a string row, by typing and Return` | ASan heap-use-after-free em `PropertyGrid::commitEdit` (`TreePropertyColorWidgets.cpp:1688`) | passa |
| `PropertyGrid::spinChanged` | `... of a float row, by a drag of its spin` | `propertyChanged` chegou 1 vez depois de a grelha sair (devia ser 0) | passa |
| `PropertyGrid::spinChanged` (inteiro) | `... of an int row, by a drag of its spin` | o mesmo | passa |
| `BoxLayout::layout` aloca dois vectores por chamada | `an idle frame of fifty windows, each with a box of widgets, does not allocate` (`test_retained_idle`) | `FAIL fifty windows with a box each` (100 chamadas ao heap por frame) | passa |
| `CodeEditor` realca ocorrencias com custo quadratico na linha | `hostile strings in text widgets`, so `64 KB of short words` | nao acaba: estado de saida 124 aos 120 s (e mais de 20 min sem limite) | acaba em 43 s o caso, 170 s as 15 cadeias |

Tudo isto com o ASan + UBSan + float-cast-overflow que o `check.sh` usa (`-fno-sanitize-recover`).

### 6.1 O que nao se fez da parte 2

- `Gizmo3D`, `VideoCanvas` (`boxGrabbed`, `boxDragged`, `boxReleased`, `boxDoubleClicked`, `hoveredPixel`), os popups do combo e da cor do `PropertyGrid`, o `FileDialog` (pede o sistema de ficheiros), o
  arrasto de `Range`/`Vec` do `PropertyGrid`, e na `Timeline` o redimensionamento, as envolventes, o Grab e a altura das pistas: os gestos pedem geometria que nao se calculou. As emissoes do
  `VideoCanvas` foram lidas (so o evento, que e uma local, vem depois do sinal): nao se correram. `GradientEditor` pelo rato nao se correu (so pela API); `SpinSlider.ungrabbed` pela arrastar
  nao se conseguiu fazer vir no modo "fora de uma frame" (o controlo falhou e retirou-se o caso).
- `Timeline.onKeyframeAdded` por duplo clique: o controlo falhou (o gesto do `Stage` nao leva a contagem de cliques) e retirou-se.
- A rapidez da leitura do `CodeEditor` nao tem um teste de tempo (um tempo sob ASan nao e fiavel): o teste e o dos 64 KB de palavras curtas que antes nao acabava (a tabela acima, o limite de 120 s).

## 7. Como repetir

```sh
cmake -S . -B ../igui-compare-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
  -DIGUI_BUILD_ZEN_ADAPTER=ON -DIGUI_BUILD_COMPARE=ON -DIGUI_BUILD_TESTS=OFF \
  -DIGUI_COMPARE_IMGUI_DIR=/caminho/imgui-1.92.5 -DIGUI_COMPARE_FLTK_CONFIG=/caminho/fltk/bin/fltk-config \
  -DIGUI_CONTAINERS_DIR=... -DIGUI_ZEN_PLATFORM_DIR=...
cmake --build ../igui-compare-build -j3
tools/bench_compare.sh --build ../igui-compare-build --runs 3        # a tabela fica em <build>/compare-out/tables.md
tools/bench_compare.sh --help
```

Cada programa tambem corre sozinho, num Xvfb: `cmp_<lib> <cena A-F> frames|idlecpu|latency [--n=N] [--soft|--gl] [--phases=idle,hover,...] [--flat|--rounded|--samples=N|--nodyn]`
(`ICOUNT=1` conta as instrucoes das frames; `ICPROF=ficheiro` guarda o endereco de cada instrucao; `LD_PRELOAD=.../libcmp_mcount.so` conta o heap e, com `MC_BIG=n` ou `CMP_TRACE_ALLOCS=1`,
escreve de onde vem). Os testes: `tools/check.sh`.

## 8. O que ficou por fazer, e o que nao se mediu

- **Instrucoes de arranque**: nao se contaram (o passo a passo leva dias a um arranque de GL; so ha tempo e CPU do processo).
- **Qt**: nao esta instalado, ficou de fora. **GTK por software de GL**: nao tem (cairo).
- **As perdas 4.1(a)(b)(c), 4.2, 4.3, 4.5, 4.7** ficavam como propostas com a estimativa; a ronda 9 fez a 4.1 (a)(b)(c), a 4.2 (do lado da biblioteca), a 4.3 e a 4.5 (a medias): seccao 10. A 4.7 (rasteriador
  por software, so o rectangulo sujo) e a espera por eventos da zen_plataform continuam por fazer.
- Os numeros de tempo (`*_us`) foram tomados com a maquina carregada (carga 8 a 25 em 12 CPUs): leiam-se as razoes grandes, nao as decimas. O CPU parado e as latencias em milissegundos variam entre corridas (as tabelas
  sao da segunda corrida, com mais carga; a primeira deu 90%/222%/400% de CPU parado).
- O ImGui sem o `ListClipper` e sem tipo de letra proprio (13 px) e o iGUI a 16 px: o texto nao tem o mesmo tamanho.
- A parte 2: seccao 6.1.

## 8.1 Verificacao

`tools/check.sh` (com `IGUI_CONTAINERS_DIR` e `IGUI_ZEN_PLATFORM_DIR` das rondas anteriores, `--jobs 3`, a base `origin/main`), na ponta deste ramo:

```
PASS nostd: nothing new with std:: or #include <...> in src, include and adapters since origin/main
PASS release: 100% tests passed, 0 tests failed out of 48
PASS asan: 100% tests passed, 0 tests failed out of 48
PASS hash: what the bench draws is what bench/retained/draw.hash says
----
check.sh: 4 stages, 0 failed
```

48 testes (os 47 de antes e `igui_retained_round8`, 287 s sob o ASan; o `igui_retained_round7` 161 s), nenhum Skipped. A tabela `std::` do `tools/nostd.baseline` nao cresceu (nenhuma linha nova com `std::` ou `#include <...>` em
`src`, `include` ou `adapters`; as da biblioteca nova usam `sqrtf`, `cosf` e `sinf`, que o codigo ja usa).

## 9. Tabelas

Geradas por `tools/bench_compare.sh` (os titulos das tabelas sao os do script, em ingles). Biblioteca: o ramo `igui-compare`. A tabela de arranque junta as duas corridas (o melhor);
as outras sao da segunda corrida (carga 8 a 25), a de CPU parado e de latencia tem por baixo a da primeira.


Medians of the runs of each cell (start-up: the best run; instruction counts: one run, they do not vary). n/a: the library has no such measure or the scene does not apply.

### 9.1. Startup to the first frame presented

**Wall time, ms (from main() to the end of the first presented frame)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 140 | 136 | 130 | 6 | 112 |
| B form, 200 widgets | 108 | 130 | 116 | 18 | 207 |
| C list, 100000 rows | 116 | 140 | 118 | 40 | 454 |
| D tree, 10000 nodes | 113 | 198 | 177 | 34 | 228 |
| E text editor, 5 MB | 513 | 557 | 135 | 34 | 2389 |
| F 50 windows/panels | 143 | 161 | 122 | 19 | 181 |

**CPU time of the process, ms (all threads)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 112 | 132 | 122 | 2 | 101 |
| B form, 200 widgets | 95 | 157 | 122 | 13 | 187 |
| C list, 100000 rows | 109 | 159 | 121 | 39 | 416 |
| D tree, 10000 nodes | 99 | 168 | 157 | 26 | 201 |
| E text editor, 5 MB | 501 | 558 | 152 | 31 | 2367 |
| F 50 windows/panels | 131 | 228 | 141 | 14 | 171 |

### 9.2. Memory, idle after the first frame

**PSS, MB (/proc/self/smaps_rollup)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 42.9 | 55.0 | 46.4 | 1.6 | 23.3 |
| B form, 200 widgets | 46.1 | 75.1 | 53.1 | 3.7 | 26.4 |
| C list, 100000 rows | 46.6 | 61.3 | 53.6 | 11.2 | 40.9 |
| D tree, 10000 nodes | 45.8 | 56.5 | 50.0 | 5.8 | 31.3 |
| E text editor, 5 MB | 81.8 | 94.2 | 56.0 | 8.4 | 41.9 |
| F 50 windows/panels | 45.9 | 81.7 | 54.3 | 3.7 | 26.0 |

**RSS, MB**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 87.6 | 117.7 | 102.5 | 6.5 | 76.7 |
| B form, 200 widgets | 90.0 | 134.9 | 114.7 | 9.9 | 83.1 |
| C list, 100000 rows | 90.0 | 120.4 | 113.8 | 17.2 | 96.8 |
| D tree, 10000 nodes | 89.2 | 119.1 | 113.8 | 11.9 | 90.2 |
| E text editor, 5 MB | 126.6 | 157.0 | 119.9 | 14.4 | 99.0 |
| F 50 windows/panels | 89.9 | 141.0 | 114.6 | 9.9 | 81.8 |

**Heap in use, MB (malloc: every allocation of the process, the GL driver included)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 28.92 | 54.97 | 41.94 | 0.17 | 2.57 |
| B form, 200 widgets | 30.79 | 74.50 | 44.04 | 0.61 | 5.13 |
| C list, 100000 rows | 32.06 | 58.52 | 43.58 | 7.44 | 17.09 |
| D tree, 10000 nodes | 30.67 | 57.06 | 43.39 | 2.63 | 5.11 |
| E text editor, 5 MB | 66.09 | 93.20 | 49.54 | 5.41 | 19.20 |
| F 50 windows/panels | 31.12 | 88.30 | 44.32 | 0.60 | 4.49 |

**Threads**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 1 | 26 | 26 | 1 | 4 |
| B form, 200 widgets | 1 | 26 | 26 | 1 | 8 |
| C list, 100000 rows | 1 | 26 | 26 | 1 | 5 |
| D tree, 10000 nodes | 1 | 26 | 26 | 1 | 5 |
| E text editor, 5 MB | 1 | 26 | 26 | 1 | 6 |
| F 50 windows/panels | 1 | 26 | 26 | 1 | 5 |

**Bytes per item: the heap of the scene minus the heap of the empty window, over the number of items (PSS in the second line of each library is the same difference in PSS)**

| item | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| widget (B, 200), heap | 9819 | 102368 | 11030 | 2294 | 13441 |
| widget (B, 200), PSS | 16768 | 105672 | 34985 | 10926 | 16492 |
| row (C, 100000), heap | 33 | 37 | 17 | 76 | 152 |
| row (C, 100000), PSS | 38 | 67 | 75 | 100 | 185 |
| tree node (D, 10100), heap | 181 | 217 | 151 | 256 | 264 |
| tree node (D, 10100), PSS | 291 | 160 | 374 | 435 | 838 |
| text line (E, 91000, 55 bytes each), heap | 428 | 440 | 88 | 60 | 192 |
| text line (E, 91000, 55 bytes each), PSS | 448 | 452 | 110 | 79 | 214 |
| window or panel (F, 50), heap | 46196 | 698953 | 49874 | 9008 | 40240 |
| window or panel (F, 50), PSS | 62403 | 560230 | 165499 | 43315 | 56791 |

### 9.3. Building the scene (heap calls, bytes and time)

**Heap calls to build the scene, thousands**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 0.0 | 0.0 | 0.0 | 0.0 | 0.5 |
| B form, 200 widgets | 0.3 | 0.3 | 0.0 | 0.4 | 101.4 |
| C list, 100000 rows | 0.0 | 0.0 | 0.0 | 116.8 | 1141.2 |
| D tree, 10000 nodes | 10.5 | 10.5 | 0.0 | 21.2 | 171.5 |
| E text editor, 5 MB | 273.0 | 273.0 | 0.0 | 0.1 | 250.1 |
| F 50 windows/panels | 0.5 | 0.5 | 0.0 | 0.3 | 77.1 |

**Heap bytes allocated while building, MB**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 0.00 | 0.00 | 0.00 | 0.00 | 0.04 |
| B form, 200 widgets | 0.16 | 0.16 | 0.00 | 0.04 | 11.14 |
| C list, 100000 rows | 6.00 | 6.00 | 0.00 | 10.84 | 39.29 |
| D tree, 10000 nodes | 1.50 | 1.50 | 0.00 | 2.43 | 10.22 |
| E text editor, 5 MB | 37.77 | 37.77 | 9.61 | 9.56 | 23.02 |
| F 50 windows/panels | 0.20 | 0.20 | 0.00 | 0.03 | 8.89 |

**Time to build the scene, ms**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 0.0 | 0.0 | 0.0 | 0.0 | 1.8 |
| B form, 200 widgets | 0.2 | 0.1 | 0.0 | 0.2 | 232.6 |
| C list, 100000 rows | 21.8 | 24.5 | 0.0 | 53.8 | 409.4 |
| D tree, 10000 nodes | 4.7 | 5.1 | 0.0 | 10.1 | 103.4 |
| E text editor, 5 MB | 101.0 | 93.4 | 24.8 | 39.5 | 177.4 |
| F 50 windows/panels | 0.3 | 0.5 | 0.0 | 0.1 | 112.8 |

### 9.4. One frame

**idle: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 6 990 | 7 516 | 19 618 | 942 | 30 187 |
| B form, 200 widgets | 2 705 611 | 2 706 137 | 666 786 | 942 | 62 598 541 |
| C list, 100000 rows | 238 664 | 238 922 | 252 858 | 4 616 | 1 346 034 |
| D tree, 10000 nodes | 162 862 | 163 388 | 21 202 425 | 942 | 1 326 653 |
| E text editor, 5 MB | 1 003 532 | 1 004 084 | 4 733 561 | 4 159 | 386 555 |
| F 50 windows/panels | 2 957 230 | 2 957 756 | 750 491 | 942 | 1 902 071 |

**idle: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 44 | 58 | 65 | 4 | 5 |
| B form, 200 widgets | 729 | 979 | 415 | 3 | 5 |
| C list, 100000 rows | 88 | 105 | 121 | 3 | 5 |
| D tree, 10000 nodes | 77 | 88 | 4048 | 3 | 5 |
| E text editor, 5 MB | 254 | 269 | 1215 | 4 | 5 |
| F 50 windows/panels | 943 | 985 | 541 | 3 | 5 |

**idle: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 1996 | 469 | 449 | 4 | 5 |
| B form, 200 widgets | 4446 | 27933 | 1978 | 3 | 5 |
| C list, 100000 rows | 1845 | 869 | 839 | 3 | 5 |
| D tree, 10000 nodes | 1838 | 662 | 4690 | 3 | 5 |
| E text editor, 5 MB | 2439 | 1613 | 2658 | 4 | 5 |
| F 50 windows/panels | 5441 | 34227 | 2708 | 3 | 5 |

**idle: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 2045 | 14491 | 3014 | 5 | 7 |
| B form, 200 widgets | 4617 | 84110 | 27465 | 5 | 7 |
| C list, 100000 rows | 1875 | 29986 | 16384 | 5 | 13 |
| D tree, 10000 nodes | 1891 | 28182 | 19404 | 4 | 14 |
| E text editor, 5 MB | 2488 | 36327 | 33904 | 6 | 11 |
| F 50 windows/panels | 5763 | 147937 | 44963 | 5 | 22 |

**idle: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 1529 | 1092 | 1068 | 0 | 0 |
| B form, 200 widgets | 1921 | 2155 | 2554 | 0 | 0 |
| C list, 100000 rows | 1391 | 915 | 910 | 0 | 1 |
| D tree, 10000 nodes | 1465 | 860 | 902 | 0 | 1 |
| E text editor, 5 MB | 1580 | 1019 | 1028 | 0 | 1 |
| F 50 windows/panels | 2408 | 1997 | 2176 | 0 | 4 |

**idle: heap calls per frame in the GUI side**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 3 | 3 | 0 | 0 | 0 |
| B form, 200 widgets | 3 | 3 | 0 | 0 | 0 |
| C list, 100000 rows | 3 | 3 | 0 | 0 | 0 |
| D tree, 10000 nodes | 3 | 3 | 0 | 0 | 0 |
| E text editor, 5 MB | 3 | 3 | 0 | 0 | 0 |
| F 50 windows/panels | 3 | 3 | 0 | 0 | 0 |

**Draw calls of the frame (what the library hands the backend)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 1 | 1 | 0 | n/a | n/a |
| B form, 200 widgets | 281 | 281 | 1 | n/a | n/a |
| C list, 100000 rows | 2 | 2 | 2 | n/a | n/a |
| D tree, 10000 nodes | 4 | 4 | 2 | n/a | n/a |
| E text editor, 5 MB | 5 | 5 | 2 | n/a | n/a |
| F 50 windows/panels | 401 | 401 | 100 | n/a | n/a |

**Vertices of the frame**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 4 | 4 | 0 | n/a | n/a |
| B form, 200 widgets | 24 932 | 24 932 | 5 732 | n/a | n/a |
| C list, 100000 rows | 2 020 | 2 020 | 2 900 | n/a | n/a |
| D tree, 10000 nodes | 1 338 | 1 338 | 1 722 | n/a | n/a |
| E text editor, 5 MB | 7 320 | 7 320 | 9 548 | n/a | n/a |
| F 50 windows/panels | 25 764 | 25 764 | 7 160 | n/a | n/a |

**hover: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 9 837 | 11 906 | 28 996 | 3 711 | 46 723 |
| B form, 200 widgets | 2 731 043 | 2 733 101 | 674 595 | 7 437 | 38 052 539 |
| C list, 100000 rows | 241 900 | 243 918 | 262 010 | 4 090 | 3 465 240 |
| D tree, 10000 nodes | 166 332 | 168 377 | 21 213 860 | 3 894 | 3 138 361 |
| E text editor, 5 MB | 1 006 945 | 1 008 828 | 4 742 968 | 281 493 | 1 191 621 |
| F 50 windows/panels | 2 961 473 | 2 963 518 | 758 901 | 4 468 | 67 439 |

**hover: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 60 | 76 | 88 | 12 | 134 |
| B form, 200 widgets | 817 | 1052 | 459 | 38 | 9038 |
| C list, 100000 rows | 97 | 117 | 135 | 13 | 993 |
| D tree, 10000 nodes | 85 | 96 | 4329 | 12 | 952 |
| E text editor, 5 MB | 274 | 270 | 1223 | 31 | 696 |
| F 50 windows/panels | 1130 | 1044 | 610 | 14 | 312 |

**hover: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 2016 | 493 | 476 | 12 | 134 |
| B form, 200 widgets | 50143 | 28659 | 1952 | 38 | 9038 |
| C list, 100000 rows | 1889 | 875 | 846 | 13 | 993 |
| D tree, 10000 nodes | 1828 | 686 | 4928 | 12 | 952 |
| E text editor, 5 MB | 2486 | 1584 | 2665 | 31 | 696 |
| F 50 windows/panels | 7873 | 33794 | 2869 | 14 | 312 |

**hover: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 2135 | 14458 | 3230 | 54 | 213 |
| B form, 200 widgets | 51648 | 85110 | 27435 | 111 | 20545 |
| C list, 100000 rows | 1956 | 29735 | 16462 | 51 | 1175 |
| D tree, 10000 nodes | 1895 | 28551 | 19490 | 49 | 1109 |
| E text editor, 5 MB | 2538 | 36616 | 33603 | 71 | 729 |
| F 50 windows/panels | 25572 | 150119 | 46709 | 66 | 1992 |

**hover: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 1691 | 1275 | 1197 | 42 | 103 |
| B form, 200 widgets | 2596 | 2458 | 2533 | 123 | 7954 |
| C list, 100000 rows | 1535 | 1022 | 1007 | 39 | 769 |
| D tree, 10000 nodes | 1531 | 961 | 963 | 41 | 751 |
| E text editor, 5 MB | 1741 | 1073 | 1151 | 82 | 275 |
| F 50 windows/panels | 3265 | 2201 | 2688 | 51 | 718 |

**hover: heap calls per frame in the GUI side**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| A empty window | 6 | 6 | 8 | 2 | 13 |
| B form, 200 widgets | 8 | 8 | 8 | 2 | 2596 |
| C list, 100000 rows | 6 | 6 | 8 | 2 | 360 |
| D tree, 10000 nodes | 6 | 6 | 8 | 2 | 370 |
| E text editor, 5 MB | 6 | 6 | 8 | 2 | 138 |
| F 50 windows/panels | 6 | 6 | 8 | 2 | 13 |

**scroll: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 243 643 | 245 697 | 269 715 | 827 786 | 12 898 953 |
| D tree, 10000 nodes | 612 769 | 613 295 | 21 204 350 | 30 804 363 | 6 077 642 |
| E text editor, 5 MB | 1 008 517 | 1 010 576 | 4 756 392 | 4 667 551 | 2 869 212 |

**scroll: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 102 | 115 | 159 | 250 | 2890 |
| D tree, 10000 nodes | 89 | 99 | 4391 | 769 | 2769 |
| E text editor, 5 MB | 278 | 271 | 1217 | 916 | 1702 |

**scroll: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 17938 | 863 | 886 | 250 | 2890 |
| D tree, 10000 nodes | 15288 | 657 | 5051 | 769 | 2769 |
| E text editor, 5 MB | 33198 | 1586 | 2720 | 916 | 1702 |

**scroll: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 17871 | 30128 | 16218 | 312 | 2939 |
| D tree, 10000 nodes | 14977 | 28376 | 19724 | 816 | 2781 |
| E text editor, 5 MB | 32783 | 36523 | 33791 | 980 | 1734 |

**scroll: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 1548 | 1031 | 1026 | 1569 | 3489 |
| D tree, 10000 nodes | 1601 | 938 | 1068 | 1562 | 3335 |
| E text editor, 5 MB | 1815 | 1111 | 1143 | 2495 | 6049 |

**scroll: heap calls per frame in the GUI side**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| C list, 100000 rows | 8 | 8 | 12 | 192 | 2214 |
| D tree, 10000 nodes | 8 | 8 | 12 | 12 | 2280 |
| E text editor, 5 MB | 8 | 8 | 12 | 510 | 458 |

**click: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 2 782 073 | 2 783 684 | 688 793 | 183 378 | 97 841 074 |

**click: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 901 | 1069 | 534 | 277 | 8434 |

**click: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 50288 | 28240 | 2113 | 277 | 8434 |

**click: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 53138 | 82945 | 27633 | 370 | 14670 |

**click: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 2447 | 2386 | 2705 | 459 | 3284 |

**click: heap calls per frame in the GUI side**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| B form, 200 widgets | 11 | 11 | 20 | 25 | 3143 |

**typing: instructions (user space) of the GUI side of the frame (input in, widgets, layout, paint, draw data ready)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 3 943 922 | 3 947 009 | 7 255 307 | 4 376 729 | 11 324 856 |

**typing: CPU time of the GUI side, microseconds (thread CPU time, median of the frames)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 1089 | 1011 | 3247 | 846 | 1707 |

**typing: CPU time of the whole frame in the main thread, microseconds (GUI side + rasteriser or GL submit + present)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 33965 | 2376 | 4628 | 846 | 1707 |

**typing: CPU time of the whole process per frame, microseconds (all threads: the software GL rasterises in its own)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 33135 | 37173 | 34834 | 927 | 1888 |

**typing: CPU time of the X server per frame, microseconds (what an X client leaves to the server to draw)**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 1781 | 1083 | 1182 | 2530 | 1537 |

**typing: heap calls per frame in the GUI side**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| E text editor, 5 MB | 30 | 30 | 4 | 448 | 396 |

### 9.5. Scene D: expand and collapse all (100 groups of 99 nodes)

**Expand all: the call, microseconds**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| D tree, 10000 nodes | 4 | 4 | 1 | 527 | 7223 |

**Expand all: the next frame, GUI side, microseconds**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| D tree, 10000 nodes | 386 | 292 | 4237 | 3880 | 2547 |

**Collapse all: the call, microseconds**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| D tree, 10000 nodes | 4 | 6 | 1 | 318 | 5336 |

**Collapse all: the next frame, GUI side, microseconds**

| scene | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| D tree, 10000 nodes | 85 | 101 | 119 | 256 | 4404 |

**Expand all and collapse all: instructions of the call**

| phase | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| expand_all_call | 543 | 543 | 28 | 504 576 | 55 205 606 |
| collapse_all_call | 543 | 543 | 28 | 484 776 | 27 889 730 |

### 9.6. Idle behaviour (scene B, the library's own main loop, 10 s, nothing happens; vsync on)

**Idle**

| measure | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| idle_cpu_pct | 36.4 | 100.8 | 114.8 | 0.0 | 0.0 |
| idle_wakeups_per_s | 437.5 | 691.2 | 2078.2 | 0.1 | 0.6 |
| idle_frames_per_s | 62.2 | 14.3 | 46.5 | 0.1 | 0.0 |

Primeira corrida (carga 11 a 15, mais calma; e a dos numeros da seccao 1):

| medida | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| idle_cpu_pct | 89.8 | 222.2 | 400.6 | 0.0 | 0.1 |
| idle_wakeups_per_s | 1538.4 | 1640.1 | 7259.0 | 0.1 | 0.8 |
| idle_frames_per_s | 262.8 | 36.7 | 168.7 | 0.1 | 0.1 |

### 9.7. Input latency (scene B, an injected click on a checkbox until the changed widget is drawn and presented)

**Latency**

| measure | iGUI soft | iGUI GL | Dear ImGui | FLTK | GTK 3 |
|---|---:|---:|---:|---:|---:|
| latency_frames_med | 2.0 | 2.0 | 3.0 | 1.0 | 1.0 |
| latency_ms_med | 48.7 | 113.6 | 63.5 | 0.3 | 21.5 |
| latency_ms_min | 39.9 | 45.9 | 32.0 | 0.3 | 10.4 |

### 9.8. Binary and library sizes (stripped)

| file | bytes | shared libraries linked | bytes of those beyond libc, libm, libstdc++ |
|---|---:|---:|---:|
| libigui_core.a | 337 568 |  |  |
| libigui_widgets.a | 5 682 562 |  |  |
| libigui_zen.a | 102 932 |  |  |
| libplatform.a | 1 020 146 |  |  |
| min_fltk | 845 304 | 24 | 3 912 752 |
| min_gtk | 14 472 | 67 | 29 225 944 |
| min_igui | 2 035 520 | 16 | 3 219 320 |
| min_imgui | 780 984 | 50 | 15 041 384 |

## 10. Depois da ronda 9

Os mesmos programas (`tools/bench_compare.sh`, as cenas A a F, soft e GL por software, o ImGui 1.92.5, o Xvfb privado), medidos na ponta do ramo `igui-round9` e em `origin/main` (0d86927, construido
com os mesmos `-O2` e medido com o mesmo `cmp_igui`). As instrucoes de utilizador (contadas passo a passo), as chamadas de desenho, os vertices e o heap em uso **nao dependem da carga da maquina**; os
tempos e o PSS em GL dependem (a maquina esta partilhada: o ImGui deu 46,1 e 50,2 MB na cena A, o mesmo binario). Nao se comparam os tempos de arranque entre as duas corridas (foram tomados com cargas
muito diferentes). A explicacao de cada mudanca e as imagens de antes e depois (`docs/img/round9/`) estao em `docs/ROUND9.md`.

### 10.1 Instrucoes, chamadas de desenho e vertices (iGUI por software; por GL a diferenca e uma centena de instrucoes)

| frame | cena | antes | depois | depois / antes | ImGui |
|---|---|---:|---:|---:|---:|
| parada | A | 6 990 | 3 860 | 0,55 | 19 618 |
| parada | B | 2 705 611 | 1 720 593 | 0,64 | 666 793 |
| parada | C | 238 396 | 209 330 | 0,88 | 252 858 |
| parada | D | 162 889 | 128 943 | 0,79 | 21 202 425 |
| parada | E | 1 003 526 | 813 745 | 0,81 | 4 733 558 |
| parada | F | 2 957 230 | 1 818 398 | 0,61 | 750 460 |
| rato (hover) | B | 2 731 016 | 1 746 861 | 0,64 | 674 783 |
| rato (hover) | F | 2 961 500 | 1 823 525 | 0,62 | 759 165 |
| roda | C | 243 922 | 215 156 | 0,88 | 267 211 |
| roda | D | 612 769 | 578 110 | 0,94 | 21 204 350 |
| roda | E | 1 008 517 | 818 688 | 0,81 | 4 756 627 |
| clique | B | 2 782 052 | 1 796 921 | 0,65 | 688 444 |
| escrever | E | 3 943 924 | 3 745 267 | 0,95 | 7 255 506 |

Nenhuma cena ficou mais lenta. Contra o ImGui **com o mesmo aspecto** (`cmp_imgui --rounded`, cantos de 4 px e borda de 1 px nos dois; frame parada): B 1 720 593 contra 1 567 272 (**1,10x**, era 1,73x),
F 1 818 398 contra 1 517 630 (**1,198x**, era 1,95x); o alvo era 1,2x. Contra o ImGui de serie (sem cantos) o iGUI ainda faz 2,6x em B e 2,4x em F: e a diferenca de aspecto de 4.1.

| cena | chamadas de desenho antes | depois | ImGui | vertices antes | depois | ImGui |
|---|---:|---:|---:|---:|---:|---:|
| A | 1 | 1 | 0 | 4 | 4 | 0 |
| B | 281 | **1** | 1 | 24 932 | 18 532 | 5 732 |
| C | 2 | 2 | 2 | 2 020 | 1 932 | 2 900 |
| D | 4 | 1 | 2 | 1 338 | 1 298 | 1 722 |
| E | 5 | 4 | 2 | 7 320 | 7 248 | 9 548 |
| F | 401 | **1** | 100 | 25 764 | 20 564 | 7 160 |

Tempo de CPU do lado do GUI de uma frame parada, microsegundos (mediana; depende da carga, leem-se as razoes): por software A 50 -> 23, B 729 -> 221, C 108 -> 51, D 88 -> 49, E 260 -> 124, F 760 -> 242;
GL B 756 -> 262, F 816 -> 319 (ImGui B 187, F 215).

### 10.2 Memoria de base (cena A, janela vazia) e por cena

| MB | A | B | C | D | E | F |
|---|---:|---:|---:|---:|---:|---:|
| PSS por software, antes | 42,5 | 44,8 | 45,1 | 44,1 | 81,7 | 45,0 |
| PSS por software, depois | **24,9** | 26,4 | 27,7 | 25,9 | 42,4 | 26,0 |
| heap por software, antes | 28,92 | 30,79 | 32,06 | 30,67 | 66,09 | 31,12 |
| heap por software, depois | **10,25** | 12,07 | 13,39 | 11,99 | 27,62 | 12,00 |
| PSS GL, antes | 55,1 | 71,8 | 57,6 | 56,1 | 94,8 | 78,2 |
| PSS GL, depois | 56,8 | 63,2 | 60,0 | 52,2 | 69,0 | 57,0 |
| heap GL, antes | 54,97 | 74,50 | 58,52 | 57,06 | 93,20 | 88,30 |
| heap GL, depois | **46,01** | 53,32 | 49,49 | 47,97 | 64,31 | 53,48 |
| PSS ImGui (corrida final) | 50,2 | 60,8 | 53,4 | 53,3 | 59,0 | 53,8 |
| heap ImGui | 41,94 | 44,04 | 43,58 | 43,39 | 49,54 | 44,32 |

O PSS em GL oscila de corrida para corrida (o do ImGui, o mesmo binario, 46,1 -> 50,2 em A e 49,7 -> 60,8 em B), por isso so se pode ler o heap (que nao varia). Por software o PSS desceu 41% em A (o rasteriador por
faixas, as paginas do atlas de um byte, a copia da ultima frame so quando ha frames iguais); a meta "abaixo do GTK (23 MB)" cumpre-se com o ciclo da biblioteca que salta frames (21,9 MB, medida a parte) e
**nao** na linha acima (24,9 MB, a pintar todas as frames). **Em GL ficou 4 MB de heap acima do ImGui** (46,0 contra 41,9): meta nao atingida. Por item (iGUI por software, heap; PSS entre parenteses): widget em B 9 819 -> 9 533 bytes (12 334 -> 7 690),
nos de D 181 -> 181 (168 -> 102), janela de F 46 197 -> 36 700 (52 470 -> 24 125).

### 10.3 Editor de texto (E, 91 000 linhas de 55 bytes)

| | antes | depois | ImGui | FLTK |
|---|---:|---:|---:|---:|
| bytes por linha, heap | 428 | **200** | 88 | 60 |
| bytes por linha, PSS | 453 | 202 | 101 | |
| instrucoes de uma frame parada | 1 003 526 | 813 745 | 4 733 558 | |
| instrucoes a escrever uma tecla | 3 943 924 | 3 745 267 | 7 255 506 | |

Alvo de menos de 100 bytes por linha: **nao atingido** (o que resta e `wrapRows_`, `lineWidths_`, `rowStarts_`; ver `docs/ROUND9.md` 2.4).

### 10.4 Parado e latencia (cena B, o ciclo da biblioteca, 10 s sem nada a acontecer; o Xvfb nao tem vblank)

| | antes | depois | ImGui |
|---|---:|---:|---:|
| por software, CPU parado | 70,2% | **0,8%** (0,61% numa corrida minha) | 412,9% |
| por software, acordares por segundo | 898 | 118 | 7 733 |
| GL, CPU parado | 172,9% | **0,7%** (0,63% numa corrida minha) | |
| GL, acordares por segundo | 1 210 | 119 | |
| latencia do clique, por GL (frames, ms) | 2, 73,4 | **1**, 26,2 (18,0 na corrida final) | 3, 18,7 |
| latencia do clique, por software (frames, ms) | 1, 45,2 | 1, 50,1 (37,9 na corrida final) | |

Os "antes" desta tabela sao de uma corrida de `cmp_igui` com o ciclo de `origin/main` (a corrida de `bench_compare.sh` sobre `origin/main` nao deixou as medidas); os "depois" e o ImGui, da corrida final de
`bench_compare.sh` (o ImGui, 187 frames por segundo, nunca espera). 118 acordares por segundo e o `pump()` a dormir 8 ms: falta a espera por eventos da zen_plataform (pedido em `docs/ROUND9.md` 2.3) para chegar a 1 a 2.
Uma aplicacao com funcao por frame (`setFrameCallback`) continua a pintar todas as frames ate ligar `setIdleSkipWithCallbacks(true)`.

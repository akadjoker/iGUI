# Review: modo imediato (`ig::Context`)

Feito a 2026-10-13 sobre o commit `9281675`. As linhas são as desse commit.

## O que foi lido

- **A fundo:** os quatro commits `692948d..9281675` (formato dos numéricos, a
  família `*Scalar`, os `drag*Range2`, os flags do `inputText`), o
  [CodeEditorImmediate.cpp](../src/CodeEditorImmediate.cpp) e o
  [FileDialog.cpp](../src/FileDialog.cpp) inteiros, e o código de popups e de tabs
  do [Gui.cpp](../src/Gui.cpp).
- **Por ler linha a linha:** o resto do `Gui.cpp` (cerca de 10 mil linhas: janelas,
  dock, tabelas, árvores, menus, plots, cores, drag & drop) e o `Gui.hpp`.

Por isso esta lista não diz que o resto está limpo; diz só que não foi visto.

## Como foram confirmados

Os problemas 1 a 14 foram reproduzidos com sondas headless sobre o
`tests/InteractionHarness.hpp` (um `Context` sem janela, com cliques e teclas
injetados), ou são avisos do compilador (o 7). O 15 é só de leitura. As sondas não
estão no repo; cada problema que se arranjar deve ganhar o seu teste.

## Estado corrompido ou memória

- [x] **1. Um popup dentro de outro estraga a janela de vez** —
      [Gui.cpp:3733](../src/Gui.cpp#L3733) (`endPopup`).
      O `endPopup` de dentro põe `popupContentIndex_ = -1`; o de fora vê `< 0` e sai
      antes do `endPopupContent()`. Ficam por fechar o `popupContentDepth_`, a pilha
      de ids e o `layout_`, e nada os repõe por frame.
      *Como se vê:* galeria, "Toggle.." → "Stacked Popup"
      (`GalleryIgui.cpp:1678-1696`). Um label a seguir ao bloco sai com o clip do
      popup de dentro; depois de fechar os dois, todos os widgets da janela ficam
      com o clip `(300,308 160x492)` e o conteúdo fica cortado para sempre.
      *Arranjo:* o `beginPopup` guarda o índice que lá estava e o `endPopup`
      repõe-no (uma pilha, como a dos popups).

- [x] **2. `sanitizeNumericFormat` sai sem terminar a string** —
      [Gui.cpp:215](../src/Gui.cpp#L215).
      Quando um `%` mais à frente não é uma conversão numérica, a função faz
      `return` sem escrever o `\0` em `out`, e o `formatNumericValue` passa esse
      buffer de stack ao `snprintf` como formato.
      *Como se vê:* `dragFloat(..., "%.0f%")` (o `%` do fim por dobrar) desenha
      "50" seguido de lixo; se o lixo tiver conversões, o `snprintf` lê argumentos
      que não existem. Só por leitura: um formato de inteiro com 60 caracteres de
      prefixo mais `%d` escreve o `ll`, o `d` e o terminador em `sanitized[64]`, um
      byte fora do buffer.
      *Arranjo:* terminar `out` em todas as saídas e contar o terminador nos
      limites do ciclo de cópia.

- [x] **3. `scalarFromDouble` converte sem limitar ao intervalo do tipo** —
      [Gui.cpp:6151](../src/Gui.cpp#L6151).
      O double de trabalho vai direto para o tipo de destino (comportamento
      indefinido fora do intervalo), e o `formatNumericValue`
      ([Gui.cpp:272](../src/Gui.cpp#L272)) limita a ±2^63 antes de um cast que o
      próprio 2^63 já rebenta.
      *Como se vê:* o "slider u64 full" da galeria arrastado até ao fim guarda 0;
      um u64 de 0,75·2^64 aparece como "9223372036854775808 ms"; um slider s64 no
      máximo guarda `INT64_MIN` e o `INT64_MAX` aparece como
      "-9223372036854775808"; `inputScalar(S8)` com "300" guarda 44. Um
      `dragScalar` sem limites rebenta qualquer tipo inteiro da mesma maneira.
      *Arranjo:* limitar ao mínimo e máximo do tipo antes do cast, e tratar o u64
      à parte (não cabe num double nem num `long long`).

## Editor de código

- [x] **4. Um clique na margem seleciona em vez de mover o cursor** —
      [CodeEditorImmediate.cpp:1531](../src/CodeEditorImmediate.cpp#L1531).
      Carregar nos números de linha ou num chevron de fold cai no ramo do arrasto:
      o cursor anda e a âncora fica onde estava.
      *Como se vê:* cursor em (1,17), clique na margem na linha 6: cursor (6,0),
      âncora (1,17), seleção ativa. Escrever uma letra substituiu a seleção e o
      texto passou de 9 linhas para 4.

- [x] **5. `replaceAll` em texto simples apanha ocorrências sobrepostas** —
      [CodeEditorImmediate.cpp:346](../src/CodeEditorImmediate.cpp#L346).
      A procura avança um byte (`pos = found + 1`), e cada substituição é aplicada
      sobre texto que a anterior já mudou.
      *Como se vê:* `'aaaa'` com `'aa'→'b'` dá `'b'` e devolve 3 (devia ser `'bb'`
      e 2); `'x----y'` com `'--'→'+'` dá `'x+y'` (devia ser `'x++y'`). O caminho de
      regex está bem.
      *Arranjo:* `pos = found + pattern.size()`.

## Numéricos

Quase tudo aqui veio com os quatro commits dos numéricos, e passa pelo mesmo
caminho (`sliderCore`, `dragCore`, `numericField`).

- [x] **6. Escrever num campo float perde dígitos** —
      [Gui.cpp:6832](../src/Gui.cpp#L6832) (`inputFloat`).
      O `numericField` guarda o double lido em `numericEditValue_`, quem chama
      estreita-o para float, e no frame seguinte a referência já não bate: o
      `editNumericText` troca o que se escreveu pelo valor reformatado. O `return`
      da linha 6832 salta o acerto da referência quando o float não mudou.
      *Como se vê:* escrever "0.105" guarda 0.15 (em "0.10" o float não muda, o
      texto volta a "0.1" e o "5" vai para o fim). `inputScalar(Float)` com "1.15"
      guarda 1.1005. O campo de valor do `sliderFloat` tem o mesmo caminho.

- [x] **7. Treze overloads de layout recebem `format` e não o passam** —
      [Gui.cpp:8673](../src/Gui.cpp#L8673).
      `sliderFloat`, `sliderInt`, `dragFloat`, `dragInt`, `inputInt`, `inputFloat`,
      `inputDouble`, `inputInt2/3/4`, `inputFloat2/3/4`. O GCC avisa
      (`-Wunused-parameter`) nas linhas 8669, 8684, 8716, 8731, 9008, 9024, 9043,
      9064, 9095, 9112, 9131, 9331 e 9346.
      *Como se vê:* `sliderFloat("sf", f, 0, 1, 300, 0, "%.1f grams")` desenha
      "0.5"; `inputInt("ii", 255, 300, "%04X")` desenha "255". O `dragFloatRange2`
      e o `dragIntRange2` passam por estes overloads, por isso o `format` e o
      `formatMaximum` deles também se perdem ("Min: %.1f" desenha "2").

- [x] **8. Sem formato do chamador, o drag escreve com `%.3g`** —
      [Gui.cpp:5228](../src/Gui.cpp#L5228) (`dragCore`; o mesmo em
      [Gui.cpp:4975](../src/Gui.cpp#L4975)).
      O valor desenhado era `%.3f`; agora é `%.3g`, inteiros incluídos.
      *Como se vê:* `dragFloat` a 1234.5 desenha "1.23e+03"; `dragInt` a 1500
      desenha "1.5e+03". Carregar num `dragInt` a 1234 abre o editor com
      "1.23e+03", e qualquer edição volta com cerca de 1230.

- [x] **9. O parse lê o prefixo do formato** —
      [Gui.cpp:319](../src/Gui.cpp#L319) (`parseNumericValue`).
      O número é lido do início do texto, mas o texto traz o prefixo literal do
      formato. Só o "0x" é tratado.
      *Como se vê:* `inputFloat` com `"0 < %.3f < 1"` mostra "0 < 0.500 < 1"; um
      Backspace põe o valor a 0 (lê o "0" do prefixo e aceita o resto como
      legenda). Só por leitura: com `"Value: %.1f"`, uma edição que mantenha o
      prefixo não se lê e é ignorada em silêncio.

- [x] **10. Sliders e drags de float devolvem `true` em todos os frames** —
      [Gui.cpp:5080](../src/Gui.cpp#L5080).
      Compara-se o valor guardado (arredondado a float) com um double por
      arredondar, a cada frame com o botão em baixo.
      *Como se vê:* `sliderFloat` 0..100 com o ponteiro parado na faixa deu `true`
      em 7 de 7 frames; `dragFloat`, em 6 de 6 depois de um movimento. Quem faz
      undo ou recalcula a cada mudança recebe um evento por frame, e o
      `item_.edited` fica ligado. O mesmo no `vSliderFloat`
      ([Gui.cpp:5136](../src/Gui.cpp#L5136)) e no `sliderAngle`. O código antigo
      em float devolvia `true` uma vez.

- [x] **11. Um intervalo invertido ou vazio faz o widget desaparecer** —
      [Gui.cpp:4964](../src/Gui.cpp#L4964) (`sliderCore`; o `dragCore` na 5216).
      Com `maximum <= minimum` a função sai antes de desenhar e de registar o
      item, e o layout avança na mesma.
      *Como se vê:* o `sliderScalar("slider s32 reverse", ...)` da galeria não
      desenha nada. `dragFloatRange2` com as duas pontas em 0 (intervalo 0..10)
      desenha só a caixa do "max"; com as duas em 10, só a do "min". Arrastar uma
      ponta até ao limite esconde a outra caixa.

## Texto, tabs e diálogo de ficheiros

- [x] **12. `EnterReturnsTrue` devolve `true` a cada tecla** —
      [Gui.cpp:6388](../src/Gui.cpp#L6388).
      O resultado do Enter é somado ao `changed`, em vez de o substituir. O header
      diz que é o `EnterReturnsTrue` do ImGui, que só devolve `true` no Enter.
      *Como se vê:* `if (ui.inputText("cmd", buf, w, EnterReturnsTrue)) run(buf);`
      corre a cada tecla; escrever "abc" sem Enter devolveu `true` três vezes.

- [x] **13. Ctrl+C num campo `Password` copia o texto real** —
      [Gui.cpp:6281](../src/Gui.cpp#L6281) (`editTextBuffer`).
      *Como se vê:* um campo com "hunter2" desenha "*******" e o Ctrl+C deixa
      "hunter2" na área de transferência. O ImGui desliga o copiar e o cortar
      nestes campos.

- [ ] **14. Uma barra de tabs dentro de outra tab não mostra tabs** —
      [Gui.cpp:2830](../src/Gui.cpp#L2830) (`beginTabItem`).
      A função recusa-se sempre que `tabContentDepth_ > 0`, apesar de as pilhas
      (`kTabBarStackSize = 4`) terem tamanho para aninhar.
      *Como se vê:* `beginTabBar("outer")` / `beginTabItem("Outer A")` /
      `beginTabBar("inner")` / `beginTabItem("Inner 1")`: a barra de dentro devolve
      `true` e desenha a faixa, mas o `beginTabItem` de dentro devolve `false` em
      todos os frames.

- [ ] **15. O duplo clique do diálogo de ficheiros conta frames** —
      [FileDialog.cpp:698](../src/FileDialog.cpp#L698) (e a linha 727, para o "..").
      Usa `frameNumber_ - lastClickedFrame <= 22u` em vez do intervalo em tempo que
      o `Context` já tem (`doubleClickInterval_`, `elapsedSeconds_`).
      *Não foi corrido.* Pelo código: a 1000 fps, 22 frames são 22 ms e nenhum
      duplo clique regista; a 30 fps são 0,73 s e dois cliques separados abrem a
      pasta.

## Ficou de fora da lista

Vistos de passagem e não corridos (a não ser onde se diz):

- [ ] `elapsedSeconds_` é um acumulador em float
      ([Gui.cpp:1191](../src/Gui.cpp#L1191)): em sessões longas a muitos fps deixa
      de avançar, e a partir daí qualquer segundo clique perto conta como duplo.
- [ ] Multi-cursor: o Ctrl+X e o Ctrl+V só atuam no cursor principal e deixam os
      outros desatualizados
      ([CodeEditorImmediate.cpp:1326-1342](../src/CodeEditorImmediate.cpp#L1326-L1342)).
- [ ] A lista de completion desenha 8 itens e a seleção percorre até 50
      ([CodeEditorImmediate.cpp:2000](../src/CodeEditorImmediate.cpp#L2000)).
- [ ] O `inputScalar` ignora o parâmetro `flags`
      ([Gui.cpp:9273](../src/Gui.cpp#L9273), também um aviso do compilador).
- [ ] O `shortcut()` não olha para `modalPopupWasOpenAtFrameStart_` e o
      `isKeyPressed()` olha ([Gui.cpp:2051](../src/Gui.cpp#L2051)).
- [ ] A guarda de overflow do `parseNumericValue` só está certa em base 10
      ([Gui.cpp:408](../src/Gui.cpp#L408)).
- [ ] Um valor negativo com formato sem sinal aparece como zero:
      `inputInt(-1, "%08X")` desenha "00000000" (visto numa sonda).
- [ ] Código morto depois dos numéricos em double: `parseIntText`,
      `parseFloatText`, os três helpers de slider em float e o
      `scalarTypeIsUnsigned`. Os helpers em
      [Gui.cpp:6112-6247](../src/Gui.cpp#L6112-L6247) estão fora do namespace
      anónimo, com ligação externa.

## Por onde começar

1. Os três primeiros: deixam estado ou memória estragados.
2. Os numéricos 6 a 11 juntos, com testes em `tests/test_numeric_input.cpp`.
3. O 4 e o 5 no editor, que perdem texto do utilizador.
4. O resto, um a um.

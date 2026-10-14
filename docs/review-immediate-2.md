# Review 2: modo imediato (`ig::Context`)

Feito a 2026-10-14 sobre o commit `0579888`. As linhas são as desse commit (o
`Gui.cpp` não mudou desde então). Cobre o que o primeiro review
([review-immediate.md](review-immediate.md)) deixou por ler.

## Como foram confirmados

Cada ponto das listas foi reproduzido com uma sonda headless sobre o
`tests/InteractionHarness.hpp`, com ASan quando o problema é de memória. As
sondas não estão no repo; cada problema que se arranjar deve ganhar o seu teste.
Os pontos de "Não confirmados" foram vistos só no código.

Nenhum destes foi arranjado ainda.

## Menus, combos, listas e árvores

Lido: 2368-2757, 3288-3669, 4097-4550, 4551-5087.

- [x] **N1. Um submenu aberto dentro de um popup bloqueia a interface toda** —
      [Gui.cpp:4910](../src/Gui.cpp#L4910), 1372, 1470. Depois de passar pelo
      submenu e fechar o popup com um clique fora, nenhum clique chega a nada até
      um Escape: o `openSubMenu_` nunca é limpo e o `menuWasOpenAtFrameStart_` fica
      ligado. *Arranjo:* limpar o `openSubMenu_` e as bounds dele no
      `releaseUnsubmittedBlockers` quando o submenu deixou de ser submetido.
- [x] **N2. As bounds de um submenu fechado continuam a comer cliques e o menu não
      fecha** — [Gui.cpp:4651](../src/Gui.cpp#L4651), 4673, 4841, 5056, 13615. O
      `subMenuPopupBounds_` só é zerado na troca por hover.
- [x] **N3. O submenu não fecha quando o rato passa para outro item do menu pai** —
      [Gui.cpp:4743](../src/Gui.cpp#L4743), 4908.
- [x] **N4. A barra de menu principal corta o texto e só responde numa faixa de
      10 px** — [Gui.cpp:4574](../src/Gui.cpp#L4574), 4614, 4660, 4671. O clip
      desconta a margem da janela. *Arranjo:* `useClientArea = true` na janela da
      barra.
- [x] **N5. A barra de rolagem do `beginCombo` não se deixa arrastar** —
      [Gui.cpp:4368](../src/Gui.cpp#L4368). O teste do polegar corre antes de o
      `comboItemsSubmitted_` ficar ligado, e o ponteiro é bloqueado.
- [x] **N6. Com conteúdo mais largo que o cabeçalho e com rolagem, o `beginCombo`
      corta o conteúdo** — [Gui.cpp:4292](../src/Gui.cpp#L4292), 4428 contra 4493.
      O begin calcula a largura sem a barra e o end com ela.
- [x] **N7. As linhas do `listBox` (e do `comboBox`) são paragens do Tab, e as setas
      deixam de funcionar** — [Gui.cpp:3416](../src/Gui.cpp#L3416), 3524.
- [x] **N8. A lista do `comboBox` sai do ecrã sem maneira de lá chegar** —
      [Gui.cpp:3492](../src/Gui.cpp#L3492). Sem limite de altura nem roda.
- [x] **N9. Os popups do `beginMenu` e do menu de contexto não ficam dentro do
      ecrã** — [Gui.cpp:4668](../src/Gui.cpp#L4668), 4677, 5050. O
      `beginSubMenu` já o faz.
- [x] **N10. O Ctrl+A de um bloco de escolha múltipla dispara enquanto se escreve
      num campo de texto** — [Gui.cpp:3573](../src/Gui.cpp#L3573).
- [x] **N11. O `checkboxFlags` com uma máscara de vários bits faz o contrário do
      ImGui** — [Gui.cpp:4183](../src/Gui.cpp#L4183). *Arranjo:*
      `(flags & flagValue) == flagValue`.
- [x] **N12. Depois do `listBox` e do `comboBox` aberto, as consultas do item falam
      da última linha** — [Gui.cpp:3415](../src/Gui.cpp#L3415), 3444, 3523. Um
      tooltip com `isItemHovered()` depois do widget nunca aparece.
- [x] **N13. Um clique direito fora não fecha o menu de contexto** —
      [Gui.cpp:5056](../src/Gui.cpp#L5056) (e o `beginMenu`, 4651).

Não confirmados: o `listBox` e o `comboBox` põem um `currentItem` fora do
intervalo a 0 sem devolver true; depois de escolher uma linha do `comboBox` o
foco fica na linha e as setas deixam de mexer no combo (confirmado e corrigido
com o N7); um `menuItem` desativado
não atualiza o item; o drop do `treeItem` aceita através de uma janela por cima.

## Janelas, frame, child e layout

Lido: 1254-2160, 9582-9728, 11850-12090, 12402-12960, 13043-13092, 13335-13769.

- [x] **W1. Fechar um popup modal com Escape bloqueia o teclado para sempre** —
      [Gui.cpp:1485](../src/Gui.cpp#L1485). O `modalPopupWasOpenAtFrameStart_` só é
      recalculado com `popupDepth_ > 0`; depois do Escape fica a true. O
      `isKeyPressed`, o `shortcut` e o Ctrl+Z/Y deixam de responder.
      *Arranjo:* pô-lo a false antes do `if`.
- [x] **W2. Um popup que passa por cima de outra janela não recebe cliques e traz
      essa janela para a frente** — [Gui.cpp:1444](../src/Gui.cpp#L1444),
      13552, 13671. O foco muda no press antes de se saber que o press é do popup;
      o `pointerInsideOwnPopup` só conhece menus e combos. A lista de um combo que
      cai sobre outra janela funciona, mas tapa a janela onde se estava.
- [x] **W3. O Tab num popup modal passa o foco para um campo de trás** —
      [Gui.cpp:13043](../src/Gui.cpp#L13043). O `inputBlockedByModal` não conhece o
      `beginPopupModal`; o texto escrito a seguir vai para o campo de trás.
- [x] **W4. O Tab salta para widgets de outra janela** —
      [Gui.cpp:13053](../src/Gui.cpp#L13053). O `focusOrder_` junta todas as
      janelas; o texto pode ir para uma janela escondida.
- [x] **W5. Um widget ativo que deixa de ser submetido bloqueia a interface para
      sempre** — [Gui.cpp:13421](../src/Gui.cpp#L13421) (arrasto e resize de
      janela), 9630 (scrollbar do child), e o mesmo com um slider. `wantsPointer()`
      fica a 1 e nenhum clique chega a outro widget.
      *Arranjo:* no `beginFrame`, largar o ativo que não foi visto no frame anterior.
- [x] **W6. O scrim do `beginDialog` nunca se desenha** —
      [Gui.cpp:1811](../src/Gui.cpp#L1811). Vai para o `frameDrawList_`, cujos
      comandos o `endFrame` limpa (1526) antes de juntar as listas. O scrim do
      `beginPopupModal` tinha o mesmo defeito (corrigido no mesmo commit).
- [x] **W7. O Escape que fecha um popup dentro do `beginDialog` fecha também o
      diálogo** — [Gui.cpp:1793](../src/Gui.cpp#L1793).
- [x] **W8. Uma janela sem `bool *open` mostra o X, e o clique fecha-a sem volta** —
      [Gui.cpp:13383](../src/Gui.cpp#L13383) e 1739. Nem `restoreWindow` nem
      `raiseWindow` a trazem de volta.
- [x] **W9. Uma janela maximizada fica com a barra de título debaixo da barra de
      menu** — [Gui.cpp:1892](../src/Gui.cpp#L1892), 13337, 1973 (tile). O X e o
      restaurar deixam de se poder clicar.
- [x] **W10. Um `beginChild` fora de vista não avança o layout** —
      [Gui.cpp:9593](../src/Gui.cpp#L9593). O conteúdo de baixo salta.
- [x] **W11. O `endChild` mede a altura pelo último item, não pela linha** —
      [Gui.cpp:9690](../src/Gui.cpp#L9690). Com `sameLine` no fim, parte do
      conteúdo fica fora do scroll.
- [x] **W12. `availableWidth` num child dentro de um dock panel devolve a largura do
      painel** — [Gui.cpp:12077](../src/Gui.cpp#L12077). O dock é aplicado depois
      do child; o `availableHeight` faz a ordem certa.
- [x] **W13. O `contentExtent` recomeça a cada `indent`/`setCursor`/`beginGroup`** —
      [Gui.cpp:12440](../src/Gui.cpp#L12440). As caixas medidas encolhem: um
      tooltip rico passa de 280 px a 24 px; afeta também o `measurePopup`, os
      menus e o `measureTableCellWidth`.
- [x] **W14. Um grupo que contém um child cresce com o conteúdo escondido do child** —
      [Gui.cpp:12453](../src/Gui.cpp#L12453).
- [x] **W15. `newLine` depois de `sameLine` ignora a altura da linha** —
      [Gui.cpp:11865](../src/Gui.cpp#L11865). O item seguinte sai por cima.
- [x] **W16. O `separator` usa a borda direita da janela** —
      [Gui.cpp:11997](../src/Gui.cpp#L11997) (e o `separatorText`). Num popup à
      direita da janela tem largura 0; devia usar o `availableWidth()`.
- [ ] **W17. O `setNextItemWidth` antes do `beginChild` passa para o primeiro item de
      dentro** — [Gui.cpp:9588](../src/Gui.cpp#L9588).

Não confirmados: o `removeWindow` não invalida a janela do ponteiro (dura um
frame); o `availableHeight` não conhece popup, tab nem tabela; o `wheelAreas_`
nunca perde entradas.

## Dock

Lido: 9729-10333, 12535-12773, 13738-13762, e o `WindowStateIni.hpp`.

- [x] **D1. O `bool *open` de um painel é lido e escrito fora da chamada que o
      recebeu** — [Gui.cpp:9978](../src/Gui.cpp#L9978), 10050, 10136. Um `open`
      apagado dá heap-use-after-free no ASan; com o padrão do ImGui (um `bool`
      local por painel) dá stack-use-after-return, e sem ASan o X da tab "Beta"
      fecha "Alpha".
      *Arranjo:* marcar `closeRequested` na tab e escrever o `*open` só dentro do
      `beginDockPanel` desse painel.
- [ ] **D2. Um painel arrastado para outra região volta à antiga quando é fechado, e
      é a antiga que se grava** — [Gui.cpp:9972](../src/Gui.cpp#L9972), 12693, 12753.
- [ ] **D3. Encolher o dock space um frame estraga para sempre os tamanhos das
      regiões** — [Gui.cpp:9882](../src/Gui.cpp#L9882). O clamp escreve por cima de
      `leftWidth`/`rightWidth`/`bottomHeight`, e é isso que se grava.
- [ ] **D4. Dock spaces com o mesmo id em janelas diferentes partilham a secção do
      `.ini`** — [Gui.cpp:9861](../src/Gui.cpp#L9861), 12625.
- [ ] **D5. Dois painéis desenham na mesma região no mesmo frame** —
      [Gui.cpp:10046](../src/Gui.cpp#L10046), 10176. Acontece no primeiro frame
      depois de ler o `.ini` e no frame em que se larga uma tab.
- [ ] **D6. O título da tab ignora `##`/`###` e não se atualiza** —
      [Gui.cpp:10039](../src/Gui.cpp#L10039), 10118, 10201, 10271.
- [ ] **D7. `setDockSizes` no primeiro frame apaga os tamanhos lidos do `.ini`, e
      nesse frame não há clamp** — [Gui.cpp:10003](../src/Gui.cpp#L10003). Um
      tamanho grande esconde o painel Center nesse frame.
- [ ] **D8 (menor). A barra de tabs de um slot é redesenhada por cada painel do
      slot** — [Gui.cpp:10106](../src/Gui.cpp#L10106). A geometria sai duplicada e
      um hover translúcido mistura-se duas vezes.

Não confirmados: um `DockSlot` fora do enum indexa fora do array; o popup da
lista de tabs não é um popup registado e pode deixar o clique cair no splitter.

## Tabelas e listas virtuais

Lido: 10334-11542.

- [x] **T1. Com `scrollY` e `freezeRows=0` (o valor por omissão) a linha 0 não
      rola** — [Gui.cpp:10538](../src/Gui.cpp#L10538), 11025. As outras linhas
      passam por cima dela, e o clique vai para a linha 0.
- [x] **T2. O `offsetY` não é limitado quando o conteúdo encolhe** —
      [Gui.cpp:10734](../src/Gui.cpp#L10734). As linhas ficam fora de vista e a
      roda não as traz de volta.
- [x] **T3. Com `scrollX` e `scrollY` a barra horizontal não se vê nem se arrasta** —
      [Gui.cpp:10813](../src/Gui.cpp#L10813), 10826, 10873. O clip dela fica com
      altura 0.
- [x] **T4. Com X e Y o fim da última linha fica tapado pela barra de baixo** —
      [Gui.cpp:10734](../src/Gui.cpp#L10734).
- [x] **T5. Uma tabela com X e Y ocupa mais 7,8 px do que a altura pedida** —
      [Gui.cpp:11461](../src/Gui.cpp#L11461). A barra é somada duas vezes.
- [x] **T6. Redimensionar colunas com `scrollX` usa a escala errada** —
      [Gui.cpp:11185](../src/Gui.cpp#L11185). Divide por `bounds.width` em vez de
      `contentWidth`; um arrasto de 30 px soma 119 e 46 px às duas colunas.
- [x] **T7. Perder um arrasto de cabeçalho deixa a interface morta** —
      [Gui.cpp:11205](../src/Gui.cpp#L11205), 10913. O `reorderWidget` é reescrito
      por cada cabeçalho, e a limpeza compara com o id errado.
- [x] **T8. `beginTable(id, int columns, style)` ignora `scrollY`, `height`,
      `reorderable`, `scrollX` e `sizingFixedFit`** —
      [Gui.cpp:10633](../src/Gui.cpp#L10633). Não chama o `configureTable`.
- [x] **T9. O `offsetX` não é limitado quando a `innerWidth` diminui** —
      [Gui.cpp:10862](../src/Gui.cpp#L10862).
- [x] **T10. Com `sizingFixedFit` as colunas crescem 16 px por frame, sem parar** —
      [Gui.cpp:11066](../src/Gui.cpp#L11066). Um item que ocupa a coluna toda
      (o próprio cabeçalho, uma progressBar) mede a coluna mais as margens.
- [x] **T11. Uma tabela `scrollY` cujas linhas cabem engole a roda dentro de um child
      que rola** — [Gui.cpp:10789](../src/Gui.cpp#L10789).
- [x] **T12. Dois `tableTreeCell` na mesma linha partilham o id; o segundo nunca
      abre** — [Gui.cpp:11371](../src/Gui.cpp#L11371).
- [x] **T13. Com `scrollX` e `freezeCols > 0` a faixa alternada das linhas fica
      cortada à coluna presa** — [Gui.cpp:11093](../src/Gui.cpp#L11093).
- [x] **T14 (menor). Uma lista virtual que encolhe passa um frame vazia** —
      [Gui.cpp:10350](../src/Gui.cpp#L10350).

Não confirmados: redimensionar com `sizingFixedFit` parece não ter efeito (fica
para depois do T10); ao largar um reorder, os cabeçalhos seguintes usam a ordem
nova no mesmo frame; uma tabela `scrollY` sem linhas fica com altura 0.

## Widgets diversos

Lido: 5532-6375, 6990-8518, 11593-11849, 12337-12401.

- [ ] **M1. Vários widgets editam o valor dentro de `beginDisabled()`** — splitter
      [Gui.cpp:5548](../src/Gui.cpp#L5548), colorEdit 7097/7120/7139, gizmo2D 5598,
      gizmo3D 5794, gradientEditor 7289, curveEditor 7382, sequencer 7547,
      timeline 7609, updateTimeView 7446, beginDragSource 8290. O clique não olha
      para o `disabledDepth_`; o `sliderValue` e o `itemClicked` olham.
- [ ] **M2. Arrastar uma paragem do gradiente ou um ponto da curva para lá do vizinho
      arrasta também o vizinho** — [Gui.cpp:7254](../src/Gui.cpp#L7254), 7312,
      7368, 7400. O sort de cada frame muda os índices e o selecionado fica com o
      antigo.
- [x] **M3. Overflow de `int` no índice das colunas dos plots lê fora do buffer** —
      [Gui.cpp:7965](../src/Gui.cpp#L7965), 7993, 8012. Com 2 000 000 amostras
      e 1100 px, SEGV.
- [ ] **M4. `showToast` com o mesmo id duplica em vez de atualizar** —
      [Gui.cpp:8266](../src/Gui.cpp#L8266). O id é guardado e nunca usado.
- [ ] **M5. `wrapText` acrescenta uma linha vazia quando o corte cai em espaços antes
      de um `\n`** — [Gui.cpp:11733](../src/Gui.cpp#L11733). Afeta o
      `textWrapped` e o `bulletText`.
- [ ] **M6. `beginDragSource` rouba o `activeWidget_` ao item a que pertence** —
      [Gui.cpp:8290](../src/Gui.cpp#L8290). Depois de um `button` o arrasto nunca
      começa; antes de um `selectable`, o selectable nunca devolve o clique.
- [ ] **M7. `acceptDragDropTarget` aceita a largada através de uma janela por cima** —
      [Gui.cpp:8327](../src/Gui.cpp#L8327).
- [ ] **M8. O Enter que aceita um `messageBox` também clica o botão com foco** —
      [Gui.cpp:8129](../src/Gui.cpp#L8129). O `enterPressed_` não é consumido; o
      Escape (8126) tem o mesmo caminho.
- [ ] **M9. O snap de escala dos gizmos leva a escala a 0** —
      [Gui.cpp:5672](../src/Gui.cpp#L5672), 5888. Limita-se a 0.01 antes do snap.
- [ ] **M10. No gizmo3D, o plano XZ move em diagonal e ignora o movimento vertical** —
      [Gui.cpp:5830](../src/Gui.cpp#L5830).
- [ ] **M11. A rotação dos gizmos salta 360° a meio do arrasto** —
      [Gui.cpp:5646](../src/Gui.cpp#L5646), 5860. O ângulo não é desenrolado.
- [ ] **M12 (menor). `colorEdit` devolve true sem mudar a cor** —
      [Gui.cpp:7132](../src/Gui.cpp#L7132), 7158.

Não confirmados: um ponto atrás da câmara é projetado espelhado no
`projectGizmo3D`; o `return changed` do gizmo3D sem centro salta o
`item_.edited`; o splitter salta até meia espessura no press; o `stepperInt` não
limita um valor que já está fora do intervalo; um `+inf` num plot achata a série.

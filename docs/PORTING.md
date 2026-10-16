# Origem do que foi portado

O que o iGUI copia de outro projeto fica aqui: o ficheiro e as linhas de origem,
o que entrou e onde difere. Os números de linha do Godot referem-se ao commit
`e7b12e7492` (4.8-dev) de https://github.com/godotengine/godot.

## Range (include/igui/widgets/Range.hpp, src/widgets/Range.cpp, third_party/r128)

- Origem: `scene/gui/range.h` 38-46 (os valores e a sua omissão: valor 0, mínimo
  0, máximo 100, passo 1, página 0) e `scene/gui/range.cpp`: `_snapped_r128`
  39-74, `_value_changed_notify` 90-95, `redraw_owners` 159-170, `set_value`
  172-180, `_set_value_no_signal` 182-184, `_calc_value` 186-213,
  `set_value_no_signal` 215-222, `set_min` 224-239, `set_max` 241-254,
  `set_step` 256-265, `set_page` 267-279, `set_as_ratio` 301-319,
  `get_as_ratio` 321-338, `set_use_rounded_values` 438-440, `set_exp_ratio`
  446-455, `set_allow_greater` 461-463, `set_allow_lesser` 469-471. O texto do
  número é o `String::num` de `core/string/ustring.cpp` 1410-1504 (com o
  `MAX_DECIMALS` 32 da linha 62); `Math::step_decimals` e
  `Math::range_step_decimals` de `core/math/math_funcs.cpp` 61-94,
  `Math::snapped` 121-126, e o `Math::is_equal_approx(double, double)` de
  `core/math/math_funcs.h` 552-563 com o `CMP_EPSILON` 0.00001 de
  `core/math/math_defs.h` 50.
- `third_party/r128/r128.h`: cópia sem alterações do `thirdparty/misc/r128.h`
  do Godot (r128 1.6.0, domínio público, aviso no próprio ficheiro), compilada à
  parte em `igui_widgets_r128` (`r128_impl.cpp`).
- O que entrou: `Range : public Widget` com `setValue`, `setValueNoSignal`,
  `setMin`, `setMax`, `setStep`, `setPage`, `setAsRatio`, `asRatio`,
  `setUseRoundedValues`, `setExpRatio`, `setAllowGreater`, `setAllowLesser`; os
  sinais `valueChanged` (o `value_changed`) e `changed`; o virtual
  `onValueChanged` (o `_value_changed`) e `notifyValueChanged` (o
  `_notify_shared_value_changed`); os estáticos `numberText` (o `String::num`) e
  `rangeStepDecimals`.
- Diferenças: (1) sem a partilha de valores entre dois `Range` (`share`,
  `unshare` e o `Shared` com vários donos), que só as barras de scroll do
  original usam: cada `Range` guarda os seus valores; (2) os sinais saem sempre,
  e não só para os donos que estão na árvore (`is_inside_tree`); (3) sem
  acessibilidade nem avisos de configuração; (4) o `snprintf` usa `%.*lf` com as
  casas como argumento, no lugar do formato montado à mão, com o mesmo
  resultado; (5) o `MIN(18, ...)` e os `MAX` seguem a semântica das macros do
  original (`a < b ? a : b`), não a do `std::fmin`/`std::fmax`, que difere com
  NaN.

## SpinSlider (include/igui/widgets/SpinSlider.hpp, src/widgets/SpinSlider.cpp)

Port do `EditorSpinSlider` (`editor/gui/editor_spin_slider.h` e `.cpp`), em
chunks. O que já entrou:

- Chunk 2, o desenho. Origem: `get_minimum_size` 63-72, `get_text_value` 74-76,
  a parte de desenho de `_draw_spin_slider` 339-450 (a caixa normal ou só de
  leitura, o fundo do rótulo quando plano, o foco, o rótulo a meio alfa, o
  número e o sufixo glifo a glifo, só os que cabem inteiros na área do número,
  o sufixo a 0.4 do alfa, e a barra: fundo a 0.2, cheio a 0.45, botão de 4 px a
  0.9), `set_control_state` 543-546, `set_editing_integer` 562-572,
  `set_label` 578-585, `set_suffix` 591-594, `set_read_only` 687-694,
  `set_flat` 700-703 e o foco do construtor 827-828. O tema é o do editor
  (`editor/themes/theme_classic.cpp` 1797-1815: `label_color` é a cor das
  letras, `read_only_label_color` a das letras só de leitura, `label_bg` o
  `dark_color_3` sem borda; 2187-2191: `inspector_property_height` é a altura
  mínima de um botão mais a da fonte) e o ícone `editor/icons/GuiSpinboxUpdown.svg`
  tem 16 px de largura.
- Diferenças: (1) as caixas do tema (`normal`, `read_only`, `focus`) são as do
  `TextInput` do iGUI: fundo `inputBg` (só de leitura `panelColor`) com a borda
  `inputBorder`, e a borda `focusColor` com foco; as margens do conteúdo são o
  `padding` do tema; o `label_bg` é o `bgColor`; a altura mínima de um botão é o
  `buttonHeight`; (2) as cores com alfa em `float` passam a 8 bits com
  arredondamento ao mais próximo (0.2 dá 51, 0.45 dá 115, 0.9 dá 230, 0.5 dá
  128 e o sufixo 0.4 do alfa); (3) sem esquerda-direita (RTL) nem números
  localizados (`format_number`); (4) o número desenha-se como o maior início do
  texto (número, espaço fino, sufixo) cujos glifos cabem, que é o que o ciclo
  por glifo do original desenha da esquerda para a direita; (5) o `EDSCALE` é 1.

- Chunk 3, o arrasto do valor. Origem: `gui_input` 78-176 (o botão esquerdo
  começa a tentativa de arrasto e o release acaba-a, emitindo o valor no modo
  diferido; o botão direito com o arrasto em curso cancela; no movimento, o
  Shift multiplica por 0.1 só com o arrasto começado, a distância soma o
  movimento vezes a velocidade, o arrasto começa acima de 4 vezes a velocidade
  e captura o rato, o valor de partida fica dentro do intervalo, o passo é no
  mínimo o `default_float_step`, o Ctrl arredonda fora da edição de inteiros, e
  o valor entra com ou sem sinal; nas teclas, com o arrasto em curso o
  `ui_cancel` (Escape sem modificadores) cancela e todas as teclas ficam no
  campo), `_grab_start` 178-185, a parte do arrasto de `_grab_end` 187-201, o
  `EXIT_TREE` 509-516, a velocidade de `set_editing_integer` 569,
  `is_grabbing` 709-711, `set_deferred_drag_mode_enabled` 713-715 e os sinais
  `grabbed` e `ungrabbed` 777-778. As definições do editor vêm de
  `editor/settings/editor_settings.cpp` 594-595 (`float_drag_speed` 5.0,
  `integer_drag_speed` 0.5) e 601 (`default_float_step` 0.001).
- Por vir: o clique sem arrasto e o `ui_accept` abrem a edição do texto
  (`_focus_entered`, chunk 5); as setas (chunk 6).
- Diferenças do chunk 3: (1) as definições do editor ficam fixas nos valores
  por omissão; (2) um campo desativado (`isEnabledInTree`) não arrasta, como o
  só de leitura; (3) os eventos do rato ficam no campo, como o
  `MOUSE_FILTER_STOP`; (4) o arrasto não acaba ao perder o foco da janela nem
  ao pedido de fecho (`WM_WINDOW_FOCUS_OUT`, `WM_CLOSE_REQUEST`), que a IO não
  reporta; a troca de stage acaba-o pelo release que a `WidgetApp` entrega; (5)
  o `EXIT_TREE` é o destrutor; (6) o `ungrabbed` sai com o estado do arrasto já
  limpo, e o campo não lê nada depois de um sinal em que um widget foi apagado;
  (7) o Ctrl é só o Ctrl (sem o Cmd do macOS); (8) até ao chunk 5, o Enter com
  o arrasto em curso fica no campo como as outras teclas.

- Chunk 4, o grabber. Origem: `_grabber_gui_input` 210-272 (com o grabber
  premido, a roda soma ou tira um passo e passa ao modo roda; o botão esquerdo
  agarra, guarda o valor, a proporção e o ponto de partida fora do modo roda,
  dá o foco ao campo e emite `grabbed`, e ao largar emite `ungrabbed`; o
  direito com o grabber premido repõe o valor; o movimento põe a proporção
  mais o deslocamento sobre o comprimento da barra, e no modo roda não conta),
  a roda sobre o campo 113-117, o `mouse_over_grabber` no fim do arrasto 192,
  a parte do grabber de `_grab_end` 203-207, o grabber escondido na edição de
  inteiros 426-428, a parte do grabber de `_draw_spin_slider` 450-477 (o rato
  volta ao centro do botão da barra no fim do arrasto, o grabber aparece sem
  arrasto do campo com o rato sobre o campo ou sobre ele ou agarrado,
  realçado com o rato sobre ele ou agarrado, centrado no botão da barra, e no
  modo roda o rato vai para o canto do grabber mais o tamanho do botão), o
  `NOTIFICATION_MOUSE_ENTER`/`EXIT` 519-527, `_grabber_mouse_entered` e
  `_grabber_mouse_exited` 677-685 e o grabber do construtor 829-836. O ícone é
  o `HSlider/grabber` e `grabber_highlight` (`BIND_THEME_ITEM_EXT` 802-803;
  `editor/themes/theme_classic.cpp` 1204-1205):
  `editor/icons/GuiSliderGrabber.svg` e `GuiSliderGrabberHl.svg`, 16 x 16, um
  disco branco de raio 3 a 0.9 e um anel de raio 6 e 2 de traço em `#e0e0e0`
  a 0.29 (realçado a 1).
- Diferenças do chunk 4: (1) o ícone desenha-se com círculos, com as cores do
  SVG do tema escuro (sem a troca de cores dos temas claros); (2) o iGUI pinta
  tudo em cada frame: o que o desenho do original calcula (a posição do
  grabber, o comprimento da barra, o ponto para onde o rato volta) é calculado
  em cada pintura, e o warp do modo roda, que muda o rato, só corre numa
  pintura com o campo marcado por `markDirty`, que faz de `queue_redraw`; (3) o
  grabber só recebe o rato dentro do retângulo do campo, porque o hit-test do
  iGUI não chega a filhos fora do pai; (4) a roda conta um passo por evento,
  com o sinal do `scrollY`; (5) o foco dado pelo grabber mostra o realce do
  foco (sem o `grab_focus(true)` que o esconde); (6) a escala da tela é 1.

## WidgetApp: botões com outro já premido

- Origem: `scene/main/viewport.cpp` 1951-1972 (um botão premido com outros em
  baixo vai para o `mouse_focus`, sem novo hit-test; sem `mouse_focus` perde-se;
  com a máscara vazia, o controlo atingido passa a `mouse_focus` e entra na
  máscara) e 2037-2055 (o release tira o botão da máscara e vai sempre para o
  `mouse_focus`, que só é largado quando a máscara fica vazia).
- O que entrou: `pressedButtons_` na `WidgetApp` faz de `mouse_focus_mask` e o
  `pressed_` de `mouse_focus`. `dropMouseFocus` é o `_drop_mouse_focus`
  (2785-2806): ao trocar de stage, que esconde o widget como o
  `_gui_hide_control` (2574-2577), o widget que tinha os botões recebe o
  release de cada botão ainda em baixo, e a máscara fica vazia. Um widget
  apagado limpa o `pressed_` e a máscara, como o `_gui_remove_control`
  (2590-2594).
- Diferenças: (1) o botão encaminhado não mexe no foco do teclado (o original
  dá o foco ao `mouse_focus` com o botão esquerdo quando o ponteiro está sobre
  ele); (2) o arrasto e largada do iGUI fica como estava (o original larga com o
  botão direito durante um arrasto); (3) a contagem de cliques duplos corre
  também para o botão encaminhado; (4) os releases do `dropMouseFocus` sobem
  pelos pais como os outros eventos do rato do iGUI, e o original só os dá ao
  controlo.

## IO: modo do rato e movimento relativo

O que o `EditorSpinSlider` usa do `Input` do original, sem código copiado:
`MouseEvent::relX`/`relY` fazem de `InputEventMouseMotion::get_relative`, e os
ganchos `IO::setMouseCaptured` e `IO::warpMouse` (com `WidgetApp::setMouseCaptured`
e `WidgetApp::warpMouse`) fazem de `Input::set_mouse_mode(MOUSE_MODE_CAPTURED /
MOUSE_MODE_VISIBLE)` e `Input::warp_mouse`.

- Diferenças: (1) o movimento relativo é a diferença entre duas posições de
  `IO::mouseX`/`mouseY`; com o rato capturado, o backend continua a somar o
  movimento a essas posições para lá da borda da janela; (2) depois de um
  `warpMouse` o movimento conta a partir do ponto para onde o ponteiro foi, e o
  salto não conta como movimento.

## WidgetApp: chamadas adiadas

O que o `EditorSpinSlider` usa do `call_deferred` do original
(`callable_mp(...).call_deferred()`, posto na fila de mensagens e corrido no fim
do frame), sem código copiado: `WidgetApp::callDeferred(owner, fn)` corre `fn`
no fim do `update()`, depois das teclas, do texto e das largadas do frame, pela
ordem das chamadas, e uma chamada adiada durante essa passagem corre nela.

- Diferenças: (1) a chamada é descartada quando o widget dono é apagado antes
  de correr (o original descarta a de um objeto já libertado); (2) corre no fim
  da entrada e não no fim do frame inteiro.

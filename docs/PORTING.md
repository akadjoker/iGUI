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

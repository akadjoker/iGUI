# Ronda 9: o tema (cores fixas escuras) e as perdas medidas contra o Dear ImGui, o FLTK e o GTK

Duas partes. A primeira (seccao 1) corrige o que uma equipa de uma aplicacao encontrou: o tema claro ficava partido por cores escuras escritas no codigo dos
widgets. A segunda (seccao 2 em diante) ataca as perdas que `docs/COMPARACAO.md` seccao 4 mediu. Tudo o que esta aqui foi corrido; o que nao se fez esta na seccao final.

## 1. O tema

### 1.1 O que estava mal

`DockPanel`, `ConsoleWidget`, `TabLayout`, `Splitter`, o editor de codigo e uns vinte widgets mais pintavam com literais (`SetColor(38, 38, 42, 255)` e parecidos) em vez de
`Theme`. Com o preset claro ficavam ilhas escuras com texto escuro, ou texto claro sobre branco (o texto de uma linha seleccionada de uma arvore era sempre branco, o da
caixa de edicao de uma celula tambem). Alem disso:

- `Theme::instance() = Theme::light()` (o que as aplicacoes fazem) nao avisava ninguem;
- o fundo da janela (`WidgetApp::bgColor()`) era 30,30,30 fosse qual fosse o tema;
- as barras de rolar da `ConsoleWidget` e da `TreeView` eram de uma cor fixa, nao a do `ScrollBar`;
- os icones de uma folha de sprites da `ToolBar` levavam sempre a tinta branca (a de `toolbarIcon` calculava-se e nao se passava);
- `CodeEditor`: a linha do cursor era um bloco opaco 45,50,60 por tras do texto (ilegivel no claro) e as cores da sintaxe eram as de um fundo escuro.

### 1.2 O que mudou nos widgets

Cada cor fixa passou a vir do `Theme`, com os valores do preset escuro **iguais aos literais de antes** (o tema escuro desenha o que sempre desenhou: ver 1.6).
Os campos novos estao em grupos, por widget (`Theme::dock.tabBg`, `Theme::console.warn`, ...), e em quatro campos soltos. Cada grupo traz em `Theme.hpp` o seu `light()`.
O `macOS` e o `Windows XP` partem do claro; o `macOS escuro` parte do escuro.

Campos soltos (escuro / claro / macos / macos-dark / xp):

| campo | uso | escuro | claro | macos | macos-dark | xp |
|---|---|---|---|---|---|---|
| `selectionText` | texto de uma linha seleccionada (TreeView, DataGrid, TreeGrid, PropertyGrid, FileDialog, e a linha sob o rato da lista de um ComboBox) | 255,255,255 | 20,20,24 | 20,20,24 | 255,255,255 | 0,0,0 |
| `iconColor` | tinta dos icones que um widget desenha sem cor propria (`PaintContext::drawIcon` sem tinta) | 255,255,255 | 52,52,58 | 52,52,58 | 255,255,255 | 30,30,30 |
| `textSecondary` | texto um passo mais fraco que `textColor` (ligacoes de um Breadcrumbs, descricao do PropertyGrid) | 150,150,155 | 90,90,96 | 90,90,96 | 150,150,155 | 90,90,80 |
| `inputText` | texto escrito numa caixa com o foco (celula de uma grelha a editar, valor no PropertyGrid) | 255,255,255 | 20,20,24 | 20,20,24 | 255,255,255 | 0,0,0 |

Os grupos (valores do escuro e do claro; o que o macos, o macos-dark e o xp mudam esta a seguir a tabelas):

### Theme::dock (DockColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `dock.leafBg` | 38,38,42 | 250,250,252 |
| `dock.border` | 60,60,65 | 200,200,205 |
| `dock.handle` | 50,50,53 | 214,214,220 |
| `dock.handleHover` | 90,90,95 | 176,176,186 |
| `dock.barBg` | 30,30,33 | 232,232,236 |
| `dock.barBorder` | 55,55,60 | 206,206,212 |
| `dock.tabBg` | 35,35,38 | 222,222,228 |
| `dock.tabActiveBg` | 45,45,50 | 250,250,252 |
| `dock.accent` | 120,160,220 | 0,120,215 |
| `dock.tabText` | 150,150,155 | 80,80,86 |
| `dock.tabTextActive` | 220,220,220 | 30,30,30 |
| `dock.closeIcon` | 110,110,115 | 120,120,126 |
| `dock.closeIconActive` | 170,170,175 | 90,90,96 |
| `dock.closeIconHot` | 235,235,235 | 20,20,24 |
| `dock.closeHotBg` | 90,90,96 | 196,196,204 |
| `dock.dragBg` | 50,55,65,230 | 238,242,250,235 |
| `dock.dragBorder` | 100,140,220,180 | 0,120,215,200 |
| `dock.dragText` | 220,230,240 | 30,30,36 |

### Theme::console (ConsoleColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `console.bg` | 22,24,28 | 250,250,252 |
| `console.toolbarBg` | 30,32,38 | 238,238,242 |
| `console.inputBg` | 26,28,34 | 244,244,247 |
| `console.line` | 50,52,60 | 210,210,216 |
| `console.buttonOff` | 40,42,48 | 228,228,232 |
| `console.buttonOffText` | 80,80,90 | 96,96,102 |
| `console.clearBg` | 50,52,60 | 222,222,228 |
| `console.clearIcon` | 170,170,180 | 80,80,90 |
| `console.searchBg` | 20,22,26 | 255,255,255 |
| `console.searchBorder` | 70,120,200 | 0,120,215 |
| `console.searchHint` | 80,80,90 | 110,110,118 |
| `console.text` | 200,210,220 | 30,34,40 |
| `console.trace` | 140,140,150 | 84,84,94 |
| `console.warn` | 240,200,80 | 120,75,0 |
| `console.error` | 240,80,80 | 170,20,20 |
| `console.infoTag` | 120,180,255 | 0,80,160 |
| `console.warnBg` | 60,50,20,40 | 255,235,170,120 |
| `console.errorBg` | 80,25,25,50 | 255,200,200,90 |
| `console.selection` | 50,70,120,80 | 0,120,215,60 |
| `console.timestamp` | 90,90,100 | 96,96,106 |
| `console.badgeText` | 100,140,200 | 30,90,170 |
| `console.badgeBg` | 40,60,100,120 | 180,210,245,120 |
| `console.prompt` | 100,160,240 | 0,100,190 |
| `console.caret` | 180,200,240,200 | 30,60,120,220 |

### Theme::layout (LayoutColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `layout.tabBarBg` | 38,40,44 | 232,232,236 |
| `layout.tabBg` | 42,44,50 | 222,222,228 |
| `layout.tabActiveBg` | 55,58,65 | 250,250,252 |
| `layout.tabAccent` | 100,160,220 | 0,120,215 |
| `layout.tabTextInactive` | 0,0,0,0 | 80,80,86 |
| `layout.tabCloseIcon` | 100,100,105,150 | 120,120,126,200 |
| `layout.tabCloseIconActive` | 160,160,165,200 | 90,90,96 |
| `layout.splitHandle` | 65,65,70 | 214,214,220 |
| `layout.splitHandleHover` | 80,80,90 | 190,190,200 |
| `layout.splitHandleDrag` | 100,100,110 | 160,160,172 |
| `layout.splitGrip` | 130,130,138,200 | 130,130,138,200 |
| `layout.splitGripHot` | 180,180,190,230 | 70,70,80,230 |

### Theme::app (AppColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `app.crumbHover` | 120,170,220 | 0,100,190 |
| `app.crumbSeparator` | 100,100,105 | 130,130,136 |
| `app.searchIcon` | 140,140,145 | 110,110,116 |
| `app.rowHover` | 255,255,255,15 | 0,0,0,15 |
| `app.outlinerArrow` | 180,180,185 | 80,80,86 |
| `app.eyeOn` | 180,180,185 | 80,80,86 |
| `app.eyeOff` | 80,80,85 | 170,170,176 |
| `app.lockOn` | 200,150,50 | 170,110,0 |
| `app.lockOff` | 100,100,105 | 150,150,156 |
| `app.richCodeBg` | 25,25,28 | 235,235,240 |
| `app.richCode` | 180,210,180 | 30,100,40 |
| `app.richBold` | 255,255,255 | 0,0,0 |
| `app.richItalic` | 200,200,205 | 60,60,66 |
| `app.richLink` | 100,170,230 | 0,90,190 |
| `app.gridCellDim` | 170,170,175 | 90,90,96 |
| `app.progressText` | 220,220,220 | 30,30,30 |
| `app.dateHeader` | 140,140,160 | 90,90,110 |
| `app.timeSeparator` | 120,120,130 | 90,90,100 |
| `app.statusBg` | 35,36,40 | 232,232,236 |

### Theme::file (FileColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `file.divider` | 60,62,66 | 206,206,212 |
| `file.headerLine` | 55,58,65 | 214,214,220 |
| `file.columnSeparator` | 105,110,122,180 | 176,176,186,180 |
| `file.sortArrow` | 150,155,165,200 | 90,90,100,200 |
| `file.selectionBg` | 40,70,120,200 | 190,215,245 |
| `file.hoverBg` | 50,55,65,100 | 0,0,0,20 |
| `file.dirText` | 120,180,255 | 0,80,170 |
| `file.sizeText` | 160,160,165 | 90,90,96 |
| `file.dateText` | 140,140,145 | 90,90,96 |
| `file.previewHint` | 100,100,110 | 110,110,118 |
| `file.previewSize` | 130,130,140 | 100,100,108 |

### Theme::prop (PropColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `prop.stringValue` | 200,220,255 | 0,70,150 |
| `prop.colorHex` | 180,180,180 | 80,80,86 |
| `prop.comboValue` | 180,220,180 | 20,100,40 |
| `prop.sliderText` | 230,230,230 | 20,20,24 |
| `prop.descName` | 220,220,225 | 30,30,34 |

### Theme::editor (EditorColors)

| campo | escuro (e macos-dark, salvo nota) | claro (e macos, xp, salvo nota) |
|---|---|---|
| `editor.nodeBg` | 32,34,38 | 240,240,243 |
| `editor.nodeBorder` | 50,52,58 | 200,200,206 |
| `editor.nodeGridMinor` | 40,42,48 | 228,228,233 |
| `editor.nodeGridMajor` | 50,52,58 | 210,210,216 |
| `editor.nodeBody` | 40,42,48,240 | 250,250,252,240 |
| `editor.nodeBodySelected` | 50,55,65,240 | 232,240,252,240 |
| `editor.nodeEdge` | 60,62,70 | 186,186,194 |
| `editor.nodePinLabel` | 190,192,200,220 | 60,60,66,230 |
| `editor.assetBg` | 22,24,28 | 250,250,252 |
| `editor.assetBar` | 30,33,38 | 238,238,242 |
| `editor.assetLine` | 45,48,55 | 210,210,216 |
| `editor.assetPath` | 160,165,175 | 80,80,88 |
| `editor.assetName` | 170,175,185 | 60,60,66 |
| `editor.assetNameSelected` | 220,225,235 | 0,0,0 |
| `editor.assetRowName` | 180,185,195 | 50,50,56 |
| `editor.assetRowNameSelected` | 230,235,245 | 0,0,0 |
| `editor.assetButtonOff` | 40,44,52 | 222,222,228 |
| `editor.assetRowSelected` | 50,80,130 | 190,215,245 |
| `editor.assetRowEven` | 26,28,34 | 250,250,252 |
| `editor.assetRowOdd` | 30,33,38 | 244,244,247 |
| `editor.assetScrollThumb` | 60,65,75,200 | 150,150,158,200 |
| `editor.thumbBg` | 24,26,30 | 244,244,247 |
| `editor.thumbEdge` | 50,52,58 | 200,200,206 |
| `editor.thumbLabel` | 180,182,190 | 60,60,66 |
| `editor.thumbLabelSelected` | 255,220,80 | 150,90,0 |
| `editor.codeCurrentLine` | 45,50,60 | 222,232,248 |
| `editor.minimapBg` | 25,28,34,240 | 238,238,242,240 |
| `editor.minimapSlider` | 180,195,220,30 | 60,80,120,40 |
| `editor.minimapSliderEdge` | 140,160,190,60 | 60,80,120,90 |
| `editor.minimapLine` | 255,255,255,15 | 0,0,0,18 |
| `editor.minimapEdge` | 50,55,65 | 206,206,212 |
| `editor.tlBg` | 30,32,36 | 244,244,247 |
| `editor.tlBorder` | 50,52,58 | 200,200,206 |
| `editor.tlRulerBg` | 38,40,46 | 228,228,233 |
| `editor.tlRulerLine` | 60,62,68 | 200,200,206 |
| `editor.tlTickMajor` | 80,82,90 | 120,120,128 |
| `editor.tlTickMinor` | 55,57,63 | 170,170,178 |
| `editor.tlRulerText` | 150,152,160,200 | 70,70,78,230 |
| `editor.tlRulerTextMinor` | 110,112,120,200 | 90,90,98,230 |
| `editor.tlTrackName` | 180,180,180 | 50,50,56 |
| `editor.tlRowEven` | 34,36,40 | 246,246,249 |
| `editor.tlRowOdd` | 30,32,36 | 238,238,242 |
| `editor.tlHeader` | 36,38,44 | 232,232,236 |
| `editor.tlHeaderSelected` | 52,60,80 | 200,218,244 |
| `editor.tlButtonOff` | 56,58,66 | 206,206,212 |
| `editor.tlButtonText` | 235,235,240 | 40,40,46 |
| `editor.tlTrackLine` | 45,47,52 | 214,214,220 |
| `editor.tlHeaderLine` | 50,52,58 | 200,200,206 |

O que os outros presets mudam alem de herdar o claro (macos, xp) ou de voltar ao escuro (macos-dark):

- **macos**: `dock.accent`, `console.searchBorder`, `layout.tabAccent` no azul do sistema 0,122,255; `dock.dragBorder` 0,122,255,200.
- **macos-dark**: os grupos do escuro, com o azul 10,132,255 em `dock.accent`, `console.searchBorder`, `layout.tabAccent` e `dock.dragBorder`; e uns textos pouco visiveis do
  escuro mais claros (la le-se, e o escuro tem de ficar como estava): `console.buttonOffText`, `console.searchHint`, `console.timestamp` 132,132,142; `console.error` 255,120,110;
  `console.trace` 165,165,175; `console.badgeText` 130,170,230; `layout.tabTextInactive` 150,150,156; `editor.tlRulerText` 190,192,200,230 e `editor.tlRulerTextMinor` 150,152,160,230.
- **xp**: `dock.accent` e `layout.tabAccent` 49,106,197; `dock.barBg` e `layout.tabBarBg` 214,223,247; `dock.tabBg` e `layout.tabBg` 198,211,247; `dock.tabActiveBg` 255,255,255.

Mais:

- `ConsoleWidget::setColors(const ConsoleColors&)`, `useThemeColors()` e `colors()`: as cores de uma consola so, no lugar das do tema. `setBgColor` continua; passou a valer 0 de alfa por
  omissao (segue o tema) e `setBgColor(Color(0,0,0,0))` volta a seguir.
- `ScrollBar::trackColor()` e `ScrollBar::thumbColor(bool active)`: as cores de uma barra de rolar (as do `ScrollBar`), que a `ConsoleWidget` e a `TreeView` usam agora, e que qualquer
  widget com uma barra propria pode usar.
- `SyntaxHighlighter::colorFor(TokenType, bool lightBackground)`: a paleta clara para fundos claros (cada cor com contraste 4,5 ou mais sobre branco), excepto nos tokens a que a aplicacao
  deu cor com `setColor` (a escolha dela vale nos dois). O `CodeEditor` escolhe pela `Theme::isLight()` (nova: o fundo da janela e claro) de cada vez que pinta: nada fica guardado.
- `StatusBar`, `ProgressBar` (texto), `ThumbnailGrid`, `NodeEditor`, `Timeline` e `WidgetApp::setBgColor`: o fundo/cor que a aplicacao nunca deu segue o tema (alfa 0 por omissao: os
  getters `StatusBar::bgColor()` e `ProgressBar::textColor()` devolvem a cor que se pinta). Quem deu uma cor fica com ela.
- `PaintContext::drawIcon(id, x, y, size)` (sem tinta) usa `Theme::iconColor`.
- `Timeline`: o rotulo de um clip e branco como sempre; num tema claro, onde o preenchimento do clip e (muitas vezes) claro, passa a escuro quando e mais legivel.
- `ToolBar`: os icones da folha de sprites levam a tinta `toolbarIcon` (`toolbarIconDisabled` se o botao esta desactivado) quando `setColoredIcons` esta desligado.

### 1.3 `WidgetApp::themeChanged`

```cpp
void WidgetApp::setTheme(const Theme& theme);          // substitui Theme::instance(), avisa
bool WidgetApp::setThemePreset(const char* name);      // "dark", "light", "xp", "macos", "macos-dark"; false (sem mudar nada) se o nome nao e um preset
void WidgetApp::notifyThemeChanged();                  // "mexi no Theme::instance() a mao": o mesmo, depois do facto
Signal<> WidgetApp::themeChanged;
```

O contrato:

- `themeChanged` e emitido **uma vez por mudanca**, depois da mudanca e antes da arrumacao que ela pede (`requestRelayout()`: os contentores arrumam de novo os filhos e as
  medidas que vem do tema, tamanho do texto, margens, grossura das barras, tomam os valores novos; os rectangulos que a aplicacao deu aos filhos directos do palco ficam).
- Quem escreve `Theme::instance() = ...` ou um campo, sem chamar nada, tambem e apanhado: `update()` compara o tema com o que viu na frame anterior (um `memcmp` do `Theme`, cerca de
  1,5 KB) e emite na frame seguinte. Uma mudanca ja notificada (por `setTheme`, `notifyThemeChanged` ou por `update()`) nao se emite outra vez.
- **Cores que a aplicacao deu a um widget** (`setBgColor`, `ConsoleWidget::setColors`, `SyntaxHighlighter::setColor`, `StatusBar::setBgColor`...) ficam como estao: o sinal e o gancho da
  aplicacao para as dar de novo.
- Os handlers ligados a `themeChanged` ficam ligados (o `WidgetApp` e unico): quem liga uma lambda que captura estado local tem de a desligar (`disconnect`).
- Caches que guardavam coisas do tema, procuradas em todos os widgets: as memorias de texto (`ElideMemo`) comparam o tipo de letra, as de tamanho (`HintMemo`) refazem-se na
  arrumacao, `ConsoleWidget::barW_`, `ScrollView::barRoom_`, `Label::dots_` e `ComboBox::widestItem_` refazem-se na arrumacao ou comparam o tamanho do texto, a `TextEdit` guarda as
  linhas quebradas com a chave (largura, tipo de letra, escala): tudo isto muda de chave com o tamanho do texto e nao pede nada. **O unico valor do tema que ficava guardado sem
  chave** era `IconButton::iconColor_ = Theme::instance().textColor` (o icone animado do menu, com a cor do tema de quando nasceu): corrigido (alfa 0 segue o texto do tema; `setIconColor`
  continua a valer). As cores da sintaxe nao se guardam: a `TextEdit` pede os trechos coloridos de cada linha em cada pintura.
- `WidgetApp::watchTheme(bool)`: desliga a procura do `update()` (uma aplicacao que escreve um campo do tema todas as frames, um acento que pulsa, nao quer uma arrumacao por frame);
  `setTheme`, `setThemePreset` e `notifyThemeChanged` avisam sempre. Os casos da ronda 6 que verificam o que a pintura faz com um tema mudado e sem nova arrumacao ligam-no.

### 1.4 Varrimento de todas as cores escritas no codigo de pintura

Procurou-se (`grep` de `SetColor(<numero>`, `Color(<numero>`, `Color{`, `{r,g,b,a}`, `0xAARRGGBB` e os ternarios com literais) em todo o `src/widgets`. Cada uma e uma de tres:

**Veio para o Theme (corrigido, escuro igual ao de antes):**
`DockPanel` (barras, abas, cruzes, pegas, fundo, arrasto), `ConsoleWidget` (barra, registo, entrada, niveis, selecao, distintivo, cursor; a barra de rolar e a do `ScrollBar`),
`TreeView` (barra de rolar, texto da linha seleccionada, icones de alternar), `TreeGrid`/`DataGrid` (texto seleccionado, a editar, celulas fracas), `PropertyGrid` (texto seleccionado, a editar,
valores texto/combo/hex/barra, descricao), `ToolBar` (icones de folha de sprites), `PaintContext::drawIcon` sem tinta, `TabLayout` e `Splitter` (barra, abas, indicador, cruz, pega, pontos),
`StatusBar`, `ProgressBar` (texto), `DatePicker` e `TimePicker` (nomes dos dias, dois pontos), `Breadcrumbs`, `SearchBar`, `Outliner` (olho, cadeado, seta, rato), `RichText` (codigo, negrito, italico,
ligacao), `FileDialog` (linhas, colunas, selecao, nomes de pastas, tamanho, data, antevisao), `AssetBrowser`, `ThumbnailGrid`, `NodeEditor` (fundo, grelha, no, texto dos pinos),
`Timeline` (fundo, regua, linhas, pistas, cabecalhos, botoes M e L, rotulo dos clips num tema claro), `CodeEditor` (linha do cursor, minimapa, e as cores da sintaxe com a paleta clara),
`IconButton` (cor do icone), o fundo da janela (`WidgetApp::bgColor`).

**Deixado, porque e conteudo, uma sobreposicao que se le sobre qualquer fundo, ou o aspecto proprio de um instrumento:**
- *Sobreposicoes de destaque* (azul/ambar translucido que se ve em fundo claro e escuro): a area de largar do `DockPanel` (80,120,200,60), a linha e a caixa de largar da `TreeView` e do `Outliner` (100,170,255),
  o contorno de largar do `ThumbnailGrid` (90,170,255), o rato e a selecao de um dia no `DatePicker` (60,140,255), a moldura da selecao (255,200,60) do `ThumbnailGrid`, do `NodeEditor` e da `Timeline`,
  as ocorrencias de uma pesquisa e o par de parenteses no `CodeEditor` (ambar), o cursor de reproducao e as marcas da `Timeline`.
- *Sombras neutras* (0,0,0,alfa): a sombra da aba que se arrasta, a dos nos do `NodeEditor`, a dos menus e janelas, o escurecer da base de uma miniatura, o veu das gavetas.
- *Texto branco sobre um acento saturado*: o dia seleccionado de um `DatePicker`/ano (sobre `focusColor` com alfa 200: branco sobre 0,120,215 da 4,7; no escuro o `focusColor` e cinzento e fica como estava),
  os rotulos dos clips sobre a cor do clip (no claro passam a escuros quando e mais legivel).
- *Cartoes com a sua propria cor e o seu texto*: `Toast` (fundo escuro por tipo, texto 240,240,245), as setas e os pontos de um `Carousel` (sobre as imagens).
- *Instrumentos com paleta propria e `setBgColor`/`setColor` do seu*: `Knob`, `Fader`, `LinearKnob`, `VUMeter`, `ADSRWidget`, `SpectrumAnalyzer`, `WaveformView`, `PianoRoll`, `HistogramWidget`, `PlotWidget`,
  `CurveEditor`, `GradientEditor` (AudioWidgets), os mostradores de `AutomotiveWidgets`, `ChartWidgets`/`ChartWidget` (series, fundo 28,30,34 com `setBgColor`), `Gizmo2D`/`Gizmo3D`, `Viewport3D` e
  `VideoCanvas` (a superficie de desenho, 14,14,16 / 20,20,22), a face das miniaturas do `AssetBrowser` (a cor do item escurecida) e o interruptor `Switch` (trilho cinzento escuro 70,70,75,
  com o polegar branco). Se uma aplicacao os quiser claros, usa os seus setters; ficam por tratar se se quiser que sigam o tema por omissao (nao se fez: sao muitos, e cada um e uma decisao de aspecto).
- *Cores do conteudo*: as cores de uma serie, de uma pista, de um clip, de um no, de um item (as que a aplicacao da); a paleta do `IconAtlas` (desenhos de icones: ver a seguir).
- *Icones coloridos do `IconAtlas`* (gravar vermelho, visto verde, cruz vermelha, aviso amarelo): um icone desenhado sem tinta leva agora `Theme::iconColor`, que e multiplicativa: no tema claro os pormenores
  coloridos escurecem junto com o branco (continuam legiveis, perdem a cor). Para os manter a aplicacao passa a tinta branca explicita (`drawIcon(id, x, y, size, Color(255,255,255,255))`).

**Cores de depuracao ou de teste**: as linhas coloridas da sobreposicao de depuracao da arrumacao (`WidgetApp.cpp`, `debugColor`); nenhuma outra.

### 1.5 O que o tema escuro deixa abaixo de 4,5 de contraste (e fica como esta)

O preset escuro tem de desenhar o que sempre desenhou (a soma de controlo do desenho da janela de teste nao muda; ver 1.6): por isso as cores abaixo, que ja eram assim, nao se mexeram. O teste
de legibilidade (4,5 em todos os presets) tem-nas isentas **so no escuro**, cada uma com o seu minimo (1,5):
`console.timestamp` (2,6 sobre o registo), `console.buttonOffText` (1,8, um filtro desligado), `console.searchHint` (2,3), `console.error` sobre a linha vermelha da sua mensagem (3,8), `console.trace` sobre o filtro
ligado (3,9), `console.badgeText` (4,2), o nome de uma aba que nao e a actual (`textDisabled` sobre 42,44,50: 3,2), o texto de uma linha seleccionada de uma `ListBox`/`ListWidget`/`ComboBox` (`textColor`
220 sobre a selecao: 3,8, com o minimo desse caso a 3,5; o texto da `TreeView`, `DataGrid`, `PropertyGrid` e `FileDialog` e `selectionText`, branco, e passa), os comentarios no `CodeEditor`
(106,115,125 sobre 35,35,38: 3,3) e o texto da regua da `Timeline` (150,152,160,200: 3,7). **O pedido "texto seleccionado a 4,5 nos quatro presets" nao se cumpre no escuro para a `ListBox`**: o seu texto
seleccionado e `textColor` e nao branco, e mudar-lhe a cor mexia no que o escuro desenha. No `macos-dark` (um look novo, sem a regra do escuro exacto) os que sao do `Theme` passaram para valores que leem bem;
o dos comentarios fica (e uma convencao). Em todos os presets ficam isentos `textDisabled` e `lineNumberColor` (o `test_themes.cpp` ja os mede com minimos proprios: sao "mais fracos de proposito") e as
cores de pista (a cor do projecto).

### 1.6 O que muda no escuro por pedido, e o que o teste mede

- A barra de rolar da `ConsoleWidget` e da `TreeView` passou a ser a do `ScrollBar` (pedido: item 4): o rasto era 25,27,32,200 e o polegar 70,75,85,180 (100,106,118,200 a arrastar); agora o rasto e `panelColor` com alfa 80 e
  o polegar `scrollbarThumb`. So se ve quando ha conteudo a rolar.
- Os icones de uma folha de sprites da `ToolBar` levam `toolbarIcon` (220,220,225) em vez de branco (pedido: item 5); o editor do banco nao usa folhas.
- `bench/retained/draw.hash` **nao mudou** (`check.sh` fase `hash`); a soma de controlo da janela de teste (todos os tipos de widget, sem barras de rolar visiveis) no escuro e a da biblioteca de `origin/main`:
  tirada com o `0d86927` (`tests/test_retained_round9.cpp`, `kDarkGolden`).

`tests/test_retained_round9.cpp` (alvo `igui_retained_round9_tests`, corre no `check.sh` em Release e sob ASan+UBSan): uma janela de 1280x720 com cerca de 50 widgets (`tests/Round9Window.hpp`: barra
de ferramentas, dock com consola, arvore e grelha de propriedades, abas, divisor, controlos, listas, grelhas, miniaturas, navegador de ficheiros, editor de codigo com minimapa e foco, linha de tempo, editor de nos, botao de menu)
e, em cada um dos cinco presets (escuro, claro, xp, macos, macos-dark) trocados em execucao:

- (a) a soma de controlo do desenho no escuro e a de antes; um preset volta a desenhar o mesmo quando regressa (nada guarda a cor do anterior);
- (b) nenhum preenchimento escuro de regiao (triangulos de 40 px2 ou mais, 3 px de altura, opacos, luminancia abaixo de 90) nos presets claros, em cada widget "de fundo"; o conteudo (cor de miniatura, cabecalho de um no, trilho do interruptor) esta isento;
- (c) `themeChanged` uma vez por `setTheme`, `setThemePreset`, atribuicao a `Theme::instance()`, campo escrito e `notifyThemeChanged`, nunca nas frames seguintes; a arrumacao e feita de novo (um widget que conta as arrumacoes);
  cores dadas pela aplicacao (`setBgColor`, `setColors`) ficam; `watchTheme(false)` nao procura;
- (d) legibilidade: o contraste de cada glifo contra o que esta por baixo (os preenchimentos desenhados antes, compostos pelo alfa, lidos da lista de desenho) tem 4,5 ou mais, incluindo o texto de uma linha seleccionada
  da arvore, da lista, da grelha de dados e da grelha de propriedades; um controlo negativo (texto branco numa linha seleccionada no tema claro) tem de ser apanhado.

Mais: as cores da barra de rolar da consola e da arvore sao as do `ScrollBar` em cada preset; os icones de uma folha de sprites levam a tinta do tema; o botao de menu segue o texto do tema. O
`test_themes.cpp` ganhou o contraste dos pares dos campos novos.

As imagens, a 1x, do que o teste desenha em cada preset (a segunda de cada par com a consola e a arvore cheias, com barras de rolar):
`docs/img/round9/window-<preset>.png` e `window-<preset>-scrolling.png`, para `dark`, `light`, `xp`, `macos` e `macos-dark`.

### 1.7 Verificacao da parte 1

`tools/check.sh` (com `IGUI_CONTAINERS_DIR` e `IGUI_ZEN_PLATFORM_DIR`, `--jobs 3`, base `origin/main`), na ponta do ramo `igui-round9` depois da parte 1:

```
PASS nostd: nothing new with std:: or #include <...> in src, include and adapters since origin/main
PASS release: 100% tests passed, 0 tests failed out of 49
PASS asan: 100% tests passed, 0 tests failed out of 49
PASS hash: what the bench draws is what bench/retained/draw.hash says
----
check.sh: 4 stages, 0 failed
```

49 testes (os 48 de antes e `igui_retained_round9`), nenhum Skipped. O caso `igui_retained_round6` ganhou `WidgetApp::watchTheme(false)` no `ThemeScope` (os casos medem o que a pintura faz sem nova
arrumacao; a procura automatica do `update()` faria a arrumacao).

Custo de instrucoes da procura do tema (um `memcmp` de cerca de 1,5 KB por frame; instrucoes de utilizador do lado do GUI de uma frame, `cmp_igui --soft`, antes = `origin/main` 0d86927):

| cena | fase | antes | depois | variacao |
|---|---|---:|---:|---:|
| A | idle | 6 990 | 7 199 | +209 (+3,0%) |
| A | hover | 9 837 | 10 017 | +180 |
| B | idle | 2 705 611 | 2 705 313 | -298 |
| B | hover | 2 731 043 | 2 730 716 | -327 |
| B | click | 2 782 084 | 2 781 767 | -317 |
| F | idle | 2 957 230 | 2 957 439 | +209 |
| F | hover | 2 961 473 | 2 961 653 | +180 |

Os ~200 de mais sao a comparacao do tema; so se notam numa janela vazia (cena A), e o item 3 da parte 2 (nada mudou: nao pintar) tira-os com folga.

## 2. As perdas medidas (docs/COMPARACAO.md seccao 4)

Medido como na ronda anterior (`tools/bench_compare.sh`, as mesmas cenas A a F, o ImGui 1.92.5 e o mesmo Xvfb privado, a maquina partilhada com carga de 15 a 40): as
**instrucoes de utilizador do lado do GUI de uma frame** (contadas passo a passo, nao dependem da carga), a memoria (PSS) e as chamadas de desenho nao dependem do ruido; o CPU parado e a latencia
(tempos) dependem, leem-se pelas razoes grandes. "Antes" e a biblioteca de `origin/main` (0d86927), construida com os mesmos `-O2` e medida com o mesmo `cmp_igui`; "depois" e a ponta deste ramo.
A tabela completa de depois esta em `docs/COMPARACAO.md` seccao 10.

### 2.1 Cenas densas de widgets (B: 200 widgets, F: 50 janelas)

**O que se fez** (um commit; cada passo medido, instrucoes de uma frame parada, `cmp_igui --soft`):

| passo | B (form de 200 widgets) | F (50 janelas) |
|---|---:|---:|
| `origin/main` | 2 705 611 | 2 957 230 |
| + procura do tema por frame (parte 1) | 2 705 313 | 2 957 439 |
| cantos pelo erro de 0,15 px (3 segmentos num raio 4, eram 5 a 6) | 2 229 687 | 2 563 094 |
| `DrawVertex` sem valores iniciais (`resize` deixa de os escrever) | 2 129 476 | |
| preenchimento e borda de um rectangulo redondo a partir de tabelas | 1 829 994 | 1 985 676 |
| `mapPoint` em linha, `floor` sem chamada no texto | 1 746 337 | 1 864 447 |
| uma linha de dois pontos sem a maquinaria do `pathStroke` | 1 744 610 | 1 799 156 |
| comandos fundidos, a primeira versao (mede os vertices de cada comando) | 1 784 636 | 2 117 392 |
| ... com os limites que cada forma diz ter, em vez de os medir | 1 801 530 | 1 949 440 |
| tabela dos glifos ASCII em cada `Font` | 1 744 239 | 1 850 072 |
| UTF-8 ASCII sem chamada, texto sem `mapPoint` quando nao ha transformacao | 1 723 386 | 1 818 534 |
| (item 3: os modificadores sem `key_mods`) | **1 720 593** | **1 818 398** |

- **Cantos pelo erro.** `cornerSegments(raio, dica)`: o menor numero de segmentos para que nenhuma corda fique a mais de 0,15 px do arco (`r * (1 - cos(passo/2))`), nunca abaixo dos 3 da tabela antiga;
  a dica de quem chama so torna o canto mais grosseiro quando e menor. O valor por omissao da dica passou de 8 a 0 (`addRoundRectFilled`, `addRoundRect`): quem nao diz nada tem o erro. Um raio 4:
  3 segmentos a 0,14 px (era 5 ou 6); um raio 12: 5. Vertices de B: 24 932 -> 18 532.
- **Formas a partir de tabelas.** `fillRoundRectFast` e `strokeRoundRectFast`: o desvio de franja de cada ponto de um rectangulo redondo depende so dos angulos do arco, nao do raio nem do tamanho;
  calcula-se uma vez por numero de segmentos (pelas mesmas expressoes do `fillConvexPath` e do `pathStroke`) e cada rectangulo escreve os seus pontos sem raizes quadradas nem divisoes. O que as tabelas nao
  cobrem (sem anti-aliasing, um lado que nao e maior que os cantos: pilulas e circulos) segue o caminho de antes. `addLine` de dois pontos tem o seu. O teste compara vertices e indices com o caminho
  generico (`pathLineTo` + `pathFillConvex`/`pathStroke`) a um milesimo de pixel, com e sem transformacao.
- **`DrawVertex` sem valores iniciais.** A estrutura passou a ter construtores (o de omissao deixa o que la estava: a cor esta numa uniao anonima para que nem o construtor de `Color` corra) e o `resize`
  de `appendVertices` deixa de escrever cinco palavras por vertice que a seguir se escrevem por inteiro. **Mudanca de comportamento**: um `DrawVertex v;` ou `DrawVertex v{};` ja nao vem a branco com zeros;
  `DrawVertex{x, y, u, v, cor}` (a forma que a biblioteca usa) e o construtor de cinco valores continuam. O `resize` de `ct::Vector` (so le) nao tem uma versao que deixe o espaco como esta: ficam os indices
  (`appendIndices`, 4% de B) a escrever zeros; **pedido ao `ct`**: `resize_for_overwrite(n)`.
- **Comandos fundidos** (`DrawList::addCmd`; `setCommandMerging(false)` volta ao de antes). (a) No adaptador `zen` o texto de um janela esta na pagina dinamica do atlas e as formas na pagina de serie: cada
  texto fechava um comando (281 em B). A pagina dinamica passou a ter o texel branco no pixel (0, 0), onde a de serie o tem (o passo de empacotamento comeca em (1, 1): nenhum glifo mudou de sitio), e
  `DrawList::setSolidAlternate(pagina, uv)` / `SetSolidAlternate` (o `ZenApp` chama-o) faz as formas desenharem-se nela; as que ficam ao lado de texto da outra pagina mudam as coordenadas e passam para essa.
  (b) Comandos com clips diferentes juntam-se quando o clip nao importa ao que se desenha: os vertices de cada um (os limites que cada forma diz ter, `note()`, ou, se nao os diz, medidos) estao dentro do seu clip,
  ou dentro do clip sob o qual se juntam, e com as mesmas pontas de clips redondos. Desenhar sob o outro clip, ou sob a uniao dos dois, nao muda um pixel. **Chamadas de desenho de B: 281 -> 1; de F: 401 -> 1** (o ImGui:
  1 e 100). O teste (`tests/test_retained_drawlist.cpp`) desenha 320 cenas ao acaso (formas sob clips ao acaso, redondos tambem, texto das duas paginas) com a fusao ligada e desligada no rasteriador por software:
  **os mesmos pixels em todas**, e 15% menos comandos em conjunto.
- **Glifos ASCII.** `Font` guarda uma tabela com os 128 primeiros glifos: `findGlyph` de uma letra e uma leitura (o texto pede cada letra duas vezes, para medir e para desenhar).

**Resultado contra o ImGui no mesmo aspecto** (cantos de 4 px com borda de 1 px nos dois: `cmp_imgui --rounded`; instrucoes de uma frame parada):

| cena | iGUI antes | iGUI depois | ImGui (mesmo aspecto) | iGUI / ImGui antes | depois | alvo |
|---|---:|---:|---:|---:|---:|---|
| B | 2 705 611 | 1 720 593 | 1 567 272 | 1,73 | **1,10** | 1,2 |
| F | 2 957 230 | 1 818 398 | 1 517 630 | 1,95 | **1,20** (1,198) | 1,2 |

| cena | chamadas de desenho antes | depois | vertices antes | depois |
|---|---:|---:|---:|---:|
| B | 281 | **1** | 24 932 | 18 532 |
| F | 401 | **1** | 25 764 | 20 564 |

O alvo de 1,2x do ImGui no mesmo aspecto e atingido em B e, por pouco (1,198), em F; o de menos de 10 chamadas em B (1) tambem. A de-serie do ImGui (sem cantos): B 666 790 instrucoes
(a iGUI faz 2,6x) e F 750 464: a diferenca de aspecto continua a ser a de 4.1 da ronda anterior.

**Nenhuma cena ficou mais lenta** (instrucoes de uma frame, antes -> depois): A idle 6 990 -> 3 860; B 2 705 611 -> 1 720 593; C idle 238 396 -> 209 330; D idle 162 889 -> 128 943; E idle 1 003 526 -> 813 745,
E a escrever 3 943 924 -> 3 745 267; F 2 957 230 -> 1 818 398 (todos os passos, a roda, o rato e o clique: COMPARACAO.md seccao 10, da corrida final de `bench_compare.sh`).

**O desenho muda, o aspecto nao** (a regra: o que muda o desenho muda `bench/retained/draw.hash`, com imagens antes e depois). Os cantos pelo erro mudam os vertices, os comandos fundidos mudam os comandos:
`bench/retained/draw.hash` passa de (janela do editor) 9096 vertices e 201 comandos a 8432 e 197, (piano roll) 338 comandos a 298 com os mesmos vertices, (graficos) 20 430 vertices e 428 comandos a 16 622 e 416; e a
soma da janela de teste da ronda 9 (`kDarkGolden`) tem o motivo no comentario. As imagens da cena B e da F, antes e depois, a 1x e com a ampliacao a 8x do canto superior esquerdo:
`docs/img/round9/B-before-1x.png`, `B-after-1x.png`, `B-before-4x.png`, `B-after-4x.png` (as de `-4x` sao do recorte de 180x75 px ampliado 8 vezes sem filtro) e as mesmas com `F-`. Pixels que mudam, antes -> depois, por software:
B 6 022 de 700 000 (0,86%), 80 deles por mais de 6% (a franja de um canto), PSNR 56,4 dB; F 2 390, nenhum por mais de 6%, 66,5 dB; **por OpenGL** (llvmpipe): B 6 673, nenhum por mais de 6%, 59,8 dB; F 2 407, 65,0 dB.
Os numeros de pixeis sao os mesmos antes e depois dos comandos fundidos e das paginas de um byte: so os cantos mudam o que se ve.

### 2.2 Memoria de base

**O que se fez.** (a) O rasteriador por software faz a imagem por faixas de 128 linhas de amostras (`SoftRaster::setBandRows`): o buffer de amostras passa de 11,2 MB (1000x700 com 2x2) a 1 MB;
os mesmos pixels para qualquer tamanho de faixa (o teste desenha cenas ao acaso com uma faixa, 128 e 8 linhas, com 2 e 3 amostras). (b) As paginas do atlas que sao brancas com a cobertura em alfa ficam num byte por pixel:
a de serie e as texturas (`SoftRaster::createTexture` e `GlRaster::createTexture` detectam-no, `createTextureA8` pede-o; o GL guarda `GL_R8` com a troca `(1, 1, 1, R)`: o mesmo texel), e a pagina dinamica
vive num byte por pixel no proprio `FontAtlas` (`dynamicAlpha()`; `dynamicPixels()` faz uma copia RGBA so se alguem a pedir). O teste compara os pixeis de uma textura de um byte com os da inteira. (c) A copia da
ultima frame do rasteriador (2,8 MB) so se guarda quando uma frame sai igual a anterior (uma aplicacao que pinta sempre coisas diferentes, ou que salta as iguais, nunca a tem).

PSS da cena A (janela vazia), MB. Duas medidas, que dao coisas diferentes: (i) **`bench_compare.sh`** (a pintar todas as frames, sem o salto das iguais), a de COMPARACAO.md; (ii) uma medida minha **com o ciclo
da biblioteca como uma aplicacao o corre** (as frames sem mudancas nao se pintam, a copia da ultima frame nao se faz):

| cena A | antes | depois | ImGui | GTK 3 |
|---|---:|---:|---:|---:|
| por software, `bench_compare` | 42,5 | **24,9** | | 23 |
| por software, com o salto de frames (ii) | 43,6 | **21,9** | | 23 |
| GL, `bench_compare` (a mesma corrida do ImGui) | 55,1 | 56,8 | 50,2 | |
| GL, `bench_compare`, o ImGui da corrida de antes | | | 46,1 | |
| GL, com o salto de frames (ii) | 58,0 | 48,9 | 46,1 | |
| heap em uso, GL (malloc: **nao varia** de corrida a corrida) | 54,97 | **46,01** | 41,94 | |
| heap em uso, por software | 28,92 | **10,25** | | |

**Por software: abaixo do GTK so na medida (ii) (21,9 contra 23); na do `bench_compare` ficam 24,9 (1,9 acima do GTK): a diferenca e a copia da ultima frame (2,8 MB), que existe porque ha frames iguais seguidas
e que o ciclo com salto nunca faz. A meta "abaixo do GTK" cumpre-se por isso para uma aplicacao que corre o ciclo da biblioteca, e nao na tabela do `bench_compare`.** Em GL **a meta (abaixo do ImGui) nao se cumpriu**: o
heap, que e exacto, desce 9 MB (54,97 -> 46,01) mas o ImGui fica a 41,94, 4 MB abaixo; o PSS (56,8 contra 50,2 na mesma corrida; 48,9 contra 46,1 na medida (ii)) oscila 4 a 10 MB de corrida para corrida com o mesmo binario (o ImGui:
46,1 -> 50,2 na A, 49,7 -> 60,8 na B, sem se ter mexido nele), por isso a comparacao de PSS em GL entre duas corridas nao prova nada (o heap prova). O que se viu na pilha das alocacoes: 2,8 MB so no `driBindContext` da janela (Mesa);
a parte da biblioteca (a pagina de serie, a dinamica e a sua copia em memoria) ja e de um byte por pixel; os 4 MB que faltam ao heap em GL nao se analisaram mais. Sem mudar o aspecto (as imagens de B e F acima
sao as mesmas depois das paginas de um byte).

### 2.3 Parado

**A biblioteca.** `WidgetApp::needsFrame()`, `nextFrameInMs()`, `requestFrame()`, `requestFrameIn(s)`, `requestFrameFromAnyThread()`, `clockSeconds()`. Uma frame e precisa quando, desde a ultima que se pintou:
- um widget mudou (`Widget::markDirty()` incrementa um contador global; todos os setters da biblioteca o chamam; `setRect`, `setPosition`, `setSize` e as barras de rolar so o chamam se o valor mudou, e a grelha de
  propriedades deixou de esconder e mostrar os seus spins em cada pintura: era o que mantinha acordada uma janela parada) ou pediu uma arrumacao, trocou o tema, mudou de tamanho, ganhou ou perdeu o foco;
- chegou um evento (o rato mexeu, um botao, uma tecla, texto, a roda, um ficheiro largado);
- corre uma animacao (`Animator`), uma transicao de palcos, a abertura ou o fecho de um menu, uma notificacao, ou ha uma funcao de sobreposicao;
- uma dica apareceu ou desapareceu, e o prazo dela esta em `nextFrameInMs`;
- um widget pediu para daqui a pouco: a mira de `TextInput`, `TextEdit`/`CodeEditor` e `SearchBar` pisca a cada meio segundo **pelo relogio da aplicacao** (a soma dos tempos que `update()` recebeu, que continua a contar
  nas frames que nao se pintam), e cada pintura pede a seguinte para quando ela vira (`requestFrameIn`); as animacoes dentro do `paint` de `PageView`, `Carousel`, `StackLayout`, `SlidePanel` marcam-se sujas
  enquanto correm e `AnimatedIcon` pede `requestFrame()`.
Uma aplicacao que muda o que se desenha sem passar por um widget (a textura de um `VideoCanvas`, uma funcao de sobreposicao) chama `requestFrame()`; de outra thread, `requestFrameFromAnyThread()`.

**O adaptador `zen`.** `Config::skipIdleFrames` (ligado por omissao) / `App::setSkipIdleFrames`: depois do `update()` o `frameWindow` so pinta e apresenta se `needsFrame()`; o `pump()` dorme `min(nextFrameInMs, 8)` ms quando
nenhuma janela pintou. Os eventos continuam a ler-se e a despachar-se em cada passagem. **Nao se salta** com o sistema a ser dono do ciclo (web, Android), com uma captura ou `ZEN_SCREENSHOT` a espera, e **com uma funcao
por frame (`setFrameCallback`) ou de depois do desenho (`setAfterRenderCallback`)**: mudam o que se mostra sem avisar os widgets; quem as usa e sabe que elas pedem frame (`requestFrame()` ou
widgets sujos) liga `App::setIdleSkipWithCallbacks(true)`. As aplicacoes de hoje com funcao por frame (ZenVideo, ZenMix...) continuam a pintar todas as frames ate o fazerem: e o preco de nao poder
saber, e uma frame antiga e pior do que um ciclo quente.

Tambem: `feedInput` deixou de chamar `key_mods()` (uma ida ao servidor X, `XkbGetState`, a que a frame esperava, e tres alocacoes) e le o estado dos modificadores das teclas que a plataforma ja sabe premidas
(`key_down`): era o `key_mods` por frame da frame parada. `mouse_position` e so a leitura de uma estrutura; nada mais de X por frame parada na iGUI.

**CPU parado** (`cmp_igui B idlecpu`, 10 s, a janela do formulario B e nada a acontecer; "antes" corre o ciclo de antes, "depois" com o salto de frames, a maquina com carga de 18; a corrida final de `bench_compare.sh`
deu 0,8% e 0,7% e 118 e 119 acordares por segundo; a do `origin/main` na mesma ferramenta nao saiu (a pasta `raw` ficou vazia), os "antes" sao da minha corrida de `cmp_igui` com o ciclo de antes):

| | antes | depois |
|---|---:|---:|
| por software, CPU | 70,2% | **0,61%** |
| por software, acordares por segundo | 898 | 119 |
| OpenGL (llvmpipe), CPU | 172,9% | **0,63%** |
| OpenGL, acordares por segundo | 1 210 | 119 |

O FLTK e o GTK ficam a 0,0% e 0,1 a 0,6 acordares por segundo. A diferenca que resta, 119 acordares por segundo contra 0,1 a 0,6, e a espera: o `pump()` dorme 8 ms com `time_sleep` porque a
zen_plataform nao tem uma espera por eventos com tempo limite (ver abaixo).

**Pedido a zen_plataform** (para a ronda dela; nao se mexeu na zen_plataform):

```c
/* Espera por trabalho: bloqueia a thread ate haver pelo menos um evento do sistema por tratar para alguma janela do processo (o proximo
   window_begin_frame tem entao o que despejar), ou window_wake() ter sido chamado, ou passarem timeout_ms. Devolve true se acordou por um
   evento ou por window_wake, false por tempo. timeout_ms == 0: nao bloqueia (devolve se ha algo na fila); UINT32_MAX: sem tempo limite. */
PLATFORM_API bool window_wait_event(PlatformWindow *w, uint32_t timeout_ms);

/* Acorda um window_wait_event de qualquer thread. Se nenhuma espera esta a correr, a proxima volta de imediato (o acordar nao se perde), uma vez. */
PLATFORM_API void window_wake(PlatformWindow *w);
```

No X11: `XFlush`, `XPending > 0` volta logo (os eventos ja lidos para o buffer do Xlib), senao `poll()` no `ConnectionNumber(dpy)` e num `eventfd` (o do `window_wake`) com o tempo limite; no Windows
`MsgWaitForMultipleObjectsEx` com um evento; no Cocoa `nextEventMatchingMask:untilDate:` com `postEvent` para acordar. O adaptador troca `time_sleep(min(nextFrameInMs, 8))` por
`window_wait_event(pw, nextFrameInMs < 0 ? 1000 : nextFrameInMs)`, e `App::wake()` chama `window_wake` (a aplicacao liga-o a `requestFrameFromAnyThread`). Ganho: 1 a 2 acordares por segundo e a latencia
de um evento sem os 8 ms. Dois achados da leitura da zen_plataform: `key_mods()` chama `backend_lock_state()`, que faz um `XkbGetState` (ida e volta) e alocacoes de cada vez que e chamado: guardar o estado
das teclas de bloqueio a partir de `XkbStateNotify` e nao perguntar; e `sync_stuck_keys` faz um `XQueryKeymap` de 0,1 em 0,1 s enquanto ha uma tecla premida (aceitavel).

**Testes** (`tests/test_retained_frames.cpp`, alvo `igui_retained_frames_tests`, uma passagem do ciclo = `NewFrame`, `update` e so se `needsFrame()` `paint` e `Render`): numa janela de uns 50 widgets assentada,
100 frames sem nada sujo pintam 0; cada tipo de mudanca (uma propriedade, um tamanho, esconder e mostrar, arrumar, uma arrumacao, `requestFrame`, de outra thread, o tema por nome e por atribuicao, o rato,
um botao, a roda, o tamanho da janela, um widget novo) pinta **exactamente uma** frame e a seguir nenhuma; uma animacao de meio segundo pinta as suas 30 frames e depois nenhuma; a mira vira 6 vezes em 190 frames,
nas frames 27, 57, 87, 117, 147 e 177 (meio segundo e 30 frames, a tolerancia 3); a dica aparece no prazo; `requestFrameIn(0,25)` pinta na frame 15; `nextFrameInMs` diz cerca de 500 para meio segundo; e **as
mesmas 60 mudancas ao acaso, correndo a pintar todas as frames e a saltar, acabam todas na mesma imagem**.

### 2.4 Editor de texto: 428 bytes por linha

`TextEdit` sem quebra de linha deixa de guardar o `wrapCache_` (uma segunda copia de cada linha, a chave, com as suas linhas visuais): sem quebra uma linha e uma linha visual e a largura, e medi-la de novo, na
remedicao completa (um texto novo, um tipo de letra ou escala novos), custa menos do que guardar a copia. Com quebra a cache fica como estava. **428 -> 200 bytes por linha** (5 005 000 bytes em 91 000 linhas de 55:
heap 69,3 MB -> 29,0 MB, PSS 84,6 -> 44,3 MB, esta parte tambem com as paginas e as faixas da 2.2); sem mudar as instrucoes de uma frame de E (idle 813 745, roda 818 688, a escrever 3 745 267: as mesmas, a menos de uma dezena de instrucoes de ruido, antes e depois da mudanca
da cache; as de `origin/main` eram 1 003 526, 1 008 517 e 3 943 924, a diferenca e das outras mudancas desta ronda). **O alvo de menos de 100 bytes por linha nao se atingiu**: o resto sao `wrapRows_` (um `Vector` de `Vector<WrapRow>`: 24 bytes e um bloco
de heap por linha), `lineWidths_` e `rowStarts_`; representar a linha visual unica como implicita pede mexer em todos os sitios que leem `wrapRows_` (a pintura, o cursor, a seleccao, a rolagem, o `CodeEditor`): ficou para outra ronda.

### 2.5 Latencia do clique

Medida: o clique injectado numa caixa da cena B ate a frame que a mostra mudada estar apresentada. **Em GL: 2 frames (73,4 ms) -> 1 frame (26,2 ms; 18,0 ms na corrida final de `bench_compare.sh`, em que o ImGui deu 3 frames e 18,7 ms); por software: 1 frame nas duas (45,2 -> 50,1 ms, 37,9 ms na corrida final: a frame de
40 ms do rasteriador e a do clique, mais ate 8 ms de espera).** A frame a mais era do ciclo sem espera: o clique chegava a meio de uma frame ja comecada, que se pintava sem ele, e a seguinte e que o mostrava; com a
espera de 8 ms o ciclo esta parado a espera quando o evento chega, le-o ao acordar e pinta-o nessa frame. (Os eventos sao lidos no inicio da frame, antes do `update` e do desenho: nao ha atraso de estado para
a frame seguinte.) Com a espera por eventos da zen_plataform (acima) os 8 ms de espera desaparecem.

## 3. O que ainda se perde, e porque

- **Acordares por segundo parado: 119, nao 1 a 2** (falta a espera por eventos da zen_plataform, pedido em 2.3). CPU parado: 0,6% (era 70% e 173%).
- **Frames com funcao por frame** (`setFrameCallback`, `setAfterRenderCallback`): continuam a pintar todas as frames ate a aplicacao ligar `setIdleSkipWithCallbacks(true)` (nao se pode saber se mudam o que se mostra).
- **Memoria de base em GL: heap de 46,0 MB contra 41,9 do ImGui** (PSS 56,8 contra 50,2 no `bench_compare`; 48,9 contra 46,1 na medida com salto): a meta de ficar abaixo nao se cumpriu; parte e do Mesa (2,8 MB no
  `driBindContext`), os 4 MB restantes de heap nao se analisaram. **Por software, abaixo do GTK so com o ciclo que salta frames** (21,9 contra 23; 24,9 no `bench_compare`). Texto do editor de 5 MB: 200 bytes por linha (FLTK 60, ImGui 88), alvo de 100.
- **O rasteriador por software rasteriza a janela toda numa frame que muda** (4.7): nao se fez o rectangulo sujo; as faixas cortaram o buffer, nao o trabalho.
- **Arranque (4.4) e binario (4.6)**: nao se mexeu.
- **Cena F a 1,198x do ImGui no mesmo aspecto** (por 0,2%): o texto (`addText`, `calcTextSize`, `findGlyph`) e a pintura dos cantos ainda levam 60% das instrucoes; `appendIndices` (os zeros de `resize`) 5% e fica ate o `ct`
  ter `resize_for_overwrite`.
- **O tema escuro**: o texto seleccionado de uma `ListBox`, `ListWidget` e `ComboBox` (e uns textos fracos da consola, da regua da linha de tempo e dos comentarios do codigo) fica abaixo de 4,5 de contraste no preset escuro
  (1.5): mexer-lhes mudava o que o escuro desenha. O `Switch`, os instrumentos de `AudioWidgets`, `AutomotiveWidgets`, os graficos, os gizmos, a `Viewport3D` e a `VideoCanvas` tem a sua propria paleta e nao seguem o tema por
  omissao (1.4).

## 4. Verificacao final

`tools/check.sh` (com `IGUI_CONTAINERS_DIR` e `IGUI_ZEN_PLATFORM_DIR` das rondas anteriores, `--jobs 3`, `--base origin/main`), na ponta do ramo `igui-round9` (o commit `a9ac906`; a seguir so mudaram estes documentos):

```
PASS nostd: nothing new with std:: or #include <...> in src, include and adapters since origin/main
PASS release: 100% tests passed, 0 tests failed out of 51
PASS asan: 100% tests passed, 0 tests failed out of 51
PASS hash: what the bench draws is what bench/retained/draw.hash says
----
check.sh: 4 stages, 0 failed
```

51 testes (os 48 de antes e `igui_retained_round9`, `igui_retained_drawlist` e `igui_retained_frames`), nenhum Skipped; Release 73 s, ASan+UBSan 713 s. Entretanto o `origin/main` do repositorio andou para `b25280e`
(o fim da parte 1, empurrado por quem coordena), por isso o `nostd` acima so olha para as linhas desta parte; contra `0d86927` (o inicio da ronda) correu-se so essa fase: `PASS nostd: nothing new with std:: or
#include <...> in src, include and adapters since 0d86927`. `tools/nostd.baseline` nao cresceu. O `bench/retained/draw.hash` mudou (2.1, com o motivo e as imagens) e a fase `hash` confirma o valor novo.

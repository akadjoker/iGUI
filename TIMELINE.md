# Timeline de joints

O widget retained `ig::retained::Timeline` combina árvore e pistas no mesmo
controlo, mantendo as linhas alinhadas ao recolher joints e ao fazer scroll.

```cpp
#include <igui/widgets/Timeline.hpp>
auto* timeline = parent->createChild<ig::retained::Timeline>();
timeline->setRect({0, 0, 800, 400});
timeline->setHeaderWidth(180);
timeline->setEdgePadding(20); // pixels antes do início e depois do fim
timeline->setTimeRange(0, 10);
int hips = timeline->addJoint("Hips");
int spine = timeline->addJoint("Spine", hips);
timeline->addKeyframe(hips, 0);
timeline->addKeyframe(hips, 10);
timeline->addKeyframe(spine, 5);
timeline->onKeyframeMoved.connect([](int joint, int key, float seconds) {
    // Atualizar a animação da aplicação.
});
```

- Clique no cabeçalho de um joint expande/recolhe os descendentes.
- Roda sobre a árvore desloca ambas as colunas verticalmente.
- Duplo clique numa pista vazia adiciona uma key (`onKeyframeAdded`).
- Arrasto de uma key altera o tempo dentro do intervalo visível.
- Clique/arrasto na régua move o playhead; roda nas pistas faz zoom;
  botão do meio arrasta a vista horizontalmente.
- `track(id).locked = true` impede editar keys e clips dessa pista.

A margem mínima é 8 pixels; por omissão são 16. Aplica-se à conversão nos
dois sentidos, para desenho e hit-test coincidirem nos extremos.
IDs de joints e keys são índices: remover pistas ou inserir/remover keys
pode alterar os índices seguintes. O widget edita tempos e não calcula poses
ou interpolação de joints. Exemplo na stage `timeline` da demo SDL2 retained.

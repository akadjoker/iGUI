# Definir linguagens no editor

O `ig::retained::CodeEditor` aceita definições copiadas, sem precisar de criar
uma subclasse:

```cpp
#include <igui/widgets/CodeEditor.hpp>
using namespace ig::retained;

SyntaxLanguage language;
language.name = "SceneScript";
language.extensions = {"scene"}; // sem ponto, comparação sem distinguir maiúsculas
language.keywords = {"entity", "component", "import"};
language.types = {"vec3", "color"};
language.constants = {"true", "false"};
language.lineComment = "//";
language.blockCommentStart = "/*";
language.blockCommentEnd = "*/";
language.quotes = "\"'";

CodeEditor editor;
editor.setLanguage(language);
// Ou registar para deteção automática, na thread da interface:
CodeEditor::registerLanguage(language);
editor.setHighlighterForFile("level.scene");
editor.highlighter()->setColor(SyntaxHighlighter::TokenType::Keyword,
                              Color(220, 140, 240, 255));
```

As definições mais recentemente adicionadas têm prioridade sobre extensões
já conhecidas. Registar o mesmo nome substitui a definição; os editores já
abertos mantêm a sua cópia até se voltar a selecionar a linguagem.
Ficheiros sem uma extensão conhecida passam a texto simples.

Além de C/C++, Python, JavaScript/TypeScript, Lua e GLSL, existem definições
para JSON, JSONC, SQL e Go. SQL ignora maiúsculas nas palavras-chave; Go
reconhece strings raw multiline com backticks.

Este lexer é lexical, não um parser ou validador da linguagem. Suporta
comentários de bloco multiline, strings com escapes, identificadores,
números, operadores e chamadas. Não interpreta comentários aninhados,
interpolação ou gramáticas contextuais. `foldDelta` é calculado por linha
e não recebe o estado anterior. Para essas necessidades, continua disponível
a API virtual `SyntaxHighlighter` e `setHighlighter<T>()`.

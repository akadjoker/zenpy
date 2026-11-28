# Tarefa: atribuição múltipla a atributos e índices

`self.a, self.b = f()` não compila: `Error at 'self': Expected variable name in multi-assign`. Em Python os alvos de
uma atribuição múltipla podem ser nomes, atributos (`obj.x`), índices (`lista[i]`, `d["k"]`) e, aninhados, listas de
alvos (`(a, obj.b), c = ...`). Encontrado num jogo do Kinetix2D, onde é a forma natural de guardar dois valores
devolvidos por uma função do motor (`self.x, self.y = node.get_position()`).

## O que fazer
- O compilador aceita como alvo de `a, b = ...` qualquer expressão que possa estar à esquerda de `=` sozinha (nome,
  atributo, índice), e listas desses alvos aninhadas. A ordem de avaliação é a do CPython: primeiro o lado direito,
  depois as atribuições da esquerda para a direita.
- Também em `for a.x, b[i] in ...` se o `for` com vários nomes já existir para nomes simples.
- Testes em `tests/` (e no `tools/diff_cpython.py` se couber): atributos, índices de lista e de dict, mistura com
  nomes, aninhados, e a mensagem de erro quando o número de valores não bate.
- `CHANGELOG.md`.

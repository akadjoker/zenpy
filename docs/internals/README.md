# Zen VM — internals

Para quem tem de perceber a VM, não só usá-la. Uma página por subsistema,
curta, com os nomes reais do código para se poder saltar para lá. A ordem
de leitura é a ordem da lista.

1. [dispatch-and-frames.md](dispatch-and-frames.md) — o ciclo de execução, registos, frames, chamada e retorno.
2. [calling-convention.md](calling-convention.md) — como uma chamada é codificada, os flags do contador de argumentos, os fast paths e o que acontece quando falham.
3. [natives-and-embedding.md](natives-and-embedding.md) — funções nativas, ClassBuilder, `zen_bind.hpp`, `native_data`, kwargs, GC durante um nativo, módulos e plugins.
4. [compiler-conditions-and-peepholes.md](compiler-conditions-and-peepholes.md) — o que o compilador funde e por que regras; onde cada optimização vive.
5. [gc-and-roots.md](gc-and-roots.md) — o que o GC vê, o que não vê, e as regras que o código C++ tem de cumprir.
6. [testing.md](testing.md) — que testes existem, o que cada um cobre, e como correr cada modo.

Regra de manutenção: quando uma mudança de código altera algo dito aqui, a
página muda no mesmo commit. Se uma frase destas páginas parecer errada ao
ler o código, é a página que está errada — corrigir.

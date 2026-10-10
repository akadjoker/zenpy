# Nativos e embedding

Ficheiros: `vm.h` (`def_native`, `ClassBuilder`, `call_fn`, `kwargs()`),
`module.h` (`NativeReg`, `NativeLib`, flags), `object.h`
(`zen_instance_data`), `builtin_*.cpp` (exemplos).

## Uma função nativa

```cpp
int fn(VM *vm, Value *args, int nargs);   // resultado em args[0]; devolve quantos (0, 1); -1 = erro
```

- `args` aponta para `R[base+1]` do caller: os argumentos, no sítio.
- Num método de `ClassBuilder`, `args[-1]` é `self`.
- Erro: `vm->runtime_error("...")` e `return -1`.
- Registo: `vm.def_native("nome", fn, arity, flags)` (global) ou numa
  `NativeLib` (`import nome`). `arity = -1` é variádico.

## GC durante um nativo

Por defeito o GC está **pausado** enquanto um nativo corre: podes alocar
vários objectos e guardá-los em locais C++ sem os enraizar. Um nativo com
`ZEN_NATIVE_GC_SAFE` corre com o GC vivo e promete enraizar tudo o que
guarda entre alocações (ver gc-and-roots.md). Marca como GC_SAFE só o que
não aloca objectos Zen (leituras, `draw(x, y)`, `count(...)`): é o que a
engine chama em cada frame e poupa o par pause/resume.

`vm->call_fn(fn, args, n)` chama script a partir de um nativo (re-entrante:
`sorted(key=f)`, `map`, `filter` usam-no).

## Classes nativas: `ClassBuilder`

```cpp
vm.def_class("Texture")
  .ctor(tex_ctor)                    // void *(*)(VM*, int argc, Value *args) → native_data
  .dtor(tex_dtor)                    // chamado quando o GC recolhe a instância
  .method("draw", tex_draw, 2, ZEN_NATIVE_GC_SAFE)
  .method("width", tex_width, 0)
  .end();
// no método: TexData *t = zen_instance_data<TexData>(args[-1]);
```

O método fica num slot da vtable como qualquer método de script, por isso
`tex.draw(x, y)` com `tex: Texture` conhecido é um `INVOKE_VT` que chama o
`NativeFn` directamente. Medido no texmark: um objecto nativo por sprite
custa o mesmo que o host desenhar directamente.

`persistent(true)`: instâncias não geridas pelo GC (o host destrói).
`constructable(false)`: o script não pode chamar `Cls(...)`.
`field(name)`: campo normal (em `inst->fields`), visível ao script.

## zen_bind.hpp — bindings a partir da assinatura C++

`libzen/include/zen/zen_bind.hpp` (header-only, C++17, sem STL) gera o
`NativeFn` a partir da assinatura: conversão dos argumentos, erro de tipo com
o nome da função e a posição do argumento, aridade tirada da assinatura,
conversão do resultado. A API crua (`def_native`, `ClassBuilder`) continua a
existir e é a que o header usa por baixo.

```cpp
#include "zen_bind.hpp"
using namespace zen;

static double host_rand(double lo, double hi);
bind::def_fn<&host_rand>(vm, "rand", ZEN_NATIVE_GC_SAFE);   /* aridade 2 */

struct Texture { int id; int hits; void draw(double x, double y); int width() const; ~Texture(); };
static Texture *tex_load(VM *vm, const char *path);         /* nullptr = erro já reportado */

bind::def_class<Texture>(vm, "Texture")
    .ctor<&tex_load>()                 /* ou .ctor<int64_t, double>() → new T(...) */
    .dtor()                            /* delete (T *) quando a instância morre */
    .method<&Texture::draw>("draw", ZEN_NATIVE_GC_SAFE)
    .method<&Texture::width>("width")
    .property<&Texture::hits>("hits")  /* hits() e set_hits(v) */
    .end();
```

Tipos aceites nos parâmetros: `bool` (truthiness do Python), qualquer inteiro
(um int do script fora do intervalo do tipo C++ é erro, nunca truncamento),
enums (como int), `float`/`double` (int é aceite), `const char *`, `Value`
(passa tudo), e `T *` / `T &` para uma classe registada com `def_class<T>`
(vem de `native_data`). Um primeiro parâmetro `VM *` recebe a VM e não conta
como argumento do script. Resultados: `void` (→ nil), os mesmos escalares,
`const char *` (copiado para uma ObjString), `Value`, e `T *` / `T &` de uma
classe registada (embrulhado, `nullptr` → `None`).

**Verificação de classe.** `ClassDef<T>::end()` guarda o `ObjClass *` em
`ClassOf<T>::klass`. Um argumento `T *` / `T &` só passa se a instância for
dessa classe ou tiver-a na cadeia de `parent` (uma subclasse de script serve),
e tiver `native_data`; senão é o erro `f: argument 2 must be Texture, got int`
e a função C++ nem corre. `None` também é rejeitado: quem aceita um objecto
opcional recebe um `Value`. O registo é um slot por tipo C++, não por VM: com
várias VMs vivas ganha a que fez `end()` por último.

**`wrap<T>(vm, ptr)`: objectos do host.** Devolve uma instância de script da
classe de `T` que guarda `ptr` em `native_data` e que o GC **nunca apaga**
(`nullptr` → `None`). Os métodos que devolvem `T *` / `T &` de uma classe
registada fazem isto automaticamente. Constrói com `new_instance`, não com
`make_instance` (que correria o ctor nativo com zero argumentos). Numa classe
sem `.dtor()` não há nada a apagar e a instância é da própria classe; numa
classe com `.dtor()` a instância é de uma subclasse escondida `Nome$ref`
(criada em `end()`, `constructable(false)`, destrutor vazio): o GC sobe a
cadeia de `parent` só até encontrar um destrutor, e o vazio pára-o antes do
verdadeiro. Regra: `.dtor()` = a classe é dona das instâncias que o script
cria; `wrap` = o host é dono.

**`getter` / `setter` / `property`.** `.getter<&T::x>("x")` regista `x()`,
`.setter<&T::x>("x")` regista `set_x(v)` e `.property<&T::x>("x")` os dois. O
setter converte e verifica como qualquer argumento (intervalo dos inteiros,
tipo). Um membro `const char *` só pode ter getter (a string do script não
pertence ao objecto).

Uma função livre cujo primeiro parâmetro é `T *`/`T &` pode ser registada como
método: o receptor entra nesse parâmetro e a aridade visível ao script desce
um (`.method<&gauge_level>("level")` com `double gauge_level(Gauge &)`).

Erros são automáticos, nomeiam a função, o argumento e o tipo recebido:
`twice: argument 1 must be an int in range, got string`,
`small: expected 2 argument(s), got 1`. Um `ctor<A...>()` com `VM *` em
primeiro lugar passa a VM a `new T(vm, ...)`.
O que a assinatura não cobrir (varargs, kwargs, tuplos de retorno) continua a
escrever-se como `NativeFn` à mão. Cobertura: teste 18 de
`tests/cpp/test_embedding.cpp`; usos reais em
`tests/manual/texmark_raylib/tex_zen.cpp` e `tests/manual/glue_bench/glue_zen.cpp`.

## Keyword arguments num nativo

`sorted(xs, key=f, reverse=True)`: o compilador põe `{"key": f,
"reverse": True}` num mapa no último slot e liga o flag 0x40. Durante a
chamada, `vm->kwargs()` devolve esse `ObjMap*` (ou nullptr). Ver
`kwarg_get()` em builtin_base.cpp. O mapa vive num registo do caller: está
enraizado sem fazer nada.

## Chamar script a partir do C++

- `vm.run(fn)` corre uma função compilada (top-level); pode ser chamado de
  dentro de um nativo (`test_run_from_inside_script`).
- `vm.call_global("nome", args, n)` para os entry points do host
  (`add_sprites(n, x, y)`, `update_all(dt)`); o modelo mínimo está em
  `tests/manual/bunnymark_raylib/bunny_zen.cpp`.
- `vm.invoke(instance, "metodo", args, n)` ou por slot.

## Módulos e plugins

- Compilado no binário: `NativeLib` + `vm.register_lib(&lib)` → `import x`.
  É o caminho da engine e o único que existe em web/Android.
- Plugin `.so/.dll`: exporta `zen_open_<nome>()` (`zen_plugin.h`); o
  `import` faz dlopen quando não encontra o módulo registado
  (`try_load_plugin`). Sem testes ainda.
- Script: `import foo` procura `foo.py` nos caminhos `-I`, executa uma vez,
  guarda o dict de globais. Sem pacotes (`a.b`) nem imports relativos.

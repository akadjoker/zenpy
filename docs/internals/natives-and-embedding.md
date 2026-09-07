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

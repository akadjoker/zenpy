# Dispatch e frames

Ficheiros: `libzen/src/vm_dispatch.cpp` (`VM::execute`), `libzen/include/zen/vm.h`
(`CallFrame`), `libzen/include/zen/object.h` (`ObjFiber`).

## O ciclo

`VM::execute(ObjFiber *fiber)` é um loop sobre instruções de 32 bits
(`[op:8 | A:8 | B:8 | C:8]`, ou `A:8 | Bx:16`). Cinco locais em registos da
CPU são o estado quente:

| local | o que é |
|---|---|
| `frame` | `&fiber->frames[frame_count - 1]`, o frame em execução |
| `ip`    | ponteiro para a instrução actual (dentro de `frame->func->code`) |
| `R`     | `frame->base`: a janela de registos deste frame, `R[0..num_regs)` |
| `K`     | `frame->constants`: a pool de constantes da função |
| `UV`    | `frame->upvalues`: os upvalues da closure |

O dispatch é computed goto (`CASE(op)` = label, `DISPATCH()` = `goto
*table[op]`) sob GCC/Clang; com `-DZEN_DISPATCH_MODE=1` o mesmo código
compila como `switch`, que é o caminho do MSVC. `NEXT()` avança `ip` uma
palavra e faz `DISPATCH()`; instruções de várias palavras avançam `ip` elas
próprias antes.

## A stack e os frames

Cada `ObjFiber` tem uma stack de `Value` (`stack .. stack_end`) e um array de
`CallFrame` (`frames`, até `kMaxFrames`). Um frame não copia nada: a sua
janela `base` aponta para dentro da stack do fiber, logo a seguir aos
registos do caller. Numa chamada `R[a](R[a+1], R[a+2])`, o callee tem
`base = &R[a+1]`: os argumentos já são os seus primeiros registos.

```
CallFrame
  closure, func        a função (func == closure->func)
  constants, upvalues  cópias de func->constants / closure->upvalues
  ip                   onde retomar (guardado por SAVE_IP() antes de sair do loop)
  base                 R deste frame
  ret_reg, ret_count   onde o caller quer o(s) resultado(s), e quantos
```

`constants` e `upvalues` estão duplicados no frame de propósito: no retorno,
`LOAD_STATE_FROM(caller_frame)` lê tudo do frame com loads independentes,
sem passar por `frame->func->constants` (dois loads dependentes).

## Entrar num frame: `ENTER_FRAME`

Depois de escrever o frame novo, o estado local passa a ser o do callee.
Havia aqui um custo escondido: `LOAD_STATE()` relia `fiber->frame_count`
(acabado de escrever) e depois cada campo do frame (acabados de escrever),
uma cadeia store→load em cada chamada. `ENTER_FRAME(nf, fn, cl, base)` põe
`frame/ip/R/K/UV` a partir dos valores que ainda estão em registos da CPU.
Medido: fib 0,162→0,134 s só com isto.

`clear_new_regs(base, nargs, num_regs)` põe a nil os registos do callee que
os argumentos não preencheram: o GC percorre `[stack, stack_top)` e não pode
ver lixo. `CHECK_STACK_SPACE` garante que `base + num_regs <= stack_end`.

## Sair: `OP_RETURN`, `OP_RETURNNIL`

Fast path (o caso comum): há um caller, ele quer exactamente um resultado,
a instrução devolve um, e nenhum nativo está à espera num "stop depth"
(`external_call_stop_depth_`, usado por `call_fn` quando C++ chama script).
Então: `frame_count--`, `caller->base[ret_reg] = R[a]`, `stack_top` volta ao
topo do caller, `LOAD_STATE_FROM(caller)`. O caminho geral trata de vários
resultados, de preencher com nil os que faltam e de devolver ao C++.

`OP_RETURNNIL` é `LOADNIL R0; RETURN R0` numa só instrução (return vazio e
fim de função). Tem o seu fast path; o caminho lento é o do RETURN.

Os registos do callee que ficam acima do caller não são limpos no retorno:
foram vistos vivos pelo GC durante a chamada, logo nunca são ponteiros
pendurados; o próximo frame limpa-os à entrada (`clear_new_regs`).

## Números que interessam

Um dispatch custa ~15 ciclos; uma chamada + retorno de script ~12,6 ns
(Lua ~9,8). Um MOVE independente entre instruções custa quase nada em tempo
de parede, porque o CPU out-of-order o esconde atrás da cadeia call/return;
o que conta é encurtar cadeias de dependência, não contar dispatches
(lição de 2026-09-07, ver bugs.md ronda 4).

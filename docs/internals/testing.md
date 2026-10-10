# Testes: o que existe e o que cobre

| Comando | Cobre |
|---|---|
| `./run_tests.sh` | `tests/[0-9]*.py` (69 scripts; asserts, saem 0), `tests/errors/*.py` (mensagens de erro esperadas), `tests/cpp/*` (embedding em C++) |
| `./run_tests.sh --stress-gc` | o mesmo com recolha em cada alocação, Debug + ASan/UBSan |
| `./run_tests.sh --switch-dispatch` | o mesmo com o dispatch por `switch` (caminho do MSVC), que sob gcc nunca corre de outra forma |
| `./run_tests.sh --bytecode` | cada script também via `--dump x.zbc` + `zen x.zbc`: a saída tem de ser igual (serialização de todos os opcodes) |
| `tools/diff_cpython.py` | `tests/diff/*.py` chunk a chunk em CPython e Zen; o que difere é decisão ou bug (ver bugs.md ronda 5) |
| `tests/65_python_semantics.py` | corre igual em CPython e Zen (`tools/diff_cpython.py --whole`) |
| `tools/speed_report.sh <label>` | relatório completo de velocidade em `tests/manual/baseline/` |
| `tools/speed_gate.sh <baseline.md> [%]` | falha se algum benchmark regredir mais do que o limiar face à baseline |
| `tests/manual/*` | bunnymark/texmark/quadtree raylib, algo_bench, glue_bench, microbench: medições, não asserts |
| `tests/fuzz` | harness libFuzzer (clang) |

## Como acrescentar

- Um comportamento de linguagem: `tests/NN_nome.py` com asserts e `print("ok")`.
  Se o Python faz o mesmo, valida-o com `tools/diff_cpython.py --whole`.
- Um erro esperado: `tests/errors/nome.py` cuja primeira linha é
  `# expect: <substring da mensagem>`; o runner exige exit != 0 e a
  substring no stderr.
- Um opcode novo: `--bytecode` apanha a serialização; `--switch-dispatch`
  apanha a tabela do switch; `debug.cpp` (nomes + disassembler) e
  `opcodes.h` têm de mudar juntos.
- Um nativo/ClassBuilder novo: um caso em `tests/cpp/test_embedding.cpp`.

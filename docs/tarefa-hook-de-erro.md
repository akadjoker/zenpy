# Tarefa: hook de erro estruturado no VM

Para os dois motores que embebem o zenpy (Kinetix2D e prisma3d). Uma só implementação aqui; cada motor passa a
usá-la depois, por bump do submódulo.

## Porquê
Os dois motores têm um ecrã de erro de script (Kinetix2D: PR #35, `ScriptPanic`; prisma3d: tarefa E05). Hoje sabem
do erro lendo o texto que o VM escreve em `print_err`: a linha `"[zen runtime error] <msg>"` e as linhas
`"  File \"<src>\", line N, in <func>"` (`VM::runtime_error`, `libzen/src/vm.cpp:1889`), contadas com
`debug_frame_count()` para saber quando acabou. Os erros de compilação vêm como `"[line N] Error at ..."`
(`Compiler::error_at`, `libzen/src/compiler.cpp:286`). Funciona, mas qualquer mudança no texto parte os motores, e a
mensagem é cortada a 512 bytes e cada linha do traceback a 256.

## O que fazer
1. Em `vm.h`, uma estrutura com o erro e um hook opcional ao lado dos callbacks:

       struct ErrorFrame { const char *source; int line; const char *function; };
       struct ErrorInfo
       {
           const char *message;          /* sem o prefixo "[zen runtime error] " */
           bool compile;                 /* erro de compilação */
           const char *source;           /* ficheiro do erro (compilação: o que se compilava) */
           int line;                     /* linha do frame mais interior, ou da compilação */
           const ErrorFrame *frames;     /* o mais interior primeiro */
           int frame_count;
       };
       typedef void (*ErrorHook)(const ErrorInfo &info, void *userdata);
       void set_error_hook(ErrorHook hook, void *userdata);

2. `VM::runtime_error` chama o hook **antes** de desenrolar, enquanto os frames existem (um debugger pode ainda
   inspecionar), com os mesmos dados que hoje escreve. O texto em `print_err` continua igual, para quem só lê texto.
3. `Compiler::error_at` (e os erros de carregamento de bytecode) chamam o hook com `compile = true`, a linha e o
   ficheiro que se compilava, sem frames.
4. Sem alocação quando não há hook; com hook, os arrays de frames vivem num buffer do VM reutilizado, válidos só
   durante a chamada.
5. Um teste em `tests/` (no estilo dos que já existem) com um erro num método chamado por outro, um erro de compilação
   e um erro dentro de `invoke`: o hook recebe a mensagem, o ficheiro, a linha e os frames certos, uma vez por erro.
6. `CHANGELOG.md` e a secção de embedding do `README.md`.
7. (Pedido do prisma3d) Os jogos exportados correm **bytecode** compilado, não fonte. O hook tem de dar o ficheiro, a
   linha e as funções dos frames também para código carregado de bytecode. Se a tabela de linhas ou os nomes não forem
   guardados no bytecode, guardá-los e subir a versão do formato. O teste do ponto 5 repete o erro em `invoke` com o
   módulo carregado de bytecode.

## Depois
Kinetix2D: o `captureZenError` de `scripting/src/ZenScriptComponent.cpp` passa a usar o hook e deixa de ler texto.
prisma3d: o E05 usa o hook desde o início se já estiver feito.

# M1 - Cliente/Servidor (versao simples, Windows/Code::Blocks)

Dois programas separados (servidor.exe e cliente.exe) que conversam por
memoria compartilhada do Windows. O servidor tem 3 threads que ficam
olhando se chegou pedido; quando chega, uma delas pega, mexe na "tabela"
(vetor + bancoTXT.txt) usando um mutex, e devolve a resposta.

## Arquivos

- `registro.h` - o struct do registro (id + nome)
- `memoria.h` - o struct que fica na memoria compartilhada (pedidos)
- `servidor.cpp` - cria a memoria compartilhada, o mutex e as threads
- `cliente.cpp` - manda pedidos (INSERT/SELECT/UPDATE/DELETE)

## Como rodar

1. Abra `m1.workspace` no Code::Blocks (abre os dois projetos juntos).
2. Compile e rode o projeto **servidor** primeiro (Ctrl+F9, Ctrl+F10).
3. Com o servidor rodando, compile e rode o **cliente** em outra janela.
4. Usa o menu do cliente pra fazer INSERT/SELECT/UPDATE/DELETE.

Da pra rodar o cliente.exe varias vezes ao mesmo tempo (abrindo ele de
novo, ou usando `cliente.exe INSERT 1 Joao` pelo cmd) pra ver as threads
do servidor atendendo mais de um pedido.

`teste_concorrencia.bat` faz isso automatico (depois de compilar os dois
projetos pelo menos uma vez).

Pra fechar o servidor eh so fechar a janela dele.

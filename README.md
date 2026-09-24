# m1-sistemas-operacionais
trabalho da matéria sistemas operacionais, que aborda conceitos como IPC, threads, concorrência e paralelismo na linguagem c++

# como funciona?
dois programas separados que conversam por memoria compartilhada do Windows. O servidor tem 3 threads que ficam
olhando se chegou pedido; quando chega, uma delas pega, mexe na "tabela" usando um mutex, e devolve a resposta.

## arquivos do projeto

- `registro.h`
- `memoria.h`
- `servidor.cpp`
- `cliente.cpp`

## como rodar

o projeto foi feito na IDE CodeBlocks, então primeiro abra o arquivo `m1.workspace`

1. abra `m1.workspace` no Code::Blocks.
2. compile e rode o projeto **servidor** primeiro.
3. com o servidor rodando, compile e rode o **cliente** em outra janela (se possível, vá até a paste /bin/Debug/cliente.exe).
4. use o menu do cliente pra fazer INSERT/SELECT/UPDATE/DELETE.

da pra rodar o cliente.exe varias vezes ao mesmo tempo  pra ver as threads do servidor atendendo mais de um pedido.

pra fechar o servidor é so fechar a janela dele.

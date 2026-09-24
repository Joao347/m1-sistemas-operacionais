//memoria compartilhada
#include "registro.h"

#define MAX_PEDIDOS 5   //quantos pedidos podem esperar ao mesmo tempo
#define TAM_OP 10
#define TAM_MSG 100

//nome da memoria compartilhada e do mutex (tem que ser igual nos dois programas)
#define NOME_MEMORIA "MinhaMemoriaCompartilhada"
#define NOME_MUTEX "MeuMutexDoBanco"

//pedido que o cliente manda pro servidor
struct Pedido {
    int ocupado;          // 1 = slot em uso por algum cliente
    int pronto;           // 1 = servidor ja respondeu
    char operacao[TAM_OP]; // INSERT, SELECT, UPDATE ou DELETE
    int id;
    char nome[TAM_NOME];
    int sucesso;
    char resposta[TAM_MSG];
};

//a memoria compartilhada e só um vetor de pedidos
struct Memoria {
    Pedido pedidos[MAX_PEDIDOS];
};

//memoria compartilhada
#include "registro.h"

#define MAX_PEDIDOS 5   //quantos pedidos podem esperar ao mesmo tempo
#define TAM_OP 10
#define TAM_MSG 100

//nome da memoria compartilhada e do mutex
#define NOME_MEMORIA "MinhaMemoriaCompartilhada"
#define NOME_MUTEX "MeuMutexDoBanco"
#define NOME_SEM_PEDIDOS "SemPedidosPendentes" //conta pedidos esperando atendimento
#define NOME_SEM_SLOTS "SemSlotsLivres" //conta slots livres
#define PREFIXO_EVENTO "EventoResposta" //um evento por slot: EventoResposta0, EventoResposta1...

//estados de um pedido
#define PEDIDO_AGUARDANDO 0 //cliente escreveu, nenhuma thread pegou ainda
#define PEDIDO_EM_ATENDIMENTO 1 //uma thread do servidor esta processando
#define PEDIDO_PRONTO 2 //servidor respondeu, cliente pode ler

//pedido que o cliente manda pro servidor
struct Pedido {
    int ocupado; //1 = slot em uso por algum cliente
    int estado; //PEDIDO_AGUARDANDO, PEDIDO_EM_ATENDIMENTO ou PEDIDO_PRONTO
    char operacao[TAM_OP]; //INSERT, SELECT, UPDATE ou DELETE
    int id;
    char nome[TAM_NOME];
    int sucesso;
    char resposta[TAM_MSG];
};

//a memoria compartilhada e só um vetor de pedidos
struct Memoria {
    Pedido pedidos[MAX_PEDIDOS];
};

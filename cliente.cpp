//arquivo cliente: manda os pedidos pro servidor usando a memoria compartilhada
#include <windows.h> //biblioteca "windows.h" equivalente ao pthreads, só que para usar no windows
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "memoria.h"

HANDLE hMutex;                // protege os slots da memoria compartilhada
HANDLE hSemPedidos;           // avisa o servidor que tem pedido novo
HANDLE hSemSlots;             // espera ter slot livre
HANDLE hEventos[MAX_PEDIDOS]; // espera a resposta do slot
Memoria *mem;

// pega um slot livre, escreve o pedido, avisa o servidor e dorme ate a resposta chegar
void enviarPedido(const char *operacao, int id, const char *nome, int *sucesso, char *resposta) {
    // se os slots estiverem todos ocupados, o cliente dorme aqui ate algum liberar
    WaitForSingleObject(hSemSlots, INFINITE);

    WaitForSingleObject(hMutex, INFINITE);
    int idx = -1;
    for (int i = 0; i < MAX_PEDIDOS; i++) {
        if (mem->pedidos[i].ocupado == 0) {
            idx = i;
            mem->pedidos[i].ocupado = 1;
            mem->pedidos[i].estado = PEDIDO_AGUARDANDO;
            snprintf(mem->pedidos[i].operacao, TAM_OP, "%s", operacao);
            mem->pedidos[i].id = id;
            snprintf(mem->pedidos[i].nome, TAM_NOME, "%s", nome);
            ResetEvent(hEventos[i]); // garante que o evento do slot comece desligado
            break;
        }
    }
    ReleaseMutex(hMutex);

    if (idx == -1) { // nao deveria acontecer, o semaforo garante que tem slot livre
        ReleaseSemaphore(hSemSlots, 1, NULL);
        *sucesso = 0;
        strcpy(resposta, "nenhum slot livre");
        return;
    }

    ReleaseSemaphore(hSemPedidos, 1, NULL); // acorda uma thread do servidor

    // dorme no evento do slot ate o servidor responder
    while (1) {
        WaitForSingleObject(hEventos[idx], INFINITE);
        WaitForSingleObject(hMutex, INFINITE);
        if (mem->pedidos[idx].estado == PEDIDO_PRONTO) {
            *sucesso = mem->pedidos[idx].sucesso;
            strcpy(resposta, mem->pedidos[idx].resposta);
            mem->pedidos[idx].ocupado = 0; // libera o slot
            ReleaseMutex(hMutex);
            break;
        }
        ReleaseMutex(hMutex);
    }

    ReleaseSemaphore(hSemSlots, 1, NULL); // avisa que tem um slot livre de novo
}

//comando pra poder só digitar inteiros
int lerInteiro(const char *mensagem, int *valor) {
    printf("%s", mensagem);
    if (scanf("%d", valor) == 1) return 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    if (c == EOF) exit(0);
    printf("digite apenas numeros\n");
    return 0;
}

int main(int argc, char *argv[]) {
    //tenta abrir a memoria compartilhada que o servidor criou
    HANDLE hMapa = NULL;
    for (int tentativa = 0; tentativa < 40; tentativa++) {
        hMapa = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, NOME_MEMORIA);
        if (hMapa != NULL) break;
        if (tentativa == 0) printf("esperando o servidor iniciar...\n");
        Sleep(250);
    }
    if (hMapa == NULL) {
        printf("servidor nao localizado\n");
        return 1;
    }

    mem = (Memoria*) MapViewOfFile(hMapa, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Memoria));
    hMutex      = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, NOME_MUTEX);
    hSemPedidos = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, NOME_SEM_PEDIDOS);
    hSemSlots   = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, NOME_SEM_SLOTS);
    int ok = (mem != NULL && hMutex != NULL && hSemPedidos != NULL && hSemSlots != NULL);
    for (int i = 0; i < MAX_PEDIDOS && ok; i++) {
        char nome[40];
        sprintf(nome, "%s%d", PREFIXO_EVENTO, i);
        hEventos[i] = OpenEventA(EVENT_ALL_ACCESS, FALSE, nome);
        if (hEventos[i] == NULL) ok = 0;
    }
    if (!ok) {
        printf("erro ao conectar nos objetos do servidor (codigo %lu)\n", GetLastError());
        return 1;
    }

    printf("conectado ao servidor!\n");

    int sucesso;
    char resposta[TAM_MSG];

    if (argc >= 3) {
        char op[TAM_OP];
        snprintf(op, TAM_OP, "%s", argv[1]);
        for (int i = 0; op[i]; i++) op[i] = toupper((unsigned char) op[i]);
        int id = atoi(argv[2]);
        char nome[TAM_NOME] = "";
        if (argc >= 4) snprintf(nome, TAM_NOME, "%s", argv[3]);

        enviarPedido(op, id, nome, &sucesso, resposta);
        printf("%s: %s\n", sucesso ? "OK" : "ERRO", resposta);
        return 0;
    }

    //menu normal
    int opcao;
    do {
        printf("\n1-INSERT  2-SELECT  3-UPDATE  4-DELETE  0-Sair\n");
        if (!lerInteiro("opcao: ", &opcao)) continue;
        if (opcao == 0) break;

        int id;
        char nome[TAM_NOME] = "";

        if (opcao < 1 || opcao > 4) {
            printf("opcao invalida\n");
            continue;
        }
        if (!lerInteiro("id: ", &id)) continue;

        if (opcao == 1) {
            printf("nome: "); scanf("%49s", nome); //como definimos o numero max pra 50, o %49s serve pra n deixar passar de 49
            enviarPedido("INSERT", id, nome, &sucesso, resposta);
        } else if (opcao == 2) {
            enviarPedido("SELECT", id, nome, &sucesso, resposta);
        } else if (opcao == 3) {
            printf("novo nome: "); scanf("%49s", nome);
            enviarPedido("UPDATE", id, nome, &sucesso, resposta);
        } else {
            enviarPedido("DELETE", id, nome, &sucesso, resposta);
        }

        printf("%s: %s\n", sucesso ? "OK" : "ERRO", resposta);
    } while (opcao != 0);

    return 0;
}

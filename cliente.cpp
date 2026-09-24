//arquivo cliente: manda os pedidos pro servidor usando a memoria compartilhada
#include <windows.h> //biblioteca "windows.h" equivalente ao pthreads, só que para usar no windows
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "memoria.h"

HANDLE hMutex;
Memoria *mem;

//procura um slot livre, escreve o pedido, e espera a resposta chegar
void enviarPedido(const char *operacao, int id, const char *nome, int *sucesso, char *resposta) {
    int idx = -1;

    //procura um slot livre pra colocar o pedido
    while (idx == -1) {
        WaitForSingleObject(hMutex, INFINITE);
        for (int i = 0; i < MAX_PEDIDOS; i++) {
            if (mem->pedidos[i].ocupado == 0) {
                idx = i;
                mem->pedidos[i].ocupado = 1;
                mem->pedidos[i].pronto = 0;
                strcpy(mem->pedidos[i].operacao, operacao);
                mem->pedidos[i].id = id;
                strcpy(mem->pedidos[i].nome, nome);
                break;
            }
        }
        ReleaseMutex(hMutex);
        if (idx == -1) Sleep(50); // todos os slots ocupados, tenta de novo
    }

    //espera o servidor responder
    while (1) {
        WaitForSingleObject(hMutex, INFINITE);
        if (mem->pedidos[idx].pronto == 1) {
            *sucesso = mem->pedidos[idx].sucesso;
            strcpy(resposta, mem->pedidos[idx].resposta);
            mem->pedidos[idx].ocupado = 0; // libera o slot
            ReleaseMutex(hMutex);
            break;
        }
        ReleaseMutex(hMutex);
        Sleep(50);
    }
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
        printf("sevidor não localizado");
        return 1;
    }

    mem = (Memoria*) MapViewOfFile(hMapa, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Memoria));
    hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, NOME_MUTEX);

    printf("conectado ao servidor!\n");

    int sucesso;
    char resposta[TAM_MSG];

    //se rodar com argumentos: cliente.exe INSERT 1 Joao
    if (argc >= 3) {
        char op[TAM_OP];
        strcpy(op, argv[1]);
        for (int i = 0; op[i]; i++) op[i] = toupper(op[i]);
        int id = atoi(argv[2]);
        char nome[TAM_NOME] = "";
        if (argc >= 4) strcpy(nome, argv[3]);

        enviarPedido(op, id, nome, &sucesso, resposta);
        printf("%s: %s\n", sucesso ? "OK" : "ERRO", resposta);
        return 0;
    }

    //menu normal
    int opcao;
    do {
        printf("\n1-INSERT  2-SELECT  3-UPDATE  4-DELETE  0-Sair\n");
        printf("opcao: ");
        scanf("%d", &opcao);

        if (opcao == 0) break;

        int id;
        char nome[TAM_NOME] = "";

        if (opcao == 1) {
            printf("id: "); scanf("%d", &id);
            printf("nome: "); scanf("%s", nome);
            enviarPedido("INSERT", id, nome, &sucesso, resposta);
        } else if (opcao == 2) {
            printf("id: "); scanf("%d", &id);
            enviarPedido("SELECT", id, nome, &sucesso, resposta);
        } else if (opcao == 3) {
            printf("id: "); scanf("%d", &id);
            printf("novo nome: "); scanf("%s", nome);
            enviarPedido("UPDATE", id, nome, &sucesso, resposta);
        } else if (opcao == 4) {
            printf("id: "); scanf("%d", &id);
            enviarPedido("DELETE", id, nome, &sucesso, resposta);
        } else {
            printf("opcao invalida\n");
            continue;
        }

        printf("%s: %s\n", sucesso ? "OK" : "ERRO", resposta);

    } while (opcao != 0);

    return 0;
}

//arquivo servidor: cria a memoria compartilhada e fica com threads esperando pedido
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "memoria.h"

#define NUM_THREADS 3
#define MAX_REGISTROS 100

Registro tabela[MAX_REGISTROS]; //o bd fica so na memoria do servidor
int totalRegistros = 0;

HANDLE hMutex; //protege a memoria compartilhada e a tabela
Memoria *mem;

//le o arquivo texto pra dentro do vetor tabela
void carregarTabela() {
    FILE *f = fopen("bancoTXT.txt", "r");
    if (f == NULL) return;
    while (fscanf(f, "%d;%49[^\n]\n", &tabela[totalRegistros].id, tabela[totalRegistros].nome) == 2) {
        totalRegistros++;
    }
    fclose(f);
}

//grava o vetor tabela inteiro no arquivo texto
void salvarTabela() {
    FILE *f = fopen("bancoTXT.txt", "w");
    for (int i = 0; i < totalRegistros; i++) {
        fprintf(f, "%d;%s\n", tabela[i].id, tabela[i].nome);
    }
    fclose(f);
}

//acha a posicao de um id na tabela, ou -1 se nao achar
int acharId(int id) {
    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) return i;
    }
    return -1;
}

//faz a operacao pedida em cima da tabela
void processarPedido(Pedido *p) {
    int pos = acharId(p->id);

    if (strcmp(p->operacao, "INSERT") == 0) {
        if (pos != -1) {
            p->sucesso = 0;
            sprintf(p->resposta, "id %d ja existe", p->id);
        } else {
            tabela[totalRegistros].id = p->id;
            strcpy(tabela[totalRegistros].nome, p->nome);
            totalRegistros++;
            salvarTabela();
            p->sucesso = 1;
            sprintf(p->resposta, "inserido id %d nome %s", p->id, p->nome);
        }
    } else if (strcmp(p->operacao, "SELECT") == 0) {
        if (pos == -1) {
            p->sucesso = 0;
            sprintf(p->resposta, "id %d nao encontrado", p->id);
        } else {
            p->sucesso = 1;
            sprintf(p->resposta, "id %d nome %s", tabela[pos].id, tabela[pos].nome);
        }
    } else if (strcmp(p->operacao, "UPDATE") == 0) {
        if (pos == -1) {
            p->sucesso = 0;
            sprintf(p->resposta, "id %d nao encontrado", p->id);
        } else {
            strcpy(tabela[pos].nome, p->nome);
            salvarTabela();
            p->sucesso = 1;
            sprintf(p->resposta, "id %d atualizado pra %s", p->id, p->nome);
        }
    } else if (strcmp(p->operacao, "DELETE") == 0) {
        if (pos == -1) {
            p->sucesso = 0;
            sprintf(p->resposta, "id %d nao encontrado", p->id);
        } else {
            for (int i = pos; i < totalRegistros - 1; i++) {
                tabela[i] = tabela[i + 1];
            }
            totalRegistros--;
            salvarTabela();
            p->sucesso = 1;
            sprintf(p->resposta, "id %d removido", p->id);
        }
    } else {
        p->sucesso = 0;
        sprintf(p->resposta, "operacao invalida");
    }
}

//funcao que cada thread do pool fica rodando
DWORD WINAPI threadServidor(LPVOID numero) {
    int meuNumero = (int)(long long) numero;

    while (1) {
        WaitForSingleObject(hMutex, INFINITE); // pega o mutex

        for (int i = 0; i < MAX_PEDIDOS; i++) {
            if (mem->pedidos[i].ocupado == 1 && mem->pedidos[i].pronto == 0) {
                printf("thread %d atendendo pedido: %s id=%d\n", meuNumero, mem->pedidos[i].operacao, mem->pedidos[i].id);
                processarPedido(&mem->pedidos[i]);
                mem->pedidos[i].pronto = 1;
                break; // atende um pedido por vez
            }
        }

        ReleaseMutex(hMutex); // libera o mutex
        Sleep(100); // espera um pouco antes de olhar de novo
    }
    return 0;
}

int main() {
    printf("servidor iniciando...\n");

    carregarTabela();

    //cria a memoria compartilhada (IPC)
    HANDLE hMapa = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Memoria), NOME_MEMORIA);
    mem = (Memoria*) MapViewOfFile(hMapa, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Memoria));
    memset(mem, 0, sizeof(Memoria));

    //cria o mutex que protege a memoria compartilhada e a tabela
    hMutex = CreateMutexA(NULL, FALSE, NOME_MUTEX);

    //cria as threads do pool
    for (int i = 0; i < NUM_THREADS; i++) {
        CreateThread(NULL, 0, threadServidor, (LPVOID)(long long)(i + 1), 0, NULL); //createthread seria equivalente a phtreadcreate
    }

    printf("servidor pronto, %d threads esperando pedidos\n", NUM_THREADS);
    printf("nao feche essa janela enquanto estiver usando o cliente\n");

    while (1) {
        Sleep(1000); //so segura o programa aberto, quem trabalha sao as threads
    }

    return 0;
}

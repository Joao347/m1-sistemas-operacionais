//arquivo servidor: cria a memoria compartilhada e fica com threads esperando pedido
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "memoria.h"

#define NUM_THREADS 3
#define MAX_REGISTROS 100
#define TEMPO_PROCESSAMENTO_MS 500 // simula o tempo que um banco real leva pra executar a operacao

Registro tabela[MAX_REGISTROS]; // "banco de dados": vetor na memoria, gravado no bancoTXT.txt
int totalRegistros = 0;

HANDLE hMutex;                // protege os slots da memoria compartilhada (entre processos)
HANDLE hSemPedidos;           // conta pedidos pendentes: as threads dormem nele
HANDLE hSemSlots;             // conta slots livres: usado pelos clientes
HANDLE hEventos[MAX_PEDIDOS]; // um evento por slot: avisa o cliente que a resposta ficou pronta
// leitores/escritor (problema classico): varios SELECT juntos, escrita exclusiva
HANDLE hMutexLeitores;        // protege o contador de leitores
HANDLE hSemEscrita;           // semaforo binario: quem tiver ele tem acesso exclusivo a tabela
int leitoresAtivos = 0;       // quantas threads estao lendo a tabela agora
Memoria *mem;
DWORD inicio;                 // instante em que o servidor subiu, pra mostrar o tempo no log

// le o arquivo texto pra dentro do vetor tabela
void carregarTabela() {
    FILE *f = fopen("bancoTXT.txt", "r");
    if (f == NULL) return;
    while (totalRegistros < MAX_REGISTROS &&
           fscanf(f, "%d;%49[^\n]\n", &tabela[totalRegistros].id, tabela[totalRegistros].nome) == 2) {
        totalRegistros++;
    }
    fclose(f);
}

// grava o vetor tabela inteiro no arquivo texto
void salvarTabela() {
    FILE *f = fopen("bancoTXT.txt", "w");
    if (f == NULL) return;
    for (int i = 0; i < totalRegistros; i++) {
        fprintf(f, "%d;%s\n", tabela[i].id, tabela[i].nome);
    }
    fclose(f);
}

// acha a posicao de um id na tabela, ou -1 se nao achar
int acharId(int id) {
    for (int i = 0; i < totalRegistros; i++) {
        if (tabela[i].id == id) return i;
    }
    return -1;
}

// entrada/saida dos leitores: o primeiro leitor bloqueia os escritores, o ultimo libera
void iniciarLeitura() {
    WaitForSingleObject(hMutexLeitores, INFINITE);
    leitoresAtivos++;
    if (leitoresAtivos == 1) WaitForSingleObject(hSemEscrita, INFINITE);
    ReleaseMutex(hMutexLeitores);
}

void terminarLeitura() {
    WaitForSingleObject(hMutexLeitores, INFINITE);
    leitoresAtivos--;
    if (leitoresAtivos == 0) ReleaseSemaphore(hSemEscrita, 1, NULL);
    ReleaseMutex(hMutexLeitores);
}

// escritor: precisa do semaforo so pra ele
void iniciarEscrita()  { WaitForSingleObject(hSemEscrita, INFINITE); }
void terminarEscrita() { ReleaseSemaphore(hSemEscrita, 1, NULL); }

// faz a operacao pedida em cima da tabela.
// SELECT pega o lock compartilhado: varias threads podem ler ao mesmo tempo.
// INSERT/UPDATE/DELETE pegam o lock exclusivo: so uma thread escreve e ninguem le enquanto isso.
void processarPedido(Pedido *p, int numThread) {
    int leitura = (strcmp(p->operacao, "SELECT") == 0);

    if (leitura) iniciarLeitura();
    else         iniciarEscrita();

    printf("[%6lu ms] thread %d EXECUTA  %s id=%d (%s)\n", GetTickCount() - inicio, numThread,
           p->operacao, p->id, leitura ? "leitura compartilhada" : "escrita exclusiva");

    Sleep(TEMPO_PROCESSAMENTO_MS); // simula o trabalho do banco

    int pos = acharId(p->id);

    if (strcmp(p->operacao, "INSERT") == 0) {
        if (pos != -1) {
            p->sucesso = 0;
            sprintf(p->resposta, "id %d ja existe", p->id);
        } else if (totalRegistros >= MAX_REGISTROS) {
            p->sucesso = 0;
            sprintf(p->resposta, "banco cheio (maximo %d registros)", MAX_REGISTROS);
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

    if (leitura) terminarLeitura();
    else         terminarEscrita();
}

// funcao que cada thread do pool fica rodando
DWORD WINAPI threadServidor(LPVOID numero) {
    int meuNumero = (int)(INT_PTR) numero;

    while (1) {
        // dorme ate existir pelo menos um pedido pendente (sem polling, sem gastar CPU)
        WaitForSingleObject(hSemPedidos, INFINITE);

        // secao critica curta: so pra retirar o pedido do slot
        WaitForSingleObject(hMutex, INFINITE);
        int idx = -1;
        Pedido copia;
        for (int i = 0; i < MAX_PEDIDOS; i++) {
            if (mem->pedidos[i].ocupado == 1 && mem->pedidos[i].estado == PEDIDO_AGUARDANDO) {
                idx = i;
                mem->pedidos[i].estado = PEDIDO_EM_ATENDIMENTO; // nenhuma outra thread pega esse
                copia = mem->pedidos[i];
                break;
            }
        }
        ReleaseMutex(hMutex);

        if (idx == -1) continue;

        // processa FORA do mutex: as outras threads continuam trabalhando ao mesmo tempo
        printf("[%6lu ms] thread %d PEGOU    %s id=%d\n", GetTickCount() - inicio, meuNumero, copia.operacao, copia.id);
        processarPedido(&copia, meuNumero);
        printf("[%6lu ms] thread %d TERMINOU %s id=%d\n", GetTickCount() - inicio, meuNumero, copia.operacao, copia.id);

        // devolve a resposta no mesmo slot
        WaitForSingleObject(hMutex, INFINITE);
        mem->pedidos[idx].sucesso = copia.sucesso;
        strcpy(mem->pedidos[idx].resposta, copia.resposta);
        mem->pedidos[idx].estado = PEDIDO_PRONTO;
        ReleaseMutex(hMutex);

        SetEvent(hEventos[idx]); // acorda o cliente dono desse slot
    }
    return 0;
}

int main() {
    printf("servidor iniciando...\n");
    inicio = GetTickCount();

    carregarTabela();
    // objetos do leitores/escritor (so usados dentro do servidor, por isso sem nome)
    hMutexLeitores = CreateMutexA(NULL, FALSE, NULL);
    hSemEscrita    = CreateSemaphoreA(NULL, 1, 1, NULL);
    if (hMutexLeitores == NULL || hSemEscrita == NULL) {
        printf("erro ao criar os objetos de leitores/escritor (codigo %lu)\n", GetLastError());
        return 1;
    }

    // cria a memoria compartilhada (isso aqui eh o IPC)
    HANDLE hMapa = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Memoria), NOME_MEMORIA);
    if (hMapa == NULL) {
        printf("erro ao criar a memoria compartilhada (codigo %lu)\n", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("ja existe um servidor (ou cliente de uma execucao anterior) aberto. feche todos e tente de novo\n");
        return 1;
    }
    mem = (Memoria*) MapViewOfFile(hMapa, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Memoria));
    if (mem == NULL) {
        printf("erro ao mapear a memoria compartilhada (codigo %lu)\n", GetLastError());
        return 1;
    }
    memset(mem, 0, sizeof(Memoria));

    // objetos de sincronizacao entre processos
    hMutex      = CreateMutexA(NULL, FALSE, NOME_MUTEX);
    hSemPedidos = CreateSemaphoreA(NULL, 0, MAX_PEDIDOS, NOME_SEM_PEDIDOS);           // comeca sem pedidos
    hSemSlots   = CreateSemaphoreA(NULL, MAX_PEDIDOS, MAX_PEDIDOS, NOME_SEM_SLOTS);   // comeca com todos livres
    if (hMutex == NULL || hSemPedidos == NULL || hSemSlots == NULL) {
        printf("erro ao criar mutex/semaforos (codigo %lu)\n", GetLastError());
        return 1;
    }
    for (int i = 0; i < MAX_PEDIDOS; i++) {
        char nome[40];
        sprintf(nome, "%s%d", PREFIXO_EVENTO, i);
        hEventos[i] = CreateEventA(NULL, FALSE, FALSE, nome); // auto-reset, comeca desligado
        if (hEventos[i] == NULL) {
            printf("erro ao criar evento %d (codigo %lu)\n", i, GetLastError());
            return 1;
        }
    }

    // cria as threads do pool
    HANDLE hThreads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        hThreads[i] = CreateThread(NULL, 0, threadServidor, (LPVOID)(INT_PTR)(i + 1), 0, NULL);
        if (hThreads[i] == NULL) {
            printf("erro ao criar a thread %d (codigo %lu)\n", i + 1, GetLastError());
            return 1;
        }
    }

    printf("servidor pronto, %d threads esperando pedidos (%d registros carregados)\n", NUM_THREADS, totalRegistros);
    printf("nao feche essa janela enquanto estiver usando o cliente\n");

    // a thread principal so espera; quem trabalha sao as threads do pool
    WaitForMultipleObjects(NUM_THREADS, hThreads, TRUE, INFINITE);
    return 0;
}

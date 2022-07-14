#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#define WORKING_SET_LIMIT 4

typedef struct No_lista_swap
{
    int pid;
    struct No_lista_swap *prox;
} No_lista_swap;
typedef struct No_lru
{
    int pid;
    int page_number;
    struct No_lru *prox;
} No_lru;

typedef struct Linha_tabela
{
    short int em_memoria;
    int frame;
} Linha_tabela;

pthread_t threads[20];
pthread_mutex_t lock_prox_pid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_numero_frames_utilizados = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_lista_swap = PTHREAD_MUTEX_INITIALIZER;
sem_t semaforo_swap;
int prox_pid = 1;
int numero_frames_utilizados = 0;
int prox_swap = 1;
No_lista_swap *inicio_lista_swap, *fim_lista_swap;

No_lista_swap *criar_no_lista_swap(int pid)
{
    No_lista_swap *novo;
    novo = malloc(sizeof(No_lista_swap *));
    novo->pid = pid;
    novo->prox = NULL;
    return novo;
}
No_lru *criar_no_lru(int pid, int numero_pagina)
{
    No_lru *novo;
    novo = malloc(sizeof(No_lru *));
    novo->pid = pid;
    novo->page_number = numero_pagina;
    novo->prox = NULL;
    return novo;
}

No_lru *achar_ultimo_no(No_lru *inicio)
{
    No_lru *atual;
    atual = inicio;
    while (atual->prox != NULL)
    {
        atual = atual->prox;
    }
    return atual;
}

void *processo(void *arg)
{
    int id;
    int numero_paginas_em_memoria = 0;
    short esta_na_lista_swap = 0;
    short deve_fazer_swap = 0;
    Linha_tabela tabela_paginas[50]; // tabela de páginas
    No_lru *lru_lista, *atual, *prox;

    pthread_mutex_lock(&lock_prox_pid);
    id = prox_pid;
    prox_pid++;
    pthread_mutex_unlock(&lock_prox_pid);
    // inicializa tabela de páginas
    for (size_t i = 0; i < 50; i++)
    {
        Linha_tabela nova_linha;
        nova_linha.em_memoria = 0;
        nova_linha.frame = -1;
    }
    // inicializa a lista de rerêncida do LRU
    lru_lista = criar_no_lru(id, 0);
    for (size_t i = 0; i < WORKING_SET_LIMIT - 1; i++)
    {
        lru_lista->prox = criar_no_lru(id, 0);
    }

    srand(time(NULL));
    while (1)
    {
        sleep(3);
        int prox_pagina = rand() % 50; // próxima página pedida pelo processo
        Linha_tabela pagina = tabela_paginas[prox_pagina];
        if (pagina.em_memoria)
        {

            atual = lru_lista;

            while (1)
            {
                prox = atual->prox;
                if (prox->page_number != prox_pagina)
                {
                    atual = prox;
                    continue;
                }
                atual->prox = prox->prox;
                break;
            }
            // garante que o nó mais recente sempre esteja No_lru começo
            atual->prox = lru_lista->prox;
            free(lru_lista);
            lru_lista = atual;
        }
        else
        {
            pthread_mutex_lock(&numero_frames_utilizados);//inicio de sessão crítica
            short todos_os_frames_em_uso = numero_frames_utilizados > 64 ? 1 : 0;
            pthread_mutex_unlock(&numero_frames_utilizados);//fim de sessão crítica

            if (todos_os_frames_em_uso)
            {
                if (esta_na_lista_swap)
                {
                    pthread_mutex_lock(&lock_lista_swap);//inicio de sessão crítica
                    short primeiro_no_da_lista = inicio_lista_swap->pid == id ? 1 : 0;
                    if (primeiro_no_da_lista)
                    {
                        No_lista_swap *temp;
                        temp = inicio_lista_swap;
                        inicio_lista_swap = temp->prox;
                        free(temp);
                        esta_na_lista_swap = 0;
                        while (lru_lista->prox != NULL)
                        {
                            int pagina_a_ser_removida = lru_lista->page_number;
                            tabela_paginas[pagina_a_ser_removida].em_memoria = 0;
                        }
                        pthread_mutex_unlock(&lock_lista_swap);

                        sem_post(&semaforo_swap);
                    }
                }
                else
                {
                    sem_wait(&semaforo_swap);
                }
            }
            pthread_mutex_lock(&lock_lista_swap); // inicio de sessão crítica
            if (inicio_lista_swap == NULL)        // se lista está vazia
            {
                inicio_lista_swap = criar_no_lista_swap(id);
                fim_lista_swap = inicio_lista_swap;
                esta_na_lista_swap = 1;
            }
            else // lista não vazia
            {
                if (!esta_na_lista_swap)
                {
                    No_lista_swap *temp;
                    temp = criar_no_lista_swap(id);
                    fim_lista_swap->prox = temp;
                    fim_lista_swap = temp;
                    esta_na_lista_swap = 1;
                }
            }
            pthread_mutex_unlock(&lock_lista_swap); // fim de sessão crítica

            No_lru *novo = criar_no_lru(id, prox_pagina); // novo nó
            // adiciona novo ao inicio
            novo->prox = lru_lista;
            lru_lista = novo;
            // remove o ultimo nó
            atual = lru_lista;
            prox = lru_lista->prox;
            while (prox->prox != NULL)
            {
                // loop até que atual seja o penultimo nó
                //  e prox o ultimo nó
                atual = atual->prox;
                prox = atual->prox;
            }
            tabela_paginas[prox->page_number].em_memoria = 0; // marca o nó a ser retirado como fora da memória
            int index = tabela_paginas[prox->page_number].frame;
            tabela_paginas[prox_pagina].em_memoria = 1; // marca o novo nó como em memória
            atual->prox = NULL;
            free(prox);

            numero_paginas_em_memoria = numero_paginas_em_memoria < 4 ? numero_paginas_em_memoria + 1 : 4;
            pthread_mutex_lock(&lock_numero_frames_utilizados);
            numero_frames_utilizados++;
            pthread_mutex_unlock(&lock_numero_frames_utilizados);
        }
    }
}

int main(int argc, char const *argv[])
{
    sem_init(&semaforo_swap, 0, 0);
    inicio_lista_swap = NULL;
    // nada importante são só testes
    No_lru *a, *b, *c, *d, *ultimo, *atual;
    a = criar_no_lru(15, 1);
    b = criar_no_lru(16, 1);
    c = criar_no_lru(17, 1);
    d = criar_no_lru(18, 1);
    a->prox = b;
    b->prox = c;
    atual = a;
    while (atual->prox != NULL)
    {
        printf("%d\n", atual->pid);
    }
    ultimo = achar_ultimo_no(a);
    printf("%d\n", ultimo->pid);
    atual = a;
    while (atual->prox != NULL)
    {
        printf("%d\n", atual->pid);
    }
    return 0;
}

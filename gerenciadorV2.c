#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#define WORKING_SET_LIMIT 4
#define TOTAL_FRAMES 4
const int NTHREADS = 2;

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
pthread_mutex_t teste = PTHREAD_MUTEX_INITIALIZER;
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

    printf("Processo %d criado\n", id);

    // inicializa tabela de páginas
    for (size_t i = 0; i < 50; i++)
    {
        Linha_tabela nova_linha;
        nova_linha.em_memoria = 0;
        nova_linha.frame = -1;
        tabela_paginas[i] = nova_linha;
    }

    // inicializa a lista de rerêncida do LRU
    lru_lista = criar_no_lru(id, 0);
    for (size_t i = 0; i < WORKING_SET_LIMIT - 1; i++)
        lru_lista->prox = criar_no_lru(id, 0);

    while (1)
    {

        int prox_pagina = rand() % 50; // próxima página pedida pelo processo
        

        Linha_tabela pagina = tabela_paginas[prox_pagina];

        // printf("Tabela de páginas atual do processo %d: ", id);
        // for (size_t i = 0; i < 50; i++)
        //     if (tabela_paginas[i].em_memoria)
        //         printf("%d ", i);

        // printf("\n");
        if (pagina.em_memoria)
        {
            printf("Processo %d pediu alocação da pagina %d\n", id, prox_pagina);
            printf("EM MEMORIA %d\n", id);
            atual = lru_lista;
            if (atual->page_number != prox_pagina)
            {
                while (1)
                {
                    printf("TO NO WHILE %d\n", id);
                    if (atual->prox == NULL)
                        printf("SOU NULL %d\n", id);

                    prox = atual->prox;
                    if (prox != NULL && (prox->page_number != prox_pagina))
                    {
                        atual = prox;
                        continue;
                    }
                    atual->prox = prox->prox;
                    break;
                }
            }
            // garante que o nó mais recente sempre esteja No_lru começo
            atual->prox = lru_lista->prox;
            free(lru_lista);
            lru_lista = atual;
            printf("ACABEI %d\n", id);
        }
        else
        {
            pthread_mutex_lock(&lock_numero_frames_utilizados);
            if (numero_paginas_em_memoria == 0)
            {
                numero_frames_utilizados += 4;
                numero_paginas_em_memoria = 4;
            }

            pthread_mutex_unlock(&lock_numero_frames_utilizados);

            pthread_mutex_lock(&lock_numero_frames_utilizados); // inicio de sessão crítica
            short todos_os_frames_em_uso = numero_frames_utilizados > TOTAL_FRAMES ? 1 : 0;
            pthread_mutex_unlock(&lock_numero_frames_utilizados); // fim de sessão crítica
            if (todos_os_frames_em_uso)
            {
                printf("TODOS FRAMES EM USO %d\n", id);

                if (esta_na_lista_swap)
                {
                    // printf("Entrou no if %d\n", id);
                    pthread_mutex_lock(&lock_lista_swap); // inicio de sessão crítica
                    short primeiro_no_da_lista = inicio_lista_swap->pid == id ? 1 : 0;
                    if (primeiro_no_da_lista)
                    {
                        No_lista_swap *temp;
                        temp = inicio_lista_swap;
                        inicio_lista_swap = temp->prox;
                        free(temp);
                        pthread_mutex_lock(&lock_numero_frames_utilizados);
                        numero_frames_utilizados -= 4;
                        numero_paginas_em_memoria = 0;
                        pthread_mutex_unlock(&lock_numero_frames_utilizados);
                        esta_na_lista_swap = 0;
                        No_lru *aux = lru_lista;
                        while (aux->prox != NULL)
                        {
                            int pagina_a_ser_removida = lru_lista->page_number;
                            tabela_paginas[pagina_a_ser_removida].em_memoria = 0;
                            aux = aux->prox;
                        }
                        
                        printf("ENVIEI SINAL %d\n", id);
                        sem_post(&semaforo_swap);
                    }
                    pthread_mutex_unlock(&lock_lista_swap);
                }
                else
                {
                    printf("TO ESPERANDO SEMAFORO %d\n", id);
                    sem_wait(&semaforo_swap);
                }
            }
            printf("Processo %d pediu alocação da pagina %d\n", id, prox_pagina);
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

            No_lru *novo = criar_no_lru(id, prox_pagina); // novo nów

            // adiciona novo ao inicio
            novo->prox = lru_lista;
            lru_lista = novo;
            // remove o ultimo nó
            atual = lru_lista;
            prox = lru_lista->prox;

            No_lru *aux = atual;

            while (prox->prox != NULL)
            {
                // loop até que atual seja o penultimo nó
                //  e prox o ultimo nó
                atual = atual->prox;
                prox = atual->prox;
            }
            // printf("Paginas atuais: ");
            // while (1)
            // {
            //     if (aux->page_number)
            //         printf("%d ", aux->page_number);
            //     if (aux->prox == NULL)
            //         break;
            //     aux = aux->prox;
            // }
            // printf("\n");

            tabela_paginas[prox->page_number].em_memoria = 0; // marca o nó a ser retirado como fora da memória
            int index = tabela_paginas[prox->page_number].frame;
            tabela_paginas[prox_pagina].em_memoria = 1; // marca o novo nó como em memória
            atual->prox = NULL;
            free(prox);
            // if (numero_paginas_em_memoria < WORKING_SET_LIMIT)
            // {
            //     numero_paginas_em_memoria++;
            //     pthread_mutex_lock(&lock_numero_frames_utilizados);
            //     numero_frames_utilizados = numero_frames_utilizados + 1;
            //     pthread_mutex_unlock(&lock_numero_frames_utilizados);
            // }
        }
        sleep(3);
    }
}

int main(int argc, char const *argv[])
{
    sem_init(&semaforo_swap, 0, 0);
    inicio_lista_swap = NULL;
    pthread_t *threads;
    int quantThred = 0;
    srand(time(NULL));

    // Alocano memoria
    if ((threads = malloc(sizeof(pthread_t) * NTHREADS)) == NULL)
    {
        printf("ERRO-- malloc\n");
        return 1;
    }
    // // Criação das threads limitadas a um total de 20 threads, e sendo criada a cada 3 segundos.
    // while (quantThred < 20)
    // {
    //     sleep(3);
    //     if (pthread_create(&threads[quantThred], NULL, processo, NULL))
    //         exit(-1);
    //     quantThred++;
    // }

    // cria as threads Processo
    for (int i = 0; i < NTHREADS; i++)
    {
        sleep(3);
        if (pthread_create(&threads[i], NULL, processo, NULL))
            exit(-1);
    }

    // Espera todas as threads completarem
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    pthread_exit(NULL);

    return 0;
}

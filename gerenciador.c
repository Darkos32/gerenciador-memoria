#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>

#define WORKING_SET_LIMIT 4

pthread_t threads[20];
pthread_mutex_t lock_prox_pid = PTHREAD_MUTEX_INITIALIZER;
int prox_pid = 1;
int numero_frames_utilizados= 0;
typedef struct No
{
    int pid;
    int page_number;
    struct no *prox;
}No;

typedef struct Linha_tabela
{
    short int em_memoria;
    int frame;
}Linha_tabela;

No* criar_no(int pid, int numero_pagina){
    No *novo;
    novo = malloc(sizeof(No *));
    novo->pid = pid;
    novo->page_number = numero_pagina;
    novo->prox = NULL;
    return novo;
}

No* achar_ultimo_no(No* inicio){
    No *atual;
    atual = inicio;
    while (atual->prox!=NULL)
    {
        atual = atual->prox;
    }
    return atual;
}

void *processo(void *arg){
    int id;
    int numero_paginas_em_memoria = 0;
    Linha_tabela tabela_paginas[50]; // tabela de páginas
    No *lru_lista, *atual, *prox;

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
    //inicializa a lista de rerêncida do LRU
    lru_lista = criar_no(id, 0);
    for (size_t i = 0; i < WORKING_SET_LIMIT - 1; i++)
    {
        lru_lista->prox = criar_no(id, 0);
    }

    srand(time(NULL));
    while (1)
    {
        sleep(3);
        int prox_pagina = rand() % 50;//próxima página pedida pelo processo
        Linha_tabela pagina = tabela_paginas[prox_pagina];
        if (pagina.em_memoria)
        {
            
            atual = lru_lista;
            
            while (1)
            {
                prox = atual->prox;
                if (prox->page_number != prox_pagina)
                {
                    atual = prox       
                    continue;
                }
                atual->prox = prox->prox;
                break;
            }
            atual-> lru_lista->prox;
            free(lre_lista);
            lru_lista =  atual;
            
        }else{
            numero_paginas_em_memoria = numero_paginas_em_memoria<4?numero_paginas_em_memoria+1:4;
            No* novo = criar_no(id,prox_pagina);//novo nó
            //adiciona novo ao inicio
            novo->prox = lru_lista;
            lru_lista = novo;
            //remove o ultimo nó 
            atual = lru_lista;
            prox = lru_lista->prox       
            while(prox->prox !=NULL){
                 atual = atual->prox;
                 prox = atual->prox;
            }
            tabela_paginas[prox->page_number].em_memoria = 0;// altera o estado na tabela de páginas, retirando o ultimo nó da memória
            int index = tabela_paginas[prox->page_number].frame;
            tabela_paginas[prox_pagina].em_memoria = 1;
            atual->prox = NULL;
            free(prox);
            
        }
        
    }
}

int main(int argc, char const *argv[])
{
    // nada importante são só testes
    No *a,*b,*c,*d,*ultimo,*atual;
    a = criar_no(15, 1);
    b = criar_no(16, 1);
    c = criar_no(17, 1);
    d = criar_no(18, 1);
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

// tetris_stack.c
// Tetris Stack - Níveis Novato, Aventureiro e Mestre integrados

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define FILA_CAP 5
#define PILHA_CAP 3
#define NOME_PIECE 3

// tipos de peças (nomes curtos)
const char *TIPOS[] = {"I", "O", "T", "L", "J", "S", "Z"};
const int NUM_TIPOS = 7;

// struct que representa uma peça
typedef struct {
    char nome[NOME_PIECE]; // "I", "O", ...
    int id;                // identificador unico
} Peca;

// fila circular
typedef struct {
    Peca dados[FILA_CAP];
    int frente; // índice da frente (0..FILA_CAP-1)
    int count;  // quantos elementos tem
} Fila;

// pilha simples
typedef struct {
    Peca dados[PILHA_CAP];
    int top; // indice do topo (-1 se vazia)
} Pilha;

// enum de ação para undo
typedef enum {
    A_NENHUMA = 0,
    A_PLAY,         // jogou peça da frente
    A_RESERVE,      // reservou (push) a peça da frente
    A_USE_RESERVED, // usou (pop) peça reservada
    A_SWAP,         // trocou topo pilha com frente fila
    A_INVERT        // inverteu fila/pilha (reversão)
} AcaoUltima;

// struct para guardar dados do último movimento (undo)
typedef struct {
    AcaoUltima acao;
    Peca peca_played;   // peça que foi jogada/reservada/usada
    bool had_new_enqueued; // se após a ação foi enfileirada nova peça
    Peca new_enqueued;  // peça enfileirada após a ação (para desfazer)
    // para swap, guardar as duas peças trocadas
    Peca swap_fila;
    Peca swap_pilha;
} UndoInfo;

// protótipos
Peca gerarPeca();
void fila_inicializar(Fila *f);
bool fila_esta_cheia(const Fila *f);
bool fila_esta_vazia(const Fila *f);
bool fila_enfileirar(Fila *f, Peca p);
bool fila_desenfileirar(Fila *f, Peca *out);
void fila_mostrar(const Fila *f);
bool fila_dequeue_tail(Fila *f, Peca *out); // remove do tail
bool fila_enqueue_front(Fila *f, Peca p);   // insere na frente (front)

void pilha_inicializar(Pilha *s);
bool pilha_esta_cheia(const Pilha *s);
bool pilha_esta_vazia(const Pilha *s);
bool pilha_push(Pilha *s, Peca p);
bool pilha_pop(Pilha *s, Peca *out);
void pilha_mostrar(const Pilha *s);

void mostrar_estado(const Fila *f, const Pilha *s);
void fazer_jogar(Fila *f, Pilha *s, UndoInfo *undo);
void fazer_reservar(Fila *f, Pilha *s, UndoInfo *undo);
void fazer_usar_reservada(Fila *f, Pilha *s, UndoInfo *undo);
void fazer_swap(Fila *f, Pilha *s, UndoInfo *undo);
void fazer_invert(Fila *f, Pilha *s, UndoInfo *undo);
void desfazer(Fila *f, Pilha *s, UndoInfo *undo);

// implementação

// gera uma peça aleatória com id incremental
Peca gerarPeca() {
    static int next_id = 1;
    Peca p;
    const char *t = TIPOS[rand() % NUM_TIPOS];
    strncpy(p.nome, t, NOME_PIECE);
    p.nome[NOME_PIECE - 1] = '\0';
    p.id = next_id++;
    return p;
}

// FILA
void fila_inicializar(Fila *f) {
    f->frente = 0;
    f->count = 0;
}

bool fila_esta_cheia(const Fila *f) {
    return f->count == FILA_CAP;
}

bool fila_esta_vazia(const Fila *f) {
    return f->count == 0;
}

// enfileira no tail (final)
bool fila_enfileirar(Fila *f, Peca p) {
    if (fila_esta_cheia(f)) return false;
    int tail = (f->frente + f->count) % FILA_CAP;
    f->dados[tail] = p;
    f->count++;
    return true;
}

// desenfileira da frente
bool fila_desenfileirar(Fila *f, Peca *out) {
    if (fila_esta_vazia(f)) return false;
    if (out) *out = f->dados[f->frente];
    f->frente = (f->frente + 1) % FILA_CAP;
    f->count--;
    return true;
}

// remove do tail (ultimo) - usado no undo
bool fila_dequeue_tail(Fila *f, Peca *out) {
    if (fila_esta_vazia(f)) return false;
    int tail = (f->frente + f->count - 1 + FILA_CAP) % FILA_CAP;
    if (out) *out = f->dados[tail];
    f->count--;
    return true;
}

// insere na frente (usa se preciso no undo)
bool fila_enqueue_front(Fila *f, Peca p) {
    if (fila_esta_cheia(f)) return false;
    f->frente = (f->frente - 1 + FILA_CAP) % FILA_CAP;
    f->dados[f->frente] = p;
    f->count++;
    return true;
}

void fila_mostrar(const Fila *f) {
    printf("\n--- FILA (frente -> tail) (cap %d) ---\n", FILA_CAP);
    if (fila_esta_vazia(f)) {
        printf("  (vazia)\n");
        return;
    }
    for (int i = 0; i < f->count; i++) {
        int idx = (f->frente + i) % FILA_CAP;
        printf("  Pos %d: %s (id:%d)\n", i+1, f->dados[idx].nome, f->dados[idx].id);
    }
}

// PILHA
void pilha_inicializar(Pilha *s) {
    s->top = -1;
}

bool pilha_esta_cheia(const Pilha *s) {
    return s->top == PILHA_CAP - 1;
}

bool pilha_esta_vazia(const Pilha *s) {
    return s->top == -1;
}

bool pilha_push(Pilha *s, Peca p) {
    if (pilha_esta_cheia(s)) return false;
    s->top++;
    s->dados[s->top] = p;
    return true;
}

bool pilha_pop(Pilha *s, Peca *out) {
    if (pilha_esta_vazia(s)) return false;
    if (out) *out = s->dados[s->top];
    s->top--;
    return true;
}

void pilha_mostrar(const Pilha *s) {
    printf("\n--- PILHA (topo) ---\n");
    if (pilha_esta_vazia(s)) {
        printf("  (vazia)\n");
        return;
    }
    for (int i = s->top; i >= 0; i--) {
        printf("  [%d] %s (id:%d)\n", i, s->dados[i].nome, s->dados[i].id);
    }
}

// mostra estado das estruturas
void mostrar_estado(const Fila *f, const Pilha *s) {
    fila_mostrar(f);
    pilha_mostrar(s);
}

// AÇÕES

// Jogar: remove peça da frente (dequeue) - Novato
// Ao jogar, o sistema automaticamente enfileira uma nova peça no tail para manter fila cheia
void fazer_jogar(Fila *f, Pilha *s, UndoInfo *undo) {
    if (fila_esta_vazia(f)) {
        printf("\nFila vazia! Nao ha peça para jogar.\n");
        return;
    }
    Peca played;
    fila_desenfileirar(f, &played);
    printf("\nVoce jogou a peça: %s (id:%d)\n", played.nome, played.id);

    // depois de jogar, enfileira nova peça automaticamente
    Peca nova = gerarPeca();
    bool enq = fila_enfileirar(f, nova);
    if (enq) {
        printf("Nova peca gerada e enfileirada: %s (id:%d)\n", nova.nome, nova.id);
    } else {
        // se não conseguiu enfileirar (fila cheia inesperadamente), ignorar
        printf("Aviso: nao foi possivel enfileirar nova peca automaticamente.\n");
    }

    // grava info para undo
    undo->acao = A_PLAY;
    undo->peca_played = played;
    undo->had_new_enqueued = enq;
    if (enq) undo->new_enqueued = nova;
}

// Reservar: retira peça da frente e empilha na pilha (Aventureiro)
// Mantem fila cheia com nova peça enfileirada
void fazer_reservar(Fila *f, Pilha *s, UndoInfo *undo) {
    if (pilha_esta_cheia(s)) {
        printf("\nPilha cheia! Nao e possivel reservar mais peças.\n");
        return;
    }
    if (fila_esta_vazia(f)) {
        printf("\nFila vazia! Nao ha peça para reservar.\n");
        return;
    }
    Peca front;
    fila_desenfileirar(f, &front);
    bool pushed = pilha_push(s, front);
    if (pushed) {
        printf("\nPeca reservada: %s (id:%d)\n", front.nome, front.id);
    } else {
        printf("\nErro ao empilhar.\n");
    }

    // enfileira nova peça para manter fila cheia
    Peca nova = gerarPeca();
    bool enq = fila_enfileirar(f, nova);
    if (enq) printf("Nova peca enfileirada: %s (id:%d)\n", nova.nome, nova.id);

    // salvar undo
    undo->acao = A_RESERVE;
    undo->peca_played = front; // peça que foi movida para pilha
    undo->had_new_enqueued = enq;
    if (enq) undo->new_enqueued = nova;
}

// Usar peça reservada (pop) - Aventureiro
// Ao usar, a peça é removida da pilha (consumida) e uma nova peça é enfileirada para manter fila cheia
void fazer_usar_reservada(Fila *f, Pilha *s, UndoInfo *undo) {
    if (pilha_esta_vazia(s)) {
        printf("\nPilha vazia! Nao ha peça reservada para usar.\n");
        return;
    }
    Peca top;
    pilha_pop(s, &top);
    printf("\nVoce usou a peca reservada: %s (id:%d)\n", top.nome, top.id);

    // enfileira nova peça para manter fila cheia (mesmo que fila nao tenha sido afetada)
    Peca nova = gerarPeca();
    bool enq = fila_enfileirar(f, nova);
    if (enq) printf("Nova peca gerada e enfileirada: %s (id:%d)\n", nova.nome, nova.id);

    // salvar undo
    undo->acao = A_USE_RESERVED;
    undo->peca_played = top; // peça que foi usada (removida da pilha)
    undo->had_new_enqueued = enq;
    if (enq) undo->new_enqueued = nova;
}

// Trocar topo da pilha com frente da fila (Mestre)
void fazer_swap(Fila *f, Pilha *s, UndoInfo *undo) {
    if (pilha_esta_vazia(s)) {
        printf("\nPilha vazia! Nada para trocar.\n");
        return;
    }
    if (fila_esta_vazia(f)) {
        printf("\nFila vazia! Nada para trocar.\n");
        return;
    }
    // Pega referencias
    Peca top = s->dados[s->top];
    Peca front = f->dados[f->frente];

    // salva para undo
    undo->acao = A_SWAP;
    undo->swap_fila = front;
    undo->swap_pilha = top;

    // troca direta
    s->dados[s->top] = front;
    f->dados[f->frente] = top;

    printf("\nSwap realizado: topo pilha <-> frente fila\n");
    printf("  Nova frente: %s (id:%d)\n", f->dados[f->frente].nome, f->dados[f->frente].id);
    printf("  Novo topo: %s (id:%d)\n", s->dados[s->top].nome, s->dados[s->top].id);
}

// Inverter fila e pilha (Mestre)
// Implementado como: reverte a ordem da fila e reverte a ordem dos elementos na pilha.
// Operacao é reversivel (undo = inverter novamente)
void fazer_invert(Fila *f, Pilha *s, UndoInfo *undo) {
    // salva ação
    undo->acao = A_INVERT;

    // inverter fila
    for (int i = 0; i < f->count / 2; i++) {
        int idx1 = (f->frente + i) % FILA_CAP;
        int idx2 = (f->frente + f->count - 1 - i + FILA_CAP) % FILA_CAP;
        Peca tmp = f->dados[idx1];
        f->dados[idx1] = f->dados[idx2];
        f->dados[idx2] = tmp;
    }

    // inverter pilha
    for (int i = 0; i < (s->top + 1) / 2; i++) {
        int j = s->top - i;
        Peca tmp = s->dados[i];
        s->dados[i] = s->dados[j];
        s->dados[j] = tmp;
    }

    printf("\nOperacao Inverter executada: fila e pilha invertidas (ordem revertida).\n");
}

// desfazer ultima ação
void desfazer(Fila *f, Pilha *s, UndoInfo *undo) {
    if (undo->acao == A_NENHUMA) {
        printf("\nNada para desfazer.\n");
        return;
    }

    switch (undo->acao) {
    case A_PLAY:
        // Para desfazer PLAY: removemos o elemento enfileirado no tail (se houver)
        // e colocamos a peça jogada de volta na frente.
        if (undo->had_new_enqueued) {
            Peca tail;
            if (fila_dequeue_tail(f, &tail)) {
                // opcional: validar que tail == new_enqueued (nao obrigatorio)
                // agora inserir played na frente
                if (!fila_enqueue_front(f, undo->peca_played)) {
                    // caso nao consiga enfileirar na frente (improvavel), tentar re-enfileirar tail de volta
                    fila_enfileirar(f, tail);
                    printf("\nErro ao desfazer: nao foi possivel reinserir a peca na frente.\n");
                    break;
                }
                printf("\nDesfeito PLAY: nova peca removida (id:%d) e peca jogada recolocada na frente (id:%d)\n",
                       tail.id, undo->peca_played.id);
            } else {
                printf("\nErro ao desfazer PLAY: fila nao tem tail para remover.\n");
            }
        } else {
            // se nao enfileirou nova peça (caso raro), apenas insere played na frente
            if (fila_enqueue_front(f, undo->peca_played))
                printf("\nDesfeito PLAY: peca recolocada na frente (id:%d)\n", undo->peca_played.id);
            else
                printf("\nErro ao desfazer PLAY.\n");
        }
        break;

    case A_RESERVE:
        // desfazer RESERVE: foi retirada peça da frente e empilhada.
        // Para desfazer: desempilhar e recolocar na frente; também remover a nova enfileirada.
        if (!pilha_esta_vazia(s)) {
            // supondo que a peça empilhada no topo seja a mesma que movemos, vamos desempilhar
            Peca top;
            if (pilha_pop(s, &top)) {
                // tentar recolocar na frente
                if (!fila_enqueue_front(f, top)) {
                    // se falhar, tentar recolocar no topo de pilha
                    pilha_push(s, top);
                    printf("\nErro ao desfazer RESERVE: nao foi possivel recolocar na fila.\n");
                    break;
                }
                // remover a nova enfileirada (tail) se houver
                if (undo->had_new_enqueued) {
                    Peca tail;
                    if (fila_dequeue_tail(f, &tail)) {
                        printf("\nDesfeito RESERVE: peca recolocada na frente (id:%d) e nova peca removida (id:%d)\n",
                               top.id, tail.id);
                    }
                } else {
                    printf("\nDesfeito RESERVE: peca recolocada na frente (id:%d)\n", top.id);
                }
            } else {
                printf("\nErro ao desfazer RESERVE: pilha vazia inesperadamente.\n");
            }
        } else {
            printf("\nErro ao desfazer RESERVE: pilha vazia (nada para desempilhar).\n");
        }
        break;

    case A_USE_RESERVED:
        // desfazer USE_RESERVED: re-criar o estado anterior: retirar a nova enfileirada (tail) e recolocar peça na pilha
        if (undo->had_new_enqueued) {
            Peca tail;
            if (fila_dequeue_tail(f, &tail)) {
                // push the used piece back to stack
                if (pilha_push(s, undo->peca_played)) {
                    printf("\nDesfeito USE_RESERVED: peca recolocada na pilha (id:%d) e nova peca removida (id:%d)\n",
                           undo->peca_played.id, tail.id);
                } else {
                    // se nao couber, re-enfileirar tail de volta
                    fila_enfileirar(f, tail);
                    printf("\nErro ao desfazer USE_RESERVED: pilha cheia, nao foi possivel recolocar.\n");
                }
            } else {
                printf("\nErro ao desfazer USE_RESERVED: nao havia nova peca para remover.\n");
            }
        } else {
            // sem nova enfileirada: apenas push de volta
            if (pilha_push(s, undo->peca_played)) {
                printf("\nDesfeito USE_RESERVED: peca recolocada na pilha (id:%d)\n", undo->peca_played.id);
            } else {
                printf("\nErro ao desfazer USE_RESERVED: pilha cheia.\n");
            }
        }
        break;

    case A_SWAP:
        // desfazer swap: trocar novamente as pecas salvas
        // encontra front e top e repoe
        if (!fila_esta_vazia(f) && !pilha_esta_vazia(s)) {
            // volta as pecas salvas
            f->dados[f->frente] = undo->swap_fila;
            s->dados[s->top] = undo->swap_pilha;
            printf("\nDesfeito SWAP: pecas restauradas.\n");
        } else {
            printf("\nErro ao desfazer SWAP: estrutura(es) vazia(s).\n");
        }
        break;

    case A_INVERT:
        // desfazer invert é inverter novamente (operacao simetrica)
        // reutiliza a mesma função
        // inverter fila
        for (int i = 0; i < f->count / 2; i++) {
            int idx1 = (f->frente + i) % FILA_CAP;
            int idx2 = (f->frente + f->count - 1 - i + FILA_CAP) % FILA_CAP;
            Peca tmp = f->dados[idx1];
            f->dados[idx1] = f->dados[idx2];
            f->dados[idx2] = tmp;
        }
        // inverter pilha
        for (int i = 0; i < (s->top + 1) / 2; i++) {
            int j = s->top - i;
            Peca tmp = s->dados[i];
            s->dados[i] = s->dados[j];
            s->dados[j] = tmp;
        }
        printf("\nDesfeito INVERT: ordem restaurada.\n");
        break;

    default:
        printf("\nAcao desconhecida para desfazer.\n");
        break;
    }

    // limpa undo
    undo->acao = A_NENHUMA;
}

// função para inicializar fila com 5 peças automaticamente (Novato)
void fila_inicializar_com_pecas(Fila *f) {
    fila_inicializar(f);
    for (int i = 0; i < FILA_CAP; i++) {
        Peca p = gerarPeca();
        fila_enfileirar(f, p);
    }
}

// menu principal
int main() {
    srand((unsigned int) time(NULL));
    Fila fila;
    Pilha pilha;
    UndoInfo undo;

    fila_inicializar_com_pecas(&fila);
    pilha_inicializar(&pilha);

    undo.acao = A_NENHUMA;

    int opc;
    printf("=== TETRIS STACK - Integrado (Novato, Aventureiro, Mestre) ===\n");
    printf("Estilo: aluno - simples e funcional\n");

    do {
        printf("\n\n--- MENU ---\n");
        printf("1 - Mostrar fila e pilha (estado)\n");
        printf("2 - Jogar uma peca (dequeue) [Novato]\n");
        printf("3 - Reservar peca (push) [Aventureiro]\n");
        printf("4 - Usar peca reservada (pop) [Aventureiro]\n");
        printf("5 - Trocar topo da pilha com frente da fila (swap) [Mestre]\n");
        printf("6 - Desfazer ultima acao (undo) [Mestre]\n");
        printf("7 - Inverter fila e pilha (reverter ordem) [Mestre]\n");
        printf("0 - Sair\n");
        printf("Escolha uma opcao: ");
        if (scanf("%d", &opc) != 1) {
            printf("Entrada invalida. Tente novamente.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n'); // limpa buffer

        switch (opc) {
            case 1:
                mostrar_estado(&fila, &pilha);
                break;
            case 2:
                fazer_jogar(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 3:
                fazer_reservar(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 4:
                fazer_usar_reservada(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 5:
                fazer_swap(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 6:
                desfazer(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 7:
                fazer_invert(&fila, &pilha, &undo);
                mostrar_estado(&fila, &pilha);
                break;
            case 0:
                printf("Saindo do Tetris Stack. Ate logo!\n");
                break;
            default:
                printf("Opcao invalida. Tente novamente.\n");
        }

    } while (opc != 0);

    return 0;
}

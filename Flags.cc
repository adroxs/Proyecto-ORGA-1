#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>




/* Valores de ifun para la ALU */
#define ALU_ANDQ 0x0
#define ALU_XORQ 0x1
#define ALU_ADDQ 0x2
#define ALU_SUBQ 0x3

/* Condiciones de salto */
#define JMP 0   // salto incondicional 
#define JL  1   // salto si SF != OF 
#define JE  2   // salto si ZF = 1 
#define JNZ 3   // salto si ZF = 0 

typedef struct {
    uint8_t ZF;   // Zero Flag
    uint8_t SF;   // Sign Flag 
    uint8_t OF;   // Overflow Flag 
} Flags;

/* Banderas del cpu */
static Flags flags;

/* Inicializar el cpu*/
void flags_reset(void)
{
    flags.ZF = 0;
    flags.SF = 0;
    flags.OF = 0;
}

/* ZF: vale 1 solo si el resultado es 0 */
uint8_t calcular_zf(uint64_t res)
{
    if (res == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* SF: vale 1 si el resultado es par*/
uint8_t calcular_sf(uint64_t res)
{

    if ((res & 1ULL) == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* OF para la suma */
uint8_t calcular_of_suma(uint64_t a, uint64_t b, uint64_t res)
{
    uint64_t signo_a = a >> 63;
    uint64_t signo_b = b >> 63;
    uint64_t signo_r = res >> 63;

    if (signo_a == signo_b && signo_r != signo_a) {
        return 1;
    }
    return 0;
}

/* OF para la resta*/
uint8_t calcular_of_resta(uint64_t a, uint64_t b, uint64_t res)
{
    uint64_t signo_a = a >> 63;
    uint64_t signo_b = b >> 63;
    uint64_t signo_r = res >> 63;

    if (signo_a != signo_b && signo_r != signo_a) {
        return 1;
    }
    return 0;
}

/* ALU flags */
uint64_t alu_flags(uint8_t ifun, uint64_t rA, uint64_t rB)
{
    uint64_t res = 0;


    if (ifun == ALU_ADDQ) {
        res = rA + rB;
        flags.OF = calcular_of_suma(rA, rB, res);
    } else if (ifun == ALU_SUBQ) {
        res = rB - rA;
        flags.OF = calcular_of_resta(rB, rA, res);
    } else if (ifun == ALU_ANDQ) {
        res = rB & rA;
        flags.OF = 0;           
    } else if (ifun == ALU_XORQ) {
        res = rB ^ rA;
        flags.OF = 0;            
    }

    /* Calcular zf y sf */
    flags.ZF = calcular_zf(res);
    flags.SF = calcular_sf(res);

    return res;
}

/* Guardar el psr */
uint64_t psr_empaquetar(void)
{
    uint64_t psr = 0;
    if (flags.ZF == 1) psr = psr | 0x1;
    if (flags.SF == 1) psr = psr | 0x2;
    if (flags.OF == 1) psr = psr | 0x4;
    return psr;
}

/* Imprime el PSR y cada bandera  */
void psr_imprimir(FILE *f)
{
    fprintf(f, "PSR = 0x%llX  (ZF=%d SF=%d OF=%d)\n",
            (unsigned long long)psr_empaquetar(),
            flags.ZF, flags.SF, flags.OF);
}

/* Decide si un salto se toma, segun las banderas actuales*/
int condicion_salto(int cond)
{
    if (cond == JMP) {
        return 1;
    }
    if (cond == JL) {
        if (flags.SF != flags.OF) return 1;
        else return 0;
    }
    if (cond == JE) {
        if (flags.ZF == 1) return 1;
        else return 0;
    }
    if (cond == JNZ) {
        if (flags.ZF == 0) return 1;
        else return 0;
    }
    return 0;   
}


typedef struct {
    const char *nombre;
    uint8_t ifun;
    uint64_t rA;
    uint64_t rB;
} Caso;



// Casos de prueba para los flags
int main(void)
{
    Caso casos[] = {
        { "ADDQ  1 + 2                      ", ALU_ADDQ, 1, 2 },
        { "ADDQ  INT64_MAX + 1 (overflow)   ", ALU_ADDQ, 1, 0x7FFFFFFFFFFFFFFFULL },
        { "SUBQ  5 - 5 (da cero)            ", ALU_SUBQ, 5, 5 },
        { "SUBQ  INT64_MIN - 1 (overflow)   ", ALU_SUBQ, 1, 0x8000000000000000ULL },
        { "SUBQ  3 - 5 (negativo)           ", ALU_SUBQ, 5, 3 },
        { "ANDQ  0xF0 & 0x0F (da cero)      ", ALU_ANDQ, 0xF0, 0x0F },
        { "XORQ  0xFF ^ 0xFF (da cero)      ", ALU_XORQ, 0xFF, 0xFF },
        { "XORQ  0x5A ^ 0x01                ", ALU_XORQ, 0x01, 0x5A }
    };
    int total = sizeof(casos) / sizeof(casos[0]);
    int i;

    printf("SF = 1 cuando el resultado es par (segun el enunciado)\n\n");

    for (i = 0; i < total; i++) {
        uint64_t res;
        flags_reset();
        res = alu_flags(casos[i].ifun, casos[i].rA, casos[i].rB);
        printf("%s -> res = 0x%016llX | ", casos[i].nombre, (unsigned long long)res);
        psr_imprimir(stdout);
    }

    /* Prueba de condiciones de salto con las banderas del ultimo caso */
    printf("\nBanderas actuales: ");
    psr_imprimir(stdout);
    printf("JMP se toma: %d\n", condicion_salto(JMP));
    printf("JL  se toma: %d\n", condicion_salto(JL));
    printf("JE  se toma: %d\n", condicion_salto(JE));
    printf("JNZ se toma: %d\n", condicion_salto(JNZ));

    return 0;
}
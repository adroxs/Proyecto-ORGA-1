/* CODIGO 2: ensamblador + emulador completo OECISEY-1 (incluye el modulo de flags nuevo) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ====================================================================
 *  INICIO DEL MODULO DE BANDERAS (FLAGS)
 * ==================================================================== */

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

/* depende del interruptor SF  */
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

/* ====================== FIN DEL MODULO DE FLAGS ====================== */

/* ====================================================================
 *  CONFIGURACION GENERAL
 * ==================================================================== */
#define MEM_SIZE      65536      /* bytes de memoria simulada                */
#define MAX_ITEMS     2048       /* instrucciones/datos maximos por programa */
#define MAX_SIMBOLOS  256        /* etiquetas maximas                        */
#define MAX_LINEA     256        /* largo maximo de una linea de ensamblador */
#define MAX_CICLOS    100000     /* tope de ciclos para no quedar en bucle   */
#define MASK48        0xFFFFFFFFFFFFULL

/* ---- Codigos de estado (registro STAT) ---- */
#define STAT_AOK 0x0   /* todo bien, la CPU sigue (no esta en el enunciado) */
#define STAT_HLT 0x1
#define STAT_INS 0x2
#define STAT_AEX 0x3
#define STAT_ADR 0x4

/* ---- icodes del enunciado ---- */
#define I_HALT   0xE
#define I_NOP    0xF
#define I_RRMVQ  0x5
#define I_IRMOVQ 0x1
#define I_OPQ    0xA
#define I_JXX    0x3
#define I_PUSH   0x8
#define I_POP    0x9

/* ---- icodes ASIGNADOS POR NOSOTROS ----
 * El enunciado NO da el icode de estas instrucciones, asi que se usaron
 * valores que estaban libres en la tabla (todos con ifun = 0). */
#define I_RMMOVQ 0x2
#define I_MRMOVQ 0x4
#define I_XORR   0x6
#define I_LDD    0x7
#define I_STD    0xB
#define I_JNZ    0xC

/* Estructura de instruccion tal cual la pide el enunciado */
typedef struct {
    uint8_t  icode;   /* codigo de operacion (4 bits)        */
    uint8_t  ifun;    /* funcion especifica  (4 bits)        */
    uint8_t  rA;      /* registro A          (4 bits)        */
    uint8_t  rB;      /* registro B          (4 bits)        */
    uint64_t valC;    /* inmediato/direccion (48 bits)       */
} Instruction;

/* ====================================================================
 *  ESTADO DE LA CPU Y MEMORIA
 * ==================================================================== */
static uint8_t  *mem;            /* memoria simulada (arreglo de bytes)       */
static uint64_t  base_mem;       /* direccion de inicio = direccion de carga  */
static uint64_t  regs[8];        /* R0..R7  (R0=ZERO, R6=SP, R7=PC)           */
static uint8_t   stat = STAT_AOK;
static FILE     *trace = NULL;   /* trace.log */

/* ====================================================================
 *  SECCION 1: ENSAMBLADOR
 * ==================================================================== */

/* Entrada de la tabla de simbolos */
typedef struct {
    char     nombre[40];
    uint64_t dir;
} Simbolo;

/* Cada linea util del .asm se convierte en un Item (1 instruccion = 8 bytes) */
typedef struct {
    uint64_t dir;               /* direccion donde queda cargado           */
    char     texto[MAX_LINEA];  /* texto original de la instruccion        */
    uint8_t  bytes[8];          /* codigo de maquina generado              */
    int      es_dato;           /* 1 si viene de la directiva .quad        */
    int      nlinea;            /* numero de linea en el archivo (errores) */
} Item;

static Simbolo tabla[MAX_SIMBOLOS];
static int     nsim = 0;
static Item    items[MAX_ITEMS];
static int     nitems = 0;
static uint64_t dir_actual = 0;   /* contador de direcciones durante la pasada 1 */

/* Muestra un error de ensamblado y termina el programa */
static void fatal(int nlinea, const char *msg)
{
    fprintf(stderr, "Error de ensamblado (linea %d): %s\n", nlinea, msg);
    exit(1);
}

/* Quita espacios al inicio y al final (modifica la cadena) */
static char *trim(char *s)
{
    char *fin;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    fin = s + strlen(s) - 1;
    while (fin > s && isspace((unsigned char)*fin)) {
        *fin = 0;
        fin--;
    }
    return s;
}

/* Busca una etiqueta; devuelve su indice o -1 */
static int buscar_simbolo(const char *n)
{
    int i;
    for (i = 0; i < nsim; i++) {
        if (strcmp(tabla[i].nombre, n) == 0) return i;
    }
    return -1;
}

static void agregar_simbolo(const char *n, uint64_t dir, int nlinea)
{
    if (buscar_simbolo(n) != -1) fatal(nlinea, "etiqueta duplicada");
    if (nsim >= MAX_SIMBOLOS)    fatal(nlinea, "demasiadas etiquetas");
    strncpy(tabla[nsim].nombre, n, 39);
    tabla[nsim].nombre[39] = 0;
    tabla[nsim].dir = dir;
    nsim++;
}

/* ---- PASADA 1: lee una linea, registra etiquetas y guarda el Item ---- */
static void procesar_linea(const char *linea, int nlinea)
{
    char buf[MAX_LINEA];
    char *p, *c, *dp;
    Item *it;

    strncpy(buf, linea, MAX_LINEA - 1);
    buf[MAX_LINEA - 1] = 0;

    /* Quitar comentarios (# o ;) */
    c = strpbrk(buf, "#;");
    if (c) *c = 0;

    p = trim(buf);
    if (*p == 0) return;          /* linea vacia */

    /* Etiqueta: "nombre:" al inicio de la linea */
    dp = strchr(p, ':');
    if (dp) {
        char *et;
        *dp = 0;
        et = trim(p);
        if (*et == 0 || strchr(et, ' ') != NULL) fatal(nlinea, "etiqueta mal formada");
        agregar_simbolo(et, dir_actual, nlinea);
        p = trim(dp + 1);
    }
    if (*p == 0) return;          /* la linea solo tenia etiqueta */

    if (nitems >= MAX_ITEMS) fatal(nlinea, "programa demasiado largo");
    it = &items[nitems];
    nitems++;
    it->dir = dir_actual;
    strncpy(it->texto, p, MAX_LINEA - 1);
    it->texto[MAX_LINEA - 1] = 0;
    it->nlinea = nlinea;
    it->es_dato = 0;
    memset(it->bytes, 0, 8);
    dir_actual += 8;              /* toda instruccion o dato ocupa 8 bytes */
}

/* ---- Utilidades de la PASADA 2 ---- */

/* Arma los 8 bytes: [icode|ifun] [rA|rB] [valC en 6 bytes little-endian] */
static void poner(uint8_t *b, int icode, int ifun, int rA, int rB, uint64_t valC)
{
    int i;
    b[0] = (uint8_t)((icode << 4) | (ifun & 0xF));
    b[1] = (uint8_t)((rA << 4) | (rB & 0xF));
    for (i = 0; i < 6; i++) {
        b[2 + i] = (uint8_t)((valC >> (8 * i)) & 0xFF);
    }
}

/* Convierte "R3", "%r3", "SP", "PC", "ZERO" en un numero de registro (o -1) */
static int parse_reg(const char *s)
{
    char t[16];
    int j = 0;
    if (*s == '%') s++;
    while (*s && j < 15) {
        t[j] = (char)toupper((unsigned char)*s);
        j++;
        s++;
    }
    t[j] = 0;
    if (t[0] == 'R' && t[1] >= '0' && t[1] <= '7' && t[2] == 0) return t[1] - '0';
    if (strcmp(t, "ZERO") == 0) return 0;
    if (strcmp(t, "SP") == 0)   return 6;
    if (strcmp(t, "PC") == 0)   return 7;
    return -1;
}

/* Registro obligatorio: si no es valido, error */
static int reg_o_error(const char *s, Item *it)
{
    int r = parse_reg(s);
    if (r < 0) fatal(it->nlinea, "registro invalido");
    return r;
}

/* Verifica la cantidad de operandos */
static void exigir(int n, int esperado, Item *it)
{
    if (n != esperado) fatal(it->nlinea, "cantidad de operandos incorrecta");
}

/* Convierte un numero (decimal/hex) o una etiqueta en valor.
 * *era_etiqueta = 1 si era una etiqueta. */
static uint64_t parse_valor(const char *s, int nlinea, int *era_etiqueta)
{
    char *fin;
    *era_etiqueta = 0;

    if (isdigit((unsigned char)s[0]) ||
        ((s[0] == '-' || s[0] == '+') && isdigit((unsigned char)s[1]))) {
        uint64_t v;
        if (s[0] == '-') v = (uint64_t)strtoll(s, &fin, 0);
        else             v = (uint64_t)strtoull(s, &fin, 0);
        if (*fin != 0) fatal(nlinea, "numero mal formado");
        return v;
    } else {
        int k = buscar_simbolo(s);
        if (k < 0) fatal(nlinea, "etiqueta no definida");
        *era_etiqueta = 1;
        return tabla[k].dir;
    }
}

/* Interpreta "D(Rb)": devuelve el desplazamiento y el registro base */
static int64_t parse_mem(const char *s, int *reg, Item *it)
{
    char buf[64];
    char *ab, *ce, *dstr, *rstr;
    int et;
    int64_t d = 0;

    strncpy(buf, s, 63);
    buf[63] = 0;
    ab = strchr(buf, '(');
    ce = strchr(buf, ')');
    if (!ab || !ce || ce < ab) fatal(it->nlinea, "se esperaba D(Rb)");
    *ab = 0;
    *ce = 0;
    dstr = trim(buf);
    rstr = trim(ab + 1);
    if (*dstr != 0) d = (int64_t)parse_valor(dstr, it->nlinea, &et);
    *reg = reg_o_error(rstr, it);
    return d;
}

/* Verifica que un inmediato quepa en 48 bits (con signo) */
static void revisar_48(uint64_t v, Item *it)
{
    int64_t s = (int64_t)v;
    if (s < -(1LL << 47) || s >= (1LL << 48)) fatal(it->nlinea, "valor no cabe en 48 bits");
}

/* ---- PASADA 2: traduce un Item a 8 bytes de codigo de maquina ---- */
static void codificar_item(Item *it)
{
    char copia[MAX_LINEA];
    char *mn, *resto, *p;
    char *ops[3];
    int n = 0, i, rA, rB, et;
    uint64_t v;
    int64_t d;
    uint8_t *b = it->bytes;

    strcpy(copia, it->texto);
    mn = strtok(copia, " \t");
    resto = strtok(NULL, "");
    for (i = 0; mn[i]; i++) mn[i] = (char)toupper((unsigned char)mn[i]);

    /* Separar operandos por coma */
    if (resto) {
        p = strtok(resto, ",");
        while (p && n < 3) {
            ops[n] = trim(p);
            n++;
            p = strtok(NULL, ",");
        }
    }

    if (strcmp(mn, "HALT") == 0) {
        exigir(n, 0, it);
        poner(b, I_HALT, 0, 0, 0, 0);
    }
    else if (strcmp(mn, "NOP") == 0) {
        exigir(n, 0, it);
        poner(b, I_NOP, 0, 0, 0, 0);
    }
    else if (strcmp(mn, "RRMVQ") == 0) {           /* RRMVQ rA, rB */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_RRMVQ, 0, rA, rB, 0);
    }
    else if (strcmp(mn, "IRMOVQ") == 0) {          /* IRMOVQ imm, rB */
        exigir(n, 2, it);
        v = parse_valor(ops[0], it->nlinea, &et);
        revisar_48(v, it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_IRMOVQ, 0, 0, rB, v & MASK48);
    }
    else if (strcmp(mn, "ANDQ") == 0) {            /* operaciones de la ALU: OPQ */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_OPQ, ALU_ANDQ, rA, rB, 0);
    }
    else if (strcmp(mn, "XORQ") == 0) {
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_OPQ, ALU_XORQ, rA, rB, 0);
    }
    else if (strcmp(mn, "ADDQ") == 0) {
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_OPQ, ALU_ADDQ, rA, rB, 0);
    }
    else if (strcmp(mn, "SUBQ") == 0) {
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_OPQ, ALU_SUBQ, rA, rB, 0);
    }
    else if (strcmp(mn, "XORR") == 0) {            /* XORR Ra, Rb */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        rB = reg_o_error(ops[1], it);
        poner(b, I_XORR, 0, rA, rB, 0);
    }
    else if (strcmp(mn, "JMP") == 0 || strcmp(mn, "JL") == 0 ||
             strcmp(mn, "JE") == 0  || strcmp(mn, "JNZ") == 0) {
        /* Saltos: Offset = (Direccion de Etiqueta) - PC, donde PC es la
         * direccion de la propia instruccion de salto. Si el operando es
         * un numero, se toma como offset ya calculado. */
        int icode = I_JXX, ifun = 0;
        exigir(n, 1, it);
        if (strcmp(mn, "JMP") == 0) ifun = 0;
        if (strcmp(mn, "JL") == 0)  ifun = 1;
        if (strcmp(mn, "JE") == 0)  ifun = 2;
        if (strcmp(mn, "JNZ") == 0) { icode = I_JNZ; ifun = 0; }
        v = parse_valor(ops[0], it->nlinea, &et);
        if (et) d = (int64_t)(v - it->dir);
        else    d = (int64_t)v;
        poner(b, icode, ifun, 0, 0, (uint64_t)d & MASK48);
    }
    else if (strcmp(mn, "PUSH") == 0) {
        exigir(n, 1, it);
        rA = reg_o_error(ops[0], it);
        poner(b, I_PUSH, 0, rA, 0, 0);
    }
    else if (strcmp(mn, "POP") == 0) {
        exigir(n, 1, it);
        rA = reg_o_error(ops[0], it);
        poner(b, I_POP, 0, rA, 0, 0);
    }
    else if (strcmp(mn, "LDD") == 0) {             /* LDD Ra, D(Rb) */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        d = parse_mem(ops[1], &rB, it);
        poner(b, I_LDD, 0, rA, rB, (uint64_t)d & MASK48);
    }
    else if (strcmp(mn, "STD") == 0) {             /* STD Ra, D(Rb) */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        d = parse_mem(ops[1], &rB, it);
        poner(b, I_STD, 0, rA, rB, (uint64_t)d & MASK48);
    }
    else if (strcmp(mn, "MRMOVQ") == 0) {          /* MRMOVQ D(Rb), Ra */
        exigir(n, 2, it);
        d = parse_mem(ops[0], &rB, it);
        rA = reg_o_error(ops[1], it);
        poner(b, I_MRMOVQ, 0, rA, rB, (uint64_t)d & MASK48);
    }
    else if (strcmp(mn, "RMMOVQ") == 0) {          /* RMMOVQ Ra, D(Rb) */
        exigir(n, 2, it);
        rA = reg_o_error(ops[0], it);
        d = parse_mem(ops[1], &rB, it);
        poner(b, I_RMMOVQ, 0, rA, rB, (uint64_t)d & MASK48);
    }
    else if (strcmp(mn, ".QUAD") == 0) {           /* dato de 8 bytes */
        exigir(n, 1, it);
        v = parse_valor(ops[0], it->nlinea, &et);
        for (i = 0; i < 8; i++) b[i] = (uint8_t)((v >> (8 * i)) & 0xFF);
        it->es_dato = 1;
    }
    else {
        fatal(it->nlinea, "instruccion desconocida");
    }
}

/* ====================================================================
 *  SECCION 2: EMULADOR (CICLO UNICO, 5 ETAPAS)
 * ==================================================================== */

/* Extiende el signo de un valor de 48 bits a 64 bits */
static uint64_t sext48(uint64_t v)
{
    v = v & MASK48;
    if (v & (1ULL << 47)) v = v | 0xFFFF000000000000ULL;
    return v;
}

/* Comprueba que [dir, dir+n) este dentro de la memoria simulada */
static int dir_valida(uint64_t dir, int n)
{
    if (dir < base_mem) return 0;
    if (dir > base_mem + MEM_SIZE - n) return 0;
    return 1;
}

/* Lee 8 bytes (little-endian). Si la direccion es mala: STAT_ADR */
static int leer64(uint64_t dir, uint64_t *val)
{
    int i;
    uint64_t v = 0;
    if (!dir_valida(dir, 8)) {
        stat = STAT_ADR;
        return 0;
    }
    for (i = 0; i < 8; i++) {
        v |= (uint64_t)mem[dir - base_mem + i] << (8 * i);
    }
    *val = v;
    return 1;
}

/* Escribe 8 bytes (little-endian). Si la direccion es mala: STAT_ADR */
static int escribir64(uint64_t dir, uint64_t val)
{
    int i;
    if (!dir_valida(dir, 8)) {
        stat = STAT_ADR;
        return 0;
    }
    for (i = 0; i < 8; i++) {
        mem[dir - base_mem + i] = (uint8_t)((val >> (8 * i)) & 0xFF);
    }
    return 1;
}

/* Descompone los 8 bytes de una instruccion en la estructura Instruction */
static Instruction decodificar(const uint8_t *b)
{
    Instruction ins;
    int i;
    ins.icode = b[0] >> 4;
    ins.ifun  = b[0] & 0xF;
    ins.rA    = b[1] >> 4;
    ins.rB    = b[1] & 0xF;
    ins.valC  = 0;
    for (i = 0; i < 6; i++) {
        ins.valC |= (uint64_t)b[2 + i] << (8 * i);
    }
    return ins;
}

/* Un ciclo de reloj completo: Fetch, Decode, Execute, Memory, Write-Back.
 * Si algo sale mal, deja el codigo en 'stat' y retorna sin modificar nada. */
static void ciclo(void)
{
    uint8_t bytes[8];
    Instruction ins;
    uint64_t pc = regs[7];
    uint64_t valP, valA = 0, valB = 0, valE = 0, valM = 0, nuevo_pc;
    int dst = -1;            /* registro destino del resultado de la ALU/mov */
    int valida = 0;
    int64_t desp;

    /* ---------------- FETCH ---------------- */
    if (pc % 8 != 0) {                    /* PC desalineado */
        stat = STAT_AEX;
        return;
    }
    if (!dir_valida(pc, 8)) {             /* PC fuera de memoria */
        stat = STAT_ADR;
        return;
    }
    memcpy(bytes, &mem[pc - base_mem], 8);
    ins = decodificar(bytes);
    valP = pc + 8;                        /* siguiente instruccion */
    nuevo_pc = valP;

    /* ---------------- DECODE ---------------- */
    /* 1) validar icode / ifun */
    switch (ins.icode) {
        case I_HALT: case I_NOP: case I_RRMVQ: case I_IRMOVQ:
        case I_PUSH: case I_POP: case I_RMMOVQ: case I_MRMOVQ:
        case I_XORR: case I_LDD: case I_STD: case I_JNZ:
            valida = (ins.ifun == 0);
            break;
        case I_OPQ:
            valida = (ins.ifun <= 3);
            break;
        case I_JXX:
            valida = (ins.ifun <= 2);
            break;
        default:
            valida = 0;
    }
    if (ins.rA > 7 || ins.rB > 7) valida = 0;   /* solo existen R0..R7 */
    if (!valida) {
        stat = STAT_INS;
        return;
    }

    /* 2) leer los registros que la instruccion necesita */
    switch (ins.icode) {
        case I_RRMVQ:
            valA = regs[ins.rA];
            break;
        case I_OPQ: case I_XORR:
            valA = regs[ins.rA];
            valB = regs[ins.rB];
            break;
        case I_PUSH:
            valA = regs[ins.rA];
            valB = regs[6];
            break;
        case I_POP:
            valA = regs[6];
            valB = regs[6];
            break;
        case I_LDD: case I_MRMOVQ:
            valB = regs[ins.rB];
            break;
        case I_STD: case I_RMMOVQ:
            valA = regs[ins.rA];
            valB = regs[ins.rB];
            break;
        default:
            break;
    }

    /* ---------------- EXECUTE ---------------- */
    desp = (int64_t)sext48(ins.valC);
    switch (ins.icode) {
        case I_RRMVQ:
            valE = valA;
            dst = ins.rB;
            break;
        case I_IRMOVQ:
            valE = sext48(ins.valC);
            dst = ins.rB;
            break;
        case I_OPQ:
            valE = alu_flags(ins.ifun, valA, valB);   /* actualiza flags */
            if (ins.ifun == ALU_ADDQ) dst = ins.rA;       /* ADDQ: rA <- rA + rB */
            else                      dst = ins.rB;
            break;
        case I_XORR:
            valE = alu_flags(ALU_XORQ, valA, valB);   /* tambien actualiza flags */
            dst = ins.rB;
            break;
        case I_LDD: case I_MRMOVQ: case I_STD: case I_RMMOVQ:
            valE = valB + (uint64_t)desp;                 /* direccion efectiva [Rb + D] */
            break;
        case I_PUSH:
            valE = valB - 8;
            break;
        case I_POP:
            valE = valB + 8;
            break;
        case I_JXX:
            if (condicion_salto(ins.ifun == 0 ? JMP :
                                ins.ifun == 1 ? JL  : JE))
                nuevo_pc = pc + (uint64_t)desp;
            break;
        case I_JNZ:
            if (condicion_salto(JNZ))
                nuevo_pc = pc + (uint64_t)desp;
            break;
        case I_HALT:
            stat = STAT_HLT;
            break;
        default:
            break;
    }

    /* ---------------- MEMORY ---------------- */
    switch (ins.icode) {
        case I_PUSH:
            if (valE % 8 != 0) { stat = STAT_AEX; return; }   /* %rsp desalineado */
            if (!escribir64(valE, valA)) return;
            break;
        case I_POP:
            if (valA % 8 != 0) { stat = STAT_AEX; return; }
            if (!leer64(valA, &valM)) return;
            break;
        case I_LDD: case I_MRMOVQ:
            if (!leer64(valE, &valM)) return;
            break;
        case I_STD: case I_RMMOVQ:
            if (!escribir64(valE, valA)) return;
            break;
        default:
            break;
    }

    /* ---------------- WRITE-BACK ---------------- */
    switch (ins.icode) {
        case I_RRMVQ: case I_IRMOVQ: case I_OPQ: case I_XORR:
            regs[dst] = valE;
            break;
        case I_PUSH:
            regs[6] = valE;
            break;
        case I_POP:
            regs[6] = valE;
            regs[ins.rA] = valM;       /* si rA es SP, gana el valor leido */
            break;
        case I_LDD: case I_MRMOVQ:
            regs[ins.rA] = valM;
            break;
        default:
            break;
    }
    regs[0] = 0;                       /* R0 (ZERO) siempre vale 0 */

    /* ---------------- ACTUALIZAR PC ---------------- */
    regs[7] = nuevo_pc;                /* esta asignacion tiene prioridad sobre R7 */
}

/* Escribe una linea en trace.log con el estado ANTES de ejecutar el ciclo
 * (registros 'snap') y el STAT resultante del ciclo. */
static void escribir_traza(const uint64_t *snap)
{
    int i;
    fprintf(trace, "PC: 0x%llx | ", (unsigned long long)snap[7]);
    for (i = 0; i < 8; i++) {
        fprintf(trace, "R%d: 0x%llx", i, (unsigned long long)snap[i]);
        if (i < 7) fprintf(trace, " ");
    }
    fprintf(trace, " | STAT: 0x%x\n", stat);
}

/* ====================================================================
 *  SECCION 3: SALIDA Y PROGRAMA PRINCIPAL
 * ==================================================================== */

static const char *nombre_stat(uint8_t s)
{
    if (s == STAT_AOK) return "AOK (sin terminar)";
    if (s == STAT_HLT) return "HLT (apagado normal)";
    if (s == STAT_INS) return "INS (instruccion ilegal)";
    if (s == STAT_AEX) return "AEX (error de alineacion)";
    if (s == STAT_ADR) return "ADR (direccion invalida)";
    return "desconocido";
}

/* Lee una cedula por teclado (ignora puntos y guiones) */
static unsigned long long leer_cedula(int n)
{
    char linea[128];
    char dig[64];
    int i, j = 0;
    printf("Cedula del integrante %d: ", n);
    fflush(stdout);
    if (!fgets(linea, sizeof(linea), stdin)) {
        fprintf(stderr, "No se pudo leer la cedula.\n");
        exit(1);
    }
    for (i = 0; linea[i] && j < 63; i++) {
        if (isdigit((unsigned char)linea[i])) {
            dig[j] = linea[i];
            j++;
        }
    }
    dig[j] = 0;
    if (j == 0) {
        fprintf(stderr, "Cedula invalida.\n");
        exit(1);
    }
    return strtoull(dig, NULL, 10);
}

/* Verifica que el nombre termine en .asm */
static int termina_en_asm(const char *s)
{
    size_t l = strlen(s);
    if (l < 5) return 0;
    return (tolower((unsigned char)s[l - 4]) == '.' &&
            tolower((unsigned char)s[l - 3]) == 'a' &&
            tolower((unsigned char)s[l - 2]) == 's' &&
            tolower((unsigned char)s[l - 1]) == 'm');
}

int main(int argc, char **argv)
{
    char salida[300] = "salida.txt";
    char linea[MAX_LINEA];
    unsigned long long c1, c2, c3, suma, inicio32, inicio;
    FILE *fin = NULL, *out;
    int nlinea = 0, i, k;
    uint64_t ciclos = 0;
    uint64_t snap[8];

    printf("=== Ensamblador + Emulador OECISEY-1 (Fase 1) ===\n");

    /* ---- 1. Direccion de inicio: suma de las 3 cedulas ---- */
    c1 = leer_cedula(1);
    c2 = leer_cedula(2);
    c3 = leer_cedula(3);
    suma = c1 + c2 + c3;
    inicio32 = suma & 0xFFFFFFFFULL;       /* 32 bits menos significativos */
    inicio = inicio32;
    if (inicio % 8 != 0) {
        inicio = inicio - (inicio % 8);    /* alineamos a 8 para que el PC sea valido */
    }

    /* ---- 2. Abrir entrada: archivo .asm o teclado ---- */
    if (argc >= 2) {
        char *punto;
        if (!termina_en_asm(argv[1])) {
            fprintf(stderr, "El archivo de entrada debe tener extension .asm\n");
            return 1;
        }
        fin = fopen(argv[1], "r");
        if (!fin) {
            perror("No se pudo abrir el archivo");
            return 1;
        }
        strncpy(salida, argv[1], 290);
        salida[290] = 0;
        punto = strrchr(salida, '.');
        if (punto) *punto = 0;
        strcat(salida, ".txt");
    } else {
        printf("Escriba el codigo ensamblador (termine con una linea que diga FIN):\n");
    }

    /* ---- 3. PASADA 1: leer lineas, etiquetas y direcciones ---- */
    dir_actual = inicio;
    base_mem = inicio;
    while (1) {
        char *r;
        if (fin) r = fgets(linea, sizeof(linea), fin);
        else     r = fgets(linea, sizeof(linea), stdin);
        if (!r) break;
        nlinea++;
        if (!fin) {
            char tmp[MAX_LINEA];
            strcpy(tmp, linea);
            if (strcmp(trim(tmp), "FIN") == 0) break;
        }
        procesar_linea(linea, nlinea);
    }
    if (fin) fclose(fin);
    if (nitems == 0) {
        fprintf(stderr, "El programa esta vacio.\n");
        return 1;
    }
    if ((uint64_t)nitems * 8 > MEM_SIZE / 2) {
        fprintf(stderr, "El programa no cabe en la memoria simulada.\n");
        return 1;
    }

    /* ---- 4. PASADA 2: generar codigo de maquina ---- */
    for (i = 0; i < nitems; i++) {
        codificar_item(&items[i]);
    }

    /* ---- 5. Cargar en memoria e inicializar la CPU ---- */
    mem = (uint8_t *)calloc(MEM_SIZE, 1);
    if (!mem) {
        fprintf(stderr, "Sin memoria.\n");
        return 1;
    }
    for (i = 0; i < nitems; i++) {
        memcpy(&mem[items[i].dir - base_mem], items[i].bytes, 8);
    }
    for (i = 0; i < 8; i++) regs[i] = 0;
    regs[6] = base_mem + MEM_SIZE;      /* SP: tope de la memoria (la pila crece hacia abajo) */
    regs[7] = base_mem;                 /* PC: primera instruccion */
    flags_reset();
    stat = STAT_AOK;

    /* ---- 6. Ejecutar y generar trace.log ---- */
    trace = fopen("trace.log", "w");
    if (!trace) {
        perror("No se pudo crear trace.log");
        return 1;
    }
    while (stat == STAT_AOK && ciclos < MAX_CICLOS) {
        memcpy(snap, regs, sizeof(regs));   /* estado ANTES del ciclo */
        ciclo();
        escribir_traza(snap);
        ciclos++;
    }
    fclose(trace);

    /* ---- 7. Escribir el archivo .txt de salida ---- */
    out = fopen(salida, "w");
    if (!out) {
        perror("No se pudo crear el archivo de salida");
        return 1;
    }

    fprintf(out, "==================== TABLA DE SIMBOLOS ====================\n");
    fprintf(out, "%-20s | %s\n", "Etiqueta", "Direccion");
    for (i = 0; i < nsim; i++) {
        fprintf(out, "%-20s | 0x%08llX\n", tabla[i].nombre, (unsigned long long)tabla[i].dir);
    }

    fprintf(out, "\n============ DIRECCION DE INICIO (CALCULO) ============\n");
    fprintf(out, "Cedula 1 = %llu\nCedula 2 = %llu\nCedula 3 = %llu\n", c1, c2, c3);
    fprintf(out, "Suma     = %llu + %llu + %llu = %llu\n", c1, c2, c3, suma);
    fprintf(out, "32 bits menos significativos = suma & 0xFFFFFFFF = 0x%08llX\n", inicio32);
    if (inicio != inicio32) {
        fprintf(out, "No es multiplo de 8; se alinea hacia abajo (PC debe ser multiplo de 8)\n");
    }
    fprintf(out, "Direccion de inicio del codigo = 0x%08llX\n", inicio);

    fprintf(out, "\n================= TRADUCCION =================\n");
    fprintf(out, "%-12s | %-23s | %s\n", "Direccion", "Codigo de maquina (hex)", "# instruccion ensamblador");
    for (i = 0; i < nitems; i++) {
        fprintf(out, "0x%08llX   | ", (unsigned long long)items[i].dir);
        for (k = 0; k < 8; k++) {
            fprintf(out, "%02X", items[i].bytes[k]);
            if (k < 7) fprintf(out, " ");
        }
        fprintf(out, " | %s\n", items[i].texto);
    }

    fprintf(out, "\n============== RESULTADO DE LA EJECUCION ==============\n");
    fprintf(out, "Ciclos ejecutados: %llu\n", (unsigned long long)ciclos);
    fprintf(out, "STAT final: 0x%X - %s\n", stat, nombre_stat(stat));
    if (ciclos >= MAX_CICLOS && stat == STAT_AOK) {
        fprintf(out, "AVISO: se alcanzo el limite de ciclos (posible bucle infinito)\n");
    }

    fprintf(out, "\n============ REGISTROS FINALES ============\n");
    fprintf(out, "R0 (ZERO) = 0x%016llX\n", (unsigned long long)regs[0]);
    for (i = 1; i <= 5; i++) {
        fprintf(out, "R%d        = 0x%016llX\n", i, (unsigned long long)regs[i]);
    }
    fprintf(out, "R6 (SP)   = 0x%016llX\n", (unsigned long long)regs[6]);
    fprintf(out, "R7 (PC)   = 0x%016llX\n", (unsigned long long)regs[7]);
    psr_imprimir(out);

    /* Datos (.quad) tal como quedaron en memoria despues de ejecutar */
    k = 0;
    for (i = 0; i < nitems; i++) if (items[i].es_dato) k++;
    if (k > 0) {
        fprintf(out, "\n============ DATOS (.quad) AL FINAL ============\n");
        for (i = 0; i < nitems; i++) {
            if (items[i].es_dato) {
                uint64_t v;
                int j;
                char asc[9];
                leer64(items[i].dir, &v);
                for (j = 0; j < 8; j++) {
                    unsigned char ch = (unsigned char)((v >> (8 * j)) & 0xFF);
                    asc[j] = (ch >= 32 && ch < 127) ? (char)ch : '.';
                }
                asc[8] = 0;
                fprintf(out, "0x%08llX | 0x%016llX | \"%s\"\n",
                        (unsigned long long)items[i].dir, (unsigned long long)v, asc);
            }
        }
    }
    fclose(out);

    printf("Listo. STAT final = 0x%X (%s), %llu ciclos.\n", stat, nombre_stat(stat),
           (unsigned long long)ciclos);
    printf("Salida: %s | Traza: trace.log\n", salida);
    free(mem);
    return 0;
}
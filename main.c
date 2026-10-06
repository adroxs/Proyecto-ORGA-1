#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
//Basico
/*int main() {
    FILE *file = fopen("prueba.asm", "r");
    if (file == NULL) {
        printf("Error: No se pudo abrir el archivo.\n");
        return 1;
    }

    char line[256];
    // Leemos línea por línea hasta el final
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == ';') {
            continue; // Se salta las líneas vacías del archivo .asm y va a la siguiente
        }
        printf("Lei: %s", line); // Aquí luego haremos la magia

    }

    fclose(file);
    return 0;
}
*/

//Corte de espacios y cosas no deseadas
/*int main() {
    FILE *file = fopen("prueba.asm", "r");
    if (file == NULL) {
        printf("Error: No se pudo abrir el archivo.\n");
        return 1;
    }

    char line[256];
    int contador = 1; // Para llevar la cuenta de instrucciones reales

    //printf("=== LECTURA LIMPIA ===\n");

    while (fgets(line, sizeof(line), file)) {
        //strcspn escanea la linea hasta encontrar uno de los caracteres a comparar, y luego lo reemplaza con \0 y asi
        line[strcspn(line, "\r\n")] = '\0';
        

        // Puntero auxiliar para saltar los espacios del inicio
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++; // Avanza el puntero para ignorar espacios y tabulaciones
        }

        // Si despues de los espacios no hay nada o un ';', se ignora esa linea (por comentario o por linea vacia)
        if (*p == '\0' || *p == ';') {
            continue;
        }

        // Si llego hasta aqui, es una instruccion util
        printf("[Instruccion %d]: %s\n", contador, p);
        contador++;
    }

    printf("=== TOTAL LINEAS UTILES: %d ===\n", contador - 1);

    fclose(file);
    return 0;
}*/

//Separacion de instrucciones (IMPORTANTE, AQUI SI HAY # LO TOMAMOS COMO COMENT)
/*int main() {
    FILE *file = fopen("caso_9.asm", "r");
    if (file == NULL) {
        printf("Error: No se pudo abrir el archivo.\n");
        return 1;
    }

    char line[256];
    int num_linea = 1;

    while (fgets(line, sizeof(line), file)) {
        // 1. Limpieza basica que ya logramos
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == ';') continue;

        printf("\n=== ANALIZANDO LÍNEA %d: %s ===\n", num_linea, p);

        // 2. AQUI EMPIEZA EL BLOQUE 2: Delimitadores = Espacio, Tab, Coma
        char *delimitadores = " \t,";
        
        // Primera llamada: le pasamos la linea 'p'
        char *token = strtok(p, delimitadores);
        int i = 0;

        // Bucle: Mientras 'token' no sea NULL, significa que hay mas piezas
        while (token != NULL) {
            printf("   [Token %d]: %s\n", i, token);
            i++;

            // Llamadas subsecuentes: pasamos NULL para seguir cortando la misma linea
            token = strtok(NULL, delimitadores);
        }

        num_linea++;
    }

    fclose(file);
    return 0;
}*/

//Lectura de direcciones, cosas de JUMP: FINAL: ese tipo de cosas, para registrarlas en memoria y asi
/*typedef struct {
    char nombre[32];
    uint32_t direccion;
} Simbolo;

Simbolo tabla_simbolos[100];
int total_simbolos = 0;

int main() {
    FILE *file = fopen("test_mision.asm", "r");
    if (file == NULL) {
        printf("Error: No se pudo abrir el archivo.\n");
        return 1;
    }

    // 1. Calcular PC Inicial (Pon tus cédulas reales aquí)
    uint32_t c1 = 32919270; 
    uint32_t c2 = 32900922;
    uint32_t c3 = 32912345;
    uint32_t PC = (c1 + c2 + c3) & 0xFFFFFFFF;

    printf("=== PC INICIAL CALCULADO: 0x%08X ===\n\n", PC);

    char line[256];

    // --- PRIMERA PASADA (First Pass) ---
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == ';') continue;

        // Extraemos solo el primer token (la primera palabra de la linea)
        char *token_0 = strtok(p, " \t,");
        if (token_0 == NULL) continue;

        size_t len = strlen(token_0);

        // Si la primera palabra termina en ':', es una etiqueta
        if (token_0[len - 1] == ':') {
            token_0[len - 1] = '\0'; // Quitamos el ':'

            // Guardamos en la Tabla de Símbolos
            strcpy(tabla_simbolos[total_simbolos].nombre, token_0);
            tabla_simbolos[total_simbolos].direccion = PC;
            total_simbolos++;

            // Revisamos si en esa misma linea HABÍA una instruccion despues de la etiqueta
            char *siguiente_token = strtok(NULL, " \t,");
            if (siguiente_token != NULL) {
                PC += 8; // Avanza el PC porque hay instruccion
            }
        } else {
            // No fue etiqueta, fue una instruccion normal
            PC += 8;
        }
    }

    fclose(file);

    // --- IMPRIMIR TABLA DE SÍMBOLOS GENERADA ---
    printf("=== TABLA DE SÍMBOLOS REGISTRADA ===\n");
    for (int i = 0; i < total_simbolos; i++) {
        printf("Etiqueta: %-15s -> Dirección: 0x%08X\n", 
                tabla_simbolos[i].nombre, 
                tabla_simbolos[i].direccion);
    }

    return 0;
}*/

// --- ESTRUCTURAS BASE ---
typedef struct {
    char nombre[32];
    uint32_t direccion;
} Simbolo;

Simbolo tabla_simbolos[100];
int total_simbolos = 0;

// --- FUNCIONES AUXILIARES DE TRADUCCIÓN ---

// Convierte cadenas como "r1", "r2," a su número entero (0 al 7)
int obtener_registro(char *str) {
    if (str == NULL) return 0;
    if (str[0] == 'r' || str[0] == 'R') {
        return atoi(&str[1]); // Convierte "1" a int 1
    }
    return 0;
}

// Busca una etiqueta en la Tabla de Símbolos y devuelve su dirección
uint32_t buscar_simbolo(const char *nombre) {
    for (int i = 0; i < total_simbolos; i++) {
        if (strcmp(tabla_simbolos[i].nombre, nombre) == 0) {
            return tabla_simbolos[i].direccion;
        }
    }
    return 0x00000000; // Si no la encuentra
}

int main() {
    // 1. Cédulas del equipo (Ajusta con los números reales)
    uint32_t c1 = 30000000; 
    uint32_t c2 = 29000000;
    uint32_t c3 = 31000000;
    uint32_t PC_INICIAL = (c1 + c2 + c3) & 0xFFFFFFFF;
    uint32_t PC = PC_INICIAL;

    // --- PRIMERA PASADA: Construir Tabla de Símbolos ---
    FILE *file_pass1 = fopen("test_mision.asm", "r");
    if (file_pass1 == NULL) {
        printf("Error: No se pudo abrir test_mision.asm\n");
        return 1;
    }

    char line[256];
    char line_backup[256];

    while (fgets(line, sizeof(line), file_pass1)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == ';') continue;

        char *token_0 = strtok(p, " \t,");
        if (token_0 == NULL) continue;

        size_t len = strlen(token_0);
        if (token_0[len - 1] == ':') {
            token_0[len - 1] = '\0';
            strcpy(tabla_simbolos[total_simbolos].nombre, token_0);
            tabla_simbolos[total_simbolos].direccion = PC;
            total_simbolos++;

            char *siguiente = strtok(NULL, " \t,");
            if (siguiente != NULL) PC += 8;
        } else {
            PC += 8;
        }
    }
    fclose(file_pass1);

    // --- SEGUNDA PASADA: Traducir a Hexadecimal y Generar .txt ---
    FILE *file_pass2 = fopen("test_mision.asm", "r");
    FILE *salida = fopen("programa_ensamblado.txt", "w");

    if (file_pass2 == NULL || salida == NULL) {
        printf("Error al abrir archivos para la Segunda Pasada.\n");
        return 1;
    }

    // Encabezado obligatorio del proyecto: Tabla de Símbolos
    fprintf(salida, "==================================================\n");
    fprintf(salida, "              TABLA DE SÍMBOLOS\n");
    fprintf(salida, "==================================================\n");
    for (int i = 0; i < total_simbolos; i++) {
        fprintf(salida, "Etiqueta: %-15s -> Dirección: 0x%08X\n", 
                tabla_simbolos[i].nombre, tabla_simbolos[i].direccion);
    }
    fprintf(salida, "==================================================\n\n");

    // Encabezado de la tabla de instrucciones
    fprintf(salida, "%-12s %-20s %-30s\n", "DIRECCION", "CODIGO_HEX", "INSTRUCCION_ORIGINAL");
    fprintf(salida, "------------------------------------------------------------------\n");

    PC = PC_INICIAL; // Reiniciamos el PC al inicio de la memoria

    while (fgets(line, sizeof(line), file_pass2)) {
        // Guardamos una copia intacta de la línea original para el reporte
        strcpy(line_backup, line);
        line_backup[strcspn(line_backup, "\r\n")] = '\0';

        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == ';') continue;

        char *token_0 = strtok(p, " \t,");
        if (token_0 == NULL) continue;

        char *op_code = token_0;
        size_t len = strlen(token_0);

        // Si la línea empieza con etiqueta, la ignoramos o pasamos al comando siguiente
        if (token_0[len - 1] == ':') {
            op_code = strtok(NULL, " \t,");
            if (op_code == NULL) continue; // Era una línea con solo una etiqueta
        }

        char hex_code[17] = "0000000000000000"; // 16 caracteres hex + '\0'

        // --- LÓGICA DE TRADUCCIÓN DE INSTRUCCIONES ---
        if (strcmp(op_code, "ADDQ") == 0) {
            char *rA_str = strtok(NULL, " \t,");
            char *rB_str = strtok(NULL, " \t,");
            int rA = obtener_registro(rA_str);
            int rB = obtener_registro(rB_str);
            // Formato: A2 (Opcode ADDQ) + rA + rB + 12 ceros
            sprintf(hex_code, "A2%X%X000000000000", rA, rB);
        }
        else if (strcmp(op_code, "SUBQ") == 0) {
            char *rA_str = strtok(NULL, " \t,");
            char *rB_str = strtok(NULL, " \t,");
            int rA = obtener_registro(rA_str);
            int rB = obtener_registro(rB_str);
            // Formato: A3 (Opcode SUBQ) + rA + rB + 12 ceros
            sprintf(hex_code, "A3%X%X000000000000", rA, rB);
        }
        else if (strcmp(op_code, "JMP") == 0) {
            char *etiqueta = strtok(NULL, " \t,");
            uint32_t dir_destino = buscar_simbolo(etiqueta);
            // Formato: 70 (Opcode JMP) + FF (Sin registros) + Dirección de Destino en 12 dígitos Hex
            sprintf(hex_code, "70FF%012X", dir_destino);
        }
        else if (strcmp(op_code, "NOP") == 0) {
            sprintf(hex_code, "0000000000000000");
        }

        // Escribimos la fila completa de 3 columnas en el archivo .txt
        fprintf(salida, "0x%08X   %-20s %-30s\n", PC, hex_code, line_backup);

        PC += 8; // Avanzamos 8 bytes en memoria
    }

    fclose(file_pass2);
    fclose(salida);

    printf("¡ÉXITO TOTAL! Se ha generado el archivo 'programa_ensamblado.txt'\n");
    return 0;
}
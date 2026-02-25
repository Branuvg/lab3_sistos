#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <omp.h>            // Para OpenMP: omp_set_num_threads(), omp_set_nested()

int grid[9][9];
int colsValid = 0;

/* ============================================================
 * checkColumns - FASE 3 OpenMP
 *
 * El for externo (col) se paraleliza: cada columna es independiente.
 * El for interno (row) NO se paraleliza: depende de seen[] acumulado.
 * Doc: "No todos los ciclos for deberan ser precedidos por la directiva"
 *
 * RACE CONDITION en seen[]: declarado antes del pragma y marcado como
 * private() para que cada thread tenga su propia copia local.
 * Sin private todos los threads compartiran el mismo seen[] y se
 * pisaran al escribirlo.
 * Doc: "Investigue el uso de la directiva private de OpenMP"
 *
 * valid puede tener race condition si dos threads escriben 0 al mismo
 * tiempo, pero en la practica ambos escriben el mismo valor (0) asi
 * que no hay corrupcion de datos real. Es aceptable para este lab.
 *
 * omp_set_num_threads(9): 9 columnas -> 9 threads, uno por columna.
 * Doc: "si su funcion ejecuta un for paralelo de nueve iteraciones,
 *  posiblemente el numero de threads deba ser nueve"
 * ============================================================ */
int checkColumns() {
    omp_set_num_threads(9);
    int valid = 1;
    int seen[9]; // declarado antes del pragma para poder referenciarlo en private()

    /* private(seen): cada thread recibe su propia copia de seen[].
     * IMPORTANTE: private() no inicializa la copia - el thread debe
     * hacerlo el mismo dentro del for (por eso el memset manual abajo). */
    #pragma omp parallel for private(seen) schedule(dynamic)
    for (int col = 0; col < 9; col++) {
        // Inicializar la copia local de seen[] de este thread
        for (int k = 0; k < 9; k++) seen[k] = 0;

        for (int row = 0; row < 9; row++) {
            int val = grid[row][col];
            if (val < 1 || val > 9) valid = 0;
            if (seen[val - 1])      valid = 0;
            if (val >= 1 && val <= 9) seen[val - 1] = 1;
        }
    }
    return valid;
}

/* ============================================================
 * checkRows - FASE 3 OpenMP
 * Mismo razonamiento que checkColumns: for externo (row)
 * paralelizable, for interno (col) no.
 * ============================================================ */
int checkRows() {
    omp_set_num_threads(9);
    int valid = 1;
    int seen[9];

    #pragma omp parallel for private(seen) schedule(dynamic)
    for (int row = 0; row < 9; row++) {
        for (int k = 0; k < 9; k++) seen[k] = 0;

        for (int col = 0; col < 9; col++) {
            int val = grid[row][col];
            if (val < 1 || val > 9) valid = 0;
            if (seen[val - 1])      valid = 0;
            if (val >= 1 && val <= 9) seen[val - 1] = 1;
        }
    }
    return valid;
}

/* ============================================================
 * checkSubgrid - sin pragma aqui
 * Los dos for internos NO se paralelizan porque dependen de
 * seen[] acumulado. La paralelizacion ocurre en el for del
 * main() que llama a esta funcion.
 * ============================================================ */
int checkSubgrid(int startRow, int startCol) {
    int seen[9] = {0};
    for (int row = startRow; row < startRow + 3; row++) {
        for (int col = startCol; col < startCol + 3; col++) {
            int val = grid[row][col];
            if (val < 1 || val > 9) return 0;
            if (seen[val - 1]) return 0;
            seen[val - 1] = 1;
        }
    }
    return 1;
}

void *columnThread(void *arg) {
    pid_t tid = syscall(SYS_gettid);
    printf("[columnThread] TID (kernel): %d\n", tid);
    colsValid = checkColumns();
    pthread_exit(0);
}

int main(int argc, char *argv[]) {

    /* FASE 3: Limitar OpenMP a 1 thread en main.
     * Doc: "Agregue la siguiente instruccion al principio de main():
     *  omp_set_num_threads(1)"
     * Las funciones checkColumns y checkRows sobreescriben esto
     * internamente con omp_set_num_threads(9). */
    omp_set_num_threads(1);

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("Error al abrir el archivo"); return 1; }

    struct stat fileStat;
    if (fstat(fd, &fileStat) == -1) { perror("Error en fstat"); close(fd); return 1; }

    char *fileData = mmap(NULL, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (fileData == MAP_FAILED) { perror("Error en mmap"); close(fd); return 1; }
    close(fd);

    for (int i = 0; i < 81; i++) {
        grid[i / 9][i % 9] = fileData[i] - '0';
    }
    munmap(fileData, fileStat.st_size);

    printf("Grid del Sudoku\n");
    for (int row = 0; row < 9; row++) {
        for (int col = 0; col < 9; col++) {
            printf("%d ", grid[row][col]);
            if ((col + 1) % 3 == 0 && col < 8) printf("| ");
        }
        printf("\n");
        if ((row + 1) % 3 == 0 && row < 8) printf("------+-------+------\n");
    }
    printf("\n");

    /* ============================================================
     * FASE 3: for de subcuadros paralelizado
     *
     * Cada llamada a checkSubgrid(i,i) es independiente de las otras
     * (i=0, i=3, i=6), por lo que el for se puede paralelizar.
     * subgridsValid: si un subcuadro falla, el thread escribe 0.
     * Usamos reduction(&:subgridsValid) para combinar los resultados
     * de todos los threads con AND bit a bit de forma segura.
     * Doc: el for de subcuadros es uno de los que "se puede paralelizar"
     * ============================================================ */
    int subgridsValid = 1;
    #pragma omp parallel for schedule(dynamic) reduction(&:subgridsValid)
    for (int i = 0; i <= 6; i += 3) {
        if (!checkSubgrid(i, i)) {
            subgridsValid = 0;
        }
    }

    pid_t parentPID = getpid();

    pid_t firstChild = fork();
    if (firstChild == -1) { perror("Error en fork"); return 1; }
    if (firstChild == 0) {
        char pidStr[20];
        sprintf(pidStr, "%d", parentPID);
        execlp("ps", "ps", "-p", pidStr, "-lLf", NULL);
        perror("Error en execlp");
        exit(1);
    }

    pthread_t columnTid;
    if (pthread_create(&columnTid, NULL, columnThread, NULL) != 0) {
        perror("Error al crear pthread"); return 1;
    }

    pthread_join(columnTid, NULL);
    pid_t mainTid = syscall(SYS_gettid);
    printf("[Main] TID del thread principal: %d\n", mainTid);

    wait(NULL);

    int rowsValid = checkRows();

    printf("\nResultado de la verificacion\n");
    printf("Columnas  : %s\n", colsValid     ? "VALIDAS"   : "INVALIDAS");
    printf("Filas     : %s\n", rowsValid     ? "VALIDAS"   : "INVALIDAS");
    printf("Subcuadros: %s\n", subgridsValid ? "VALIDOS"   : "INVALIDOS");
    if (colsValid && rowsValid && subgridsValid)
        printf("\nLa solucion del Sudoku es valida\n\n");
    else
        printf("\nLa solucion del Sudoku es invalida\n\n");

    pid_t secondChild = fork();
    if (secondChild == -1) { perror("Error en segundo fork"); return 1; }
    if (secondChild == 0) {
        char pidStr[20];
        sprintf(pidStr, "%d", parentPID);
        execlp("ps", "ps", "-p", pidStr, "-lLf", NULL);
        perror("Error en execlp");
        exit(1);
    }

    wait(NULL);
    return 0;
}
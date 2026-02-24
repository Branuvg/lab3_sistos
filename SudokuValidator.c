#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // Para open()
#include <sys/mman.h>   // Para mmap()
#include <sys/stat.h>   // Para fstat()
#include <unistd.h>     // Para close()

int grid[9][9]; //grid de 9x9


int checkColumns() {
    // Recorremos cada una de las 9 columnas
    for (int col = 0; col < 9; col++) {
        // seen[i] marcara si el digito (i+1) ya aparecio en esta columna
        int seen[9] = {0};

        // Recorremos cada fila dentro de la columna actual
        for (int row = 0; row < 9; row++) {
            int val = grid[row][col];   // indices: primero fila, luego col

            // Si el valor no esta en rango valido, la columna es invalida
            if (val < 1 || val > 9) return 0;

            // Si ya lo vimos antes, hay un duplicado -> invalido
            if (seen[val - 1]) return 0;

            seen[val - 1] = 1;
        }
    }
    return 1; // Todas las columnas son validas
}


int checkRows() {
    // Recorremos cada una de las 9 filas
    for (int row = 0; row < 9; row++) {
        int seen[9] = {0};

        // Recorremos cada columna dentro de la fila actual
        for (int col = 0; col < 9; col++) {
            int val = grid[row][col];   // indices intercambiados vs checkColumns

            if (val < 1 || val > 9) return 0;
            if (seen[val - 1]) return 0;
            seen[val - 1] = 1;
        }
    }
    return 1;
}


int checkSubgrid(int startRow, int startCol) {
    int seen[9] = {0};

    // Recorremos las 3 filas del subcuadro
    for (int row = startRow; row < startRow + 3; row++) {
        // Recorremos las 3 columnas del subcuadro
        for (int col = startCol; col < startCol + 3; col++) {
            int val = grid[row][col];

            if (val < 1 || val > 9) return 0;
            if (seen[val - 1]) return 0;
            seen[val - 1] = 1;
        }
    }
    return 1;
}


int main(int argc, char *argv[]) {
   
    if (argc < 2) { // Verificar que se paso el nombre del archivo como argumento
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY); // Abrir el archivo
    if (fd == -1) {
        perror("Error al abrir el archivo");
        return 1;
    }

    struct stat fileStat; // tamano del archivo con fstat, saber el numero de bytes para pasarlo a mmap
    if (fstat(fd, &fileStat) == -1) {
        perror("Error en fstat");
        close(fd);
        return 1;
    }

    char *fileData = mmap(NULL, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // mapear el archivo a memoria
    if (fileData == MAP_FAILED) {
        perror("Error en mmap");
        close(fd);
        return 1;
    }
    close(fd); // Ya no necesitamos el file descriptor, mmap ya tiene la referencia 

    for (int i = 0; i < 81; i++) { // copiar cada simbolo del string al arreglo bidimensional
        int row = i / 9;   // division entera: 0-8 para la fila
        int col = i % 9;   // modulo: 0-8 para la columna
        grid[row][col] = fileData[i] - '0';
    }
    munmap(fileData, fileStat.st_size); // Liberar el mapeo de memoria

    printf("Grilla del Sudoku\n"); // Imprimir la grilla (hmm)
    for (int row = 0; row < 9; row++) {
        for (int col = 0; col < 9; col++) {
            printf("%d ", grid[row][col]);
            if ((col + 1) % 3 == 0 && col < 8) printf("| ");
        }
        printf("\n");
        if ((row + 1) % 3 == 0 && row < 8) printf("------+-------+------\n");
    }
    printf("\n");

    int subgridsValid = 1; // Variable para almacenar si los subcuadros son validos
    for (int i = 0; i <= 6; i += 3) {   // i toma valores 0, 3, 6
        if (!checkSubgrid(i, i)) {
            subgridsValid = 0;
            break;
        }
    }

    // Verificar filas y columnas
    int colsValid  = checkColumns();
    int rowsValid  = checkRows();

    // Mostrar resultado final
    printf("Resultado de la verificacion\n");
    printf("Columnas : %s\n", colsValid    ? "VALIDAS"   : "INVALIDAS");
    printf("Filas    : %s\n", rowsValid    ? "VALIDAS"   : "INVALIDAS");
    printf("Subcuadros: %s\n", subgridsValid ? "VALIDOS" : "INVALIDOS");

    if (colsValid && rowsValid && subgridsValid) {
        printf("\n*** La solucion del Sudoku es VALIDA ***\n");
    } else {
        printf("\n*** La solucion del Sudoku es INVALIDA ***\n");
    }

    return 0;
}

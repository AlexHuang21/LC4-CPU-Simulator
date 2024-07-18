/*
 * trace.c: location of main() to start the simulator
 */

#include "loader.h"

// Global variable defining the current state of the machine

MachineState* CPU;

//helper function to check if a file exists and return 1 if so, if not return 0
int fileExists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

//helper function to output memory contents to file
int outputMemory(MachineState* CPU, char* outputFilename) {
    //try to open file and if we can't return with an error code
    FILE* outputFile = fopen(outputFilename, "w");
    if (outputFile == NULL) {
        perror("Error opening output file");
        return -1;
    }
    //traverse through all addresses and write each address
    for (int i = 0; i < 65536; i++) {
        if (CPU->memory[i] != 0) {
            fprintf(outputFile, "address: %05d contents: 0x%04X\n", i, CPU->memory[i]);
        }
    }
    fclose(outputFile);
    return 0;
}

int main(int argc, char** argv) {
    //make sure we're receiving at least one output file and obj file
    if (argc < 3) {
        printf("Usage: %s output_filename.txt first.obj [second.obj ...]\n", argv[0]);
    }

    //cehck if an obj file exists and if not exit with an error code
    for (int i = 2; i < argc; i++) {
        if (!fileExists(argv[i])) {
            printf("Error: file %s not found\n", argv[i]);
            return -1;
        }
    }

    //initialize machine state structure and reset CPU
    CPU = malloc(sizeof (MachineState));
    Reset(CPU);

    //load each obj file into machine's memory
    for (int i = 2; i < argc; i++) {
        //make sure all files can be read and if not exit with error code
        if (ReadObjectFile(argv[i], CPU) != 0) {
            printf("Error loading file %s\n", argv[i]);
            return -1;
        }
    }

    //output memory contents to the file and we're done
    if(outputMemory(CPU, argv[1])) return -1;

    free(CPU);

    return 0;
}
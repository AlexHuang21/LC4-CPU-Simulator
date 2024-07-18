/*
 * loader.c : Defines loader functions for opening and loading object files
 */

#include "loader.h"
#include <stdio.h>
#include <stdint.h>

//define headers for different kinds of words in the obj file
#define CODE_HEADER 0xCADE
#define DATA_HEADER 0xDADA
#define SYMBOL_HEADER 0xC3B7
#define FILENAME_HEADER 0xF17E
#define LINENUMBER_HEADER 0x715E

// memory array location
unsigned short memoryAddress;

/*
 * Read an object file and modify the machine state as described in the writeup
 */

int reached_eof;

//helper function to read a word from the file
uint16_t readWord(FILE *file) {
  uint16_t word;
  if(fread(&word, sizeof(uint16_t), 1, file) == 0) reached_eof = 1;
  // Swap bytes to handle endianness
    return ((word >> 8) & 0xFF) | ((word & 0xFF) << 8);
}

int ReadObjectFile(char* filename, MachineState* CPU) {
  //open file in binary read mode

  reached_eof = 0;

  FILE *file = fopen(filename, "rb");

  if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

  //loop through everything in the file
  while (!feof(file)) {
    //read each header in the file
    uint16_t header = readWord(file);

    if(reached_eof) break;

    switch (header) {
      //if the word is a code header 
      case CODE_HEADER: {
        uint16_t address = readWord(file);
        uint16_t n = readWord(file);
        //loop through all instructions
        for (int i = 0; i < n; i++) {
          //populate the cpu memory with the instruction
          uint16_t instruction = readWord(file);
          CPU->memory[address + i] = instruction;
        }
        break;
      }
      case DATA_HEADER: {
        uint16_t address = readWord(file);
        uint16_t n = readWord(file);
        //loop through all data words
        for (int i = 0; i < n; i++) {
          //populate the cpu memory with the data
          uint16_t data = readWord(file);
          CPU->memory[address + i] = data;
        }
        break;
      }
      //for the other three cases we don't need to do anything
      case SYMBOL_HEADER: {
        uint16_t word = readWord(file);
        word = readWord(file);
        uint8_t byte;
        for (int i = 0; i < word; i++) {
          fread(&byte, sizeof byte, 1, file);
        }
        break;
      }
      case FILENAME_HEADER: {
        uint16_t word = readWord(file);
        uint8_t byte;
        for (int i = 0; i < word; i++) {
          fread(&byte, sizeof byte, 1, file);
        }
        break;
      }
      case LINENUMBER_HEADER: {
        readWord(file);
        readWord(file);
        readWord(file);
        break;
      }
      default: printf("Bad header\n"); return -1;
    }
  }
  fclose(file);
  return 0;
}

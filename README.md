# LC4-CPU-Simulator
Alex Huang

### Introduction <br>
This project implements a command-line version of the assembly-level LC4 CPU simulator, similar to PennSim. It involves loading and processing machine code files and executing them by tracking internal states such as PC, PSR, registers, and control signals. The simulator outputs a trace text file that contains information from each LC4 cycle as the program executes the loaded machine code. The project showcases the use of various algorithms and data structures to manage the CPU's operations and ensures proper memory handling and error detection.

### Features <br>
- Machine Code Processing: Load and process machine code files produced by the LC4 assembler.
- Internal State Tracking: Maintain and update the internal state of the CPU, including PC, PSR, registers, and control signals.
- Cycle Tracing: Generate a trace text file that records information from each LC4 cycle.
- Memory and Error Handling: Properly handle memory operations and detect errors such as invalid memory access and division by zero.
- Endianness Handling: Ensure correct processing of LC4 object files regardless of endianness.

### Implementation Details <br>
- Instruction Decoding and Execution: Use macros and functions to decode and execute LC4 instructions.
- Control Signal Management: Set and clear control signals for each instruction cycle.
- NZP Bit Management: Set NZP bits in the PSR based on the results of operations.
- Error Detection: Detect and handle errors such as executing data as code, accessing code as data, and invalid memory access.
- Trace Generation: Write the state of the CPU to the trace file for each cycle.

### How to Run <br>
- Prepare Machine Code Files: Create or obtain LC4 machine code files (binary files produced by the LC4 assembler).
- Compile the Simulator: Use the provided Makefile to compile the simulator.

make
- Run the Simulator: Execute the simulator with the command:

./trace output_filename.txt first.obj second.obj third.obj ...

Example:

./trace output.txt program1.obj program2.obj
- Monitor Output: The trace text file will be generated with detailed information from each LC4 cycle.

### Topics Covered <br>
- Assembly-Level CPU Simulation
- Instruction Decoding and Execution
- Memory Management
- Error Detection and Handling
- Endianness Handling
- Cycle Tracing

### Conclusion <br>
This project demonstrates the implementation of a robust LC4 CPU simulator that accurately processes and executes machine code, manages internal CPU states, and generates detailed trace files. It showcases a comprehensive understanding of CPU simulation and assembly-level programming.

Feel free to explore the code and use it as a reference for understanding LC4 CPU simulation and its practical applications!

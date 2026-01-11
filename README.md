# Tron Light Cycle Game
This code is allows you to play **Tron light cycle game** on a **VGA monitor** using your **FPGA board** with embedded C programmming on RISC-V.
The Code uses mechine interupts such as **timer interupts** to time the game and **key interupts** to register key presses outside of timer interupts.

https://github.com/user-attachments/assets/4187f4ca-70b8-49af-9139-5a798acfb4dc

To get started:
* Go to this website: https://cpulator.01xz.net/?sys=rv32-de1soc
* in the Editor window, change Language from RV32 to C
* change this preprocessor directive at the top:

```bash
#define DE10LITE 0 // change to 0 for CPUlator or DE1-SoC, 1 for DE10-Lite
```

## Getting Started with Hardware 🛠️

Connect your FPGA board to your laptop and a VGA monitor.  There are VGA
monitors in the lab with a BLUE connector on the end. If you do not have a VGA
monitor at home, you can use CPUlator to get started (see below), but to you
will be marked with things running on your FPGA board.

In Windows, go to **Start** --> **Nios V Command Shell** and run the following commands on your laptop:

    make DE10-Lite
    make GDB_SERVER

(alternatively, use `make DE1-SoC`)

The first command transmits a Nios V computer system to your DE10-Lite board.
The second command will start a program on your laptop that you need to leave running -- it
continuously communicates between your Nios V computer system and your laptop.
If you exit the GDB server by typing Q, the commands below will not longer work.
Sometimes things get confused and you need to stop everything, including the GDB server,
and start over. Sometimes the GDB server can't find the JTAG server to start -- try starting Quartus
and then re-run the command.

Again, go to **Start** --> **Nios V Command Shell** and run the following commands on your laptop:

    make TERMINAL

This launches a program to listen and display any characters sent by your Nios V computer over the JTAG UART.
Leave this program running to capture everything printed by your program. Control-C will exit.

You can merge windows by dragging the tab for the Nios V Command Shell window over the tab from the first window.

Again, go to **Start** --> **Nios V Command Shell** and run the following commands on your laptop:

    make COMPILE
    make RUN

The first command compiles your program, `vga.c` into an object file,
`vga.c.o`, and then linking it into an executable, `vga.elf`.  The second
command transfers the `vga.elf` executable to your Nios V computer and runs it.

This Nios V program will print the message "start" to the TERMINAL window you started earlier, then it will
fill the VGA screen with colour bar pattern, then it will print the message "done".

You can edit your vga.c program, save it, then run `make COMPILE` and `make RUN` again.
Hint: the easiest way to edit is with `notepad vga.c`, but you can also right-click the
file and use notepad++ or launch WSL and run vim.

### Switching between CPUlator (DE1-SoC) and DE10-Lite

If you have a DE1-SoC, or are running in CPUlator, modify the beginning of
`address_map_niosv.h` so it reads `#define DE10LITE 0`. If you switch from
CPUlator back to DE10-Lite hardware, be sure to change the 0 back to a 1.

## Tech Stack 🛠️

Embedded C programmming on RISC-V

# Contact 📬

Created by Owen Anderson on Nov 26, 2025

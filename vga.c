#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 * This file provides address values that exist in the DE10-Lite Computer
 * This file also works for DE1-SoC, except change #define DE10LITE to 0
 ******************************************************************************/

#ifndef __SYSTEM_INFO__
#define __SYSTEM_INFO__

#define DE10LITE 1 // change to 0 for CPUlator or DE1-SoC, 1 for DE10-Lite

/* do not change anything after this line */

#if DE10LITE
 #define BOARD				"DE10-Lite"
 #define MAX_X		160
 #define MAX_Y		120
 #define YSHIFT		  8
#else
 #define MAX_X		320
 #define MAX_Y		240
 #define YSHIFT		  9
#endif



/* Memory */
#define SDRAM_BASE			0x00000000
#define SDRAM_END			0x03FFFFFF
#define FPGA_PIXEL_BUF_BASE		0x08000000
#define FPGA_PIXEL_BUF_END		0x0800FFFF
#define FPGA_CHAR_BASE			0x09000000
#define FPGA_CHAR_END			0x09001FFF

/* Devices */
#define LED_BASE			0xFF200000
#define LEDR_BASE			0xFF200000
#define HEX3_HEX0_BASE			0xFF200020
#define HEX5_HEX4_BASE			0xFF200030
#define SW_BASE				0xFF200040
#define KEY_BASE			0xFF200050
#define JP1_BASE			0xFF200060
#define ARDUINO_GPIO			0xFF200100
#define ARDUINO_RESET_N			0xFF200110
#define JTAG_UART_BASE			0xFF201000
#define TIMER_BASE			0xFF202000
#define TIMER_2_BASE			0xFF202020
#define MTIMER_BASE			0xFF202100
#define RGB_RESAMPLER_BASE    		0xFF203010
#define PIXEL_BUF_CTRL_BASE		0xFF203020
#define CHAR_BUF_CTRL_BASE		0xFF203030
#define ADC_BASE			0xFF204000
#define ACCELEROMETER_BASE		0xFF204020

/* Nios V memory-mapped registers */
#define MTIME_BASE             		0xFF202100
#define	CLOCK_RATE				500000

#endif
	
//// FUNCTIONS

static void handler(void) __attribute__ ((interrupt ("machine")));
void set_mtimer( volatile uint32_t *time_ptr, uint64_t new_time64 );
void set_itimer(void);
void set_KEY(void);
void SWI_ISR(void);
void mtimer_ISR(void);
void itimer_ISR(void);
void KEY_ISR(void);

//// POINTERS TO BOARD

volatile int *SW_ptr = (int *)SW_BASE;
volatile int *KEY_ptr = (int *) KEY_BASE;
volatile uint32_t *hex_grn = (uint32_t *)HEX3_HEX0_BASE;
volatile uint32_t *hex_red = (uint32_t *)HEX5_HEX4_BASE;
volatile int *LEDR_ptr = (int *) LEDR_BASE;

////// KEY SETUP

void set_KEY(void){
	volatile int *KEY_ptr = (int *) KEY_BASE;
	*(KEY_ptr + 3) = 0xF; // clear EdgeCapture register
	*(KEY_ptr + 2) = 0xF; // enable interrupts for all KEYs
}

////// MTIME SETUP

void set_mtimer( volatile uint32_t *time_ptr, uint64_t new_time64 )
{
	*(time_ptr+0) = (uint32_t)0;
	// prevent hi from increasing before setting lo
	*(time_ptr+1) = (uint32_t)(new_time64>>32);
	// set hi part
	*(time_ptr+0) = (uint32_t)new_time64;
	// set lo part
}

uint64_t get_mtimer( volatile uint32_t *time_ptr)
{
	uint32_t mtime_h, mtime_l;
	// can only read 32b at a time
	// hi part may increment between reading hi and lo
	// if it increments, re-read lo and hi again
	do {
	mtime_h = *(time_ptr+1); // read mtime-hi
	mtime_l = *(time_ptr+0); // read mtime-lo
	} while( mtime_h != *(time_ptr+1) );
	// if mtime-hi has changed, repeat
	// return 64b result
	return ((uint64_t)mtime_h << 32) | mtime_l ;
}

volatile uint32_t *mtime_ptr = (uint32_t *) MTIMER_BASE;
const uint64_t PERIOD = (uint64_t)CLOCK_RATE; // 100M

void setup_mtimecmp()
{
	uint64_t mtime64 = get_mtimer( mtime_ptr );
	// read current mtime (counter)
	mtime64 = (mtime64/PERIOD+1) * PERIOD;
	// compute end of next time PERIOD
	set_mtimer( mtime_ptr+2, mtime64 );
	// write first mtimecmp ("+2" == mtimecmp)
}

/////// HEX FUNCTIONS

uint32_t hex_encode[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};

void show_score_grn(int score) {
    *hex_grn = hex_encode[score];
}

void show_score_red(int score) {
    *hex_red = hex_encode[score];
}

///// DELAY FUNCTION

void delay(uint64_t ticks)
{
	volatile int *SW_ptr = (int *)SW_BASE;
    volatile uint64_t *MMTIME = (uint64_t*)MTIME_BASE;
    uint64_t nownow = *MMTIME;
	
	
	ticks = ((ticks * (*SW_ptr) == 0) ? ticks*8 : ticks * (*SW_ptr))*2;

    while ((*MMTIME - nownow) < ticks) {
	//pauses here
	}
}

////// PIXEL FUNCTIONS

typedef uint16_t pixel_t;

volatile pixel_t *pVGA = (pixel_t *)FPGA_PIXEL_BUF_BASE;

const pixel_t blk = 0x0000;
const pixel_t wht = 0xffff;
const pixel_t red = 0xf800;
const pixel_t grn = 0x07e0;
const pixel_t blu = 0x001f;

void drawPixel( int y, int x, pixel_t colour )
{
	*(pVGA + (y<<YSHIFT) + x ) = colour;
}

pixel_t makePixel( uint8_t r8, uint8_t g8, uint8_t b8 )
{
	// inputs: 8b of each: red, green, blue
	const uint16_t r5 = (r8 & 0xf8)>>3; // keep 5b red
	const uint16_t g6 = (g8 & 0xfc)>>2; // keep 6b green
	const uint16_t b5 = (b8 & 0xf8)>>3; // keep 5b blue
	return (pixel_t)( (r5<<11) | (g6<<5) | b5 );
}

void rect( int y1, int y2, int x1, int x2, pixel_t c )
{
	for( int y=y1; y<y2; y++ )
		for( int x=x1; x<x2; x++ )
			drawPixel( y, x, c );
}

////// GLOBAL VARIABLES

int redWin = 0;
int grnWin = 0;
int reset = 1;


int redScr = 0;
int grnScr = 0;

int direction = 1;
int direction2 = 1;	

int y = MAX_Y/2;
int x = MAX_X/3;

int y2 = MAX_Y/2;
int x2 = MAX_X-MAX_X/3;

volatile int pending_left = 0;
volatile int pending_right = 0;

/////// ISRS

void key_ISR(void)
{
	volatile int *KEY_ptr = (int *) KEY_BASE;
	uint32_t edge = *(KEY_ptr + 3);
    if (edge == 0) {
        return;
    }

    if (edge & 0x2) {
        if (pending_left == 1) {
            pending_left = 0;
        } 
		else {
            pending_left = 1;
            pending_right = 0;
        }
    }

    if (edge & 0x1) {
        if (pending_right == 1) {
            pending_right = 0;
        } 
		else {
            pending_right = 1;
            pending_left = 0;
        }
    }
    *(KEY_ptr + 3) = edge;
}

/////

void mtimer_ISR(void)
{
	SW_ptr = (int *)SW_BASE;
	
	uint64_t mtimecmp64 = get_mtimer( mtime_ptr+2 );
	// read mtimecmp
	
	mtimecmp64 = (PERIOD * (*SW_ptr) == 0) ? PERIOD*8 + mtimecmp64 : PERIOD * (*SW_ptr) + mtimecmp64;
	// time of future irq = one period in future
	set_mtimer( mtime_ptr+2, mtimecmp64 );
	// write next mtimecmp
	
	if (reset) return;
	
	// KEY_ISR CONSUME
	
	if (pending_left != 0 || pending_right != 0) {		
        if (pending_left == 1) {
            direction++;
        } else if (pending_right == 1) {
            direction--;
        }
        pending_left = 0;
        pending_right = 0;	
    }

	////// PLAYER 1 CONTROLS

	if(direction > 4)
		direction = 1;
	else if(direction < 1)
		direction = 4;

	if (direction == 1) {
		y++;
		//down
	}
	else if (direction == 2) {
		x++;
		//right
	}
	else if (direction == 3) {
		y--;
		//up
	}
	else if (direction == 4) {
		x--;
		//left
	}
	
	//// PLAYER 2 CONTROLS
	
	if(direction2 > 4)
		direction2 = 1;
	else if(direction2 < 1)
		direction2 = 4;

	int randMove = rand() % (165);


	if(randMove == 0){
		int randDir = rand() % (2);

		pixel_t checkUp = *(pVGA + ((y2-1) << YSHIFT) + x2);
		pixel_t checkLeft = *(pVGA + (y2 << YSHIFT) + (x2-1));
		pixel_t checkRight = *(pVGA + (y2 << YSHIFT) + (x2+1));
		pixel_t checkDown = *(pVGA + ((y2+1) << YSHIFT) + x2);

		pixel_t checkUp2 = *(pVGA + ((y2-2) << YSHIFT) + x2);
		pixel_t checkLeft2 = *(pVGA + (y2 << YSHIFT) + (x2-2));
		pixel_t checkRight2 = *(pVGA + (y2 << YSHIFT) + (x2+2));
		pixel_t checkDown2 = *(pVGA + ((y2+2) << YSHIFT) + x2);


		if(randDir == 0){
			if(direction2 == 1 && checkRight == blk && checkRight2 == blk)
				direction2++;
			else if(direction2 == 2 && checkUp == blk && checkUp2 == blk)
				direction2++;
			else if(direction2 == 3 && checkLeft == blk && checkLeft2 == blk)
				direction2++;
			else if(direction2 == 4 && checkDown == blk && checkDown2 == blk)
				direction2++;
		}
		else if(randDir == 1){
			if(direction2 == 1 && checkLeft == blk && checkLeft2 == blk)
				direction2--;
			else if(direction2 == 2 && checkDown == blk && checkDown2 == blk)
				direction2--;
			else if(direction2 == 3 && checkRight == blk && checkRight2 == blk)
				direction2--;
			else if(direction2 == 4 && checkUp == blk && checkUp2 == blk)
				direction2--;
		}

	}

	if(direction2 > 4)
		direction2 = 1;
	else if(direction2 < 1)
		direction2 = 4;

	if (direction2 == 1) {
		//down

		pixel_t checkFront = *(pVGA + ((y2+2) << YSHIFT) + x2);
		pixel_t checkLeft = *(pVGA + ((y2) << YSHIFT) + (x2+2));
		pixel_t checkRight = *(pVGA + ((y2) << YSHIFT) + (x2-2));

		if(checkFront != blk && checkLeft != blk && checkRight != blk) {}
		else if(checkFront != blk && checkLeft != blk) {
			direction2--;
			x2--;
		}
		else if (checkFront != blk){
			direction2++;
			x2++;
		}
		else
			y2++;		
	}

	else if (direction2 == 2) {
		//right

		pixel_t checkFront = *(pVGA + ((y2) << YSHIFT) + (x2+2));
		pixel_t checkLeft = *(pVGA + ((y2-2) << YSHIFT) + (x2));
		pixel_t checkRight = *(pVGA + ((y2+2) << YSHIFT) + (x2));

		if(checkFront != blk && checkLeft != blk && checkRight != blk) {}
		else if(checkFront != blk && checkLeft != blk) {
			direction2--;
			y2++;
		}
		else if (checkFront != blk){
			direction2++;
			y2--;

		}
		else
			x2++;
	}

	else if (direction2 == 3) {

		//up

		pixel_t checkFront = *(pVGA + ((y2-2) << YSHIFT) + (x2));
		pixel_t checkLeft = *(pVGA + ((y2) << YSHIFT) + (x2-2));
		pixel_t checkRight = *(pVGA + ((y2) << YSHIFT) + (x2+2));

		if(checkFront != blk && checkLeft != blk && checkRight != blk) {}
		else if(checkFront != blk && checkLeft != blk) {
			direction2--;
			x2++;
		}
		else if (checkFront != blk){
			direction2++;
			x2--;
		}
		else
			y2--;
	}

	else if (direction2 == 4) {

		//left

		pixel_t checkFront = *(pVGA + ((y2) << YSHIFT) + (x2-2));
		pixel_t checkLeft = *(pVGA + ((y2+2) << YSHIFT) + (x2));
		pixel_t checkRight = *(pVGA + ((y2-2) << YSHIFT) + (x2));

		if(checkFront != blk && checkLeft != blk && checkRight != blk) {}
		else if(checkFront != blk && checkLeft != blk) {
			direction2--;
			y2--;
		}
		else if (checkFront != blk){
			direction2++;
			y2++;
		}
		else
			x2--;
	}
	
	//////// PIXEL DRAWING
	
	pixel_t color = *(pVGA + (y << YSHIFT) + x);
	pixel_t color2 = *(pVGA + (y2 << YSHIFT) + x2);

	if (color != blk && color2 != blk){
		reset = 1;
		return;
	}
	else if (color != blk) {
		grnWin = 1;
		reset = 1;
		return;
	}
	else if (color2 != blk) {
		redWin = 1;
		reset = 1;
		return;
	}

	////// PLAYER 1

	drawPixel(y, x, red);

	////// PLAYER 2

	drawPixel(y2, x2, grn);
	
}


////////// CPU HANDLER

// this attribute tells compiler to use "mret" rather than "ret"
// at the end of handler() function; also declares its type
static void handler(void) __attribute__ ((interrupt ("machine")));
void handler(void)
{
	int mcause_value;
	// inline assembly, links register %0 to mcause_value
	__asm__ volatile( "csrr %0, mcause" : "=r"(mcause_value) );
	if (mcause_value == 0x80000007) // machine timer
		mtimer_ISR();
	else if (mcause_value == 0x80000012) // machine external interrupt (from PIO)
        key_ISR();
}

void setup_cpu_irqs( uint32_t new_mie_value )
{
	uint32_t mstatus_value, mtvec_value, old_mie_value;
	mstatus_value = 0b1000; // interrupt bit mask
	mtvec_value = (uint32_t) &handler; // set trap address
	__asm__ volatile ("csrc mstatus, %0" :: "r"(mstatus_value));
	// master irq disable
	__asm__ volatile( "csrw mtvec, %0" :: "r"(mtvec_value) );
	// sets handler
	__asm__ volatile( "csrr %0, mie" : "=r"(old_mie_value) );
	__asm__ volatile( "csrc mie, %0" :: "r"(old_mie_value) );
	__asm__ volatile( "csrs mie, %0" :: "r"(new_mie_value) );
	// reads old irq mask, removes old irqs, sets new irq mask
	__asm__ volatile( "csrs mstatus, %0" :: "r"(mstatus_value) );
	// master irq enable
}

///// MAIN

int main(void)
{
	setup_mtimecmp();
	setup_cpu_irqs( 0x50088 ); // enable mtimer IRQs
	set_KEY();
	
	/////////
	
	srand(*(volatile uint64_t*)MTIME_BASE);	
	
	/////////
	
	printf("start\n");
	
	show_score_grn(grnScr);
	show_score_red(redScr);
	
	////// GAME RESET
	
	while (grnScr < 9 && redScr < 9) {
		
        uint32_t display = (pending_right ? 1 : 0) | (pending_left ? 2 : 0);
        *LEDR_ptr = display;
		
		if(reset == 1){
			if(redWin){
				redScr++;
				rect(0, MAX_Y, 0, MAX_X, red);
				printf(" --Red Wins-- \n");
			}
			if(grnWin){
				grnScr++;
				rect(0, MAX_Y, 0, MAX_X, grn);
				printf(" --Green Wins-- \n");
			}
			
			show_score_grn(grnScr);
			show_score_red(redScr);
			
			if(grnScr < 9 && redScr < 9){

				redWin = 0;
				grnWin = 0;

				delay(2000000);

				rect( 0, MAX_Y, 0, MAX_X, wht );
				rect( 5, MAX_Y-5, 5, MAX_X-5, blk );


				for(int j = 0; j < 12; j++){

					int randMinx = rand() % (MAX_X + 1);
					int randMiny = rand() % (MAX_Y + 1);

					int randMaxx = rand() % (MAX_X + 1);
					int randMaxy = rand() % (MAX_Y + 1);

					rect(randMiny, randMaxy, randMinx, randMaxx, wht);

					delay(200000);

				}


				y = MAX_Y/2;
				x = MAX_X/3;

				y2 = MAX_Y/2;
				x2 = MAX_X-MAX_X/3;

				rect(y-30, y+50, x-30, x+30, blk);
				rect(y2-30, y2+50, x2-30, x2+30, blk);

				rect(MAX_Y/2-15, MAX_Y/2+15, MAX_X/3, MAX_X-MAX_X/3, blk);

				drawPixel(y, x, red);
				drawPixel(y2, x2, grn);


				delay(2000000);

				/////////

				direction = 1;
				direction2 = 1;	

				/////////

				drawPixel(y, x, blk);
				drawPixel(y2, x2, blk);

				reset = 0;
			}
		}
	}
	
	if(grnScr > redScr){
		rect(0, MAX_Y, 0, MAX_X, grn);
		printf(" --GREEN WINS THE GAME-- ");
	}
	if(redScr > grnScr){
		rect(0, MAX_Y, 0, MAX_X, red);
		printf(" --RED WINS THE GAME-- ");
	}
}
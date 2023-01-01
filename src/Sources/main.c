/**
 * @file main.c
 * @author Tadeáš Kachyòa <xkachy00@stud.fit.vutbr.cz>
 * @brief simple implementation of running text on a matrix display using multiplexing and LPTMR
 * @version 1
 * @date 2022-12-16
 *
 */

#include "MK60D10.h"

/* Macros for bit-level registers manipulation */
#define GPIO_PIN_MASK	0x1Fu
#define GPIO_PIN(x)		(((1)<<(x & GPIO_PIN_MASK)))

/* Buttons masks */
#define BTN_SW2 0x400     // Port E, bit 10
#define BTN_SW3 0x1000    // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800     // Port E, bit 11

/* Definitions of individual letters and their decimal representation*/
#define LETTER_A 127,136,136,136,127
#define LETTER_B 118,137,137,137,255
#define LETTER_C 128,128,128,128,255
#define LETTER_D 126,129,129,129,255
#define LETTER_E 129,137,137,137,255
#define LETTER_F 128,136,136,136,255
#define LETTER_G 239,137,137,129,255
#define LETTER_H 255,8,8,8,255
#define LETTER_i 255
#define LETTER_J 254,129,129,129,130
#define LETTER_K 255,24,36,64,129
#define LETTER_L 255,1,1,1,1
#define LETTER_M 255,64,48,64,255
#define LETTER_N 255,12,48,64,255
#define LETTER_O 255,129,129,129,255
#define LETTER_P 255,136,136,136,248
#define LETTER_Q 255,129,133,131,255
#define LETTER_R 255,136,140,138,249
#define LETTER_S 249,137,137,137,143
#define LETTER_T 128,128,255,128,128
#define LETTER_U 255,1,1,1,255
#define LETTER_V 248,6,1,6,248
#define LETTER_W 254,1,127,1,254
#define LETTER_X 231,24,24,24,231
#define LETTER_Y 224,16,15,16,224
#define LETTER_Z 131,133,137,145,255
#define NUM_8 126,145,145,145,126
#define ARROW 16,16,16,84,56,16

/* Constants specifying delay loop duration */
int	tdelay1		=	500;
int tdelay2 	=	10;

/* Enable/disable shift of columns */
int shift_col   =   0;

/* Enable/disable buttons */
int button_sw2  =   0;
int button_sw3  =   0;
int button_sw4  =   0;
int button_sw5  =   0;
int button_sw6  =   1;

/* Start to render from column 0 */
int from_start  =   1;

/* Variable delay loop */
void delay(int t1, int t2) {
	int i, j;
	for(i=0; i<t1; i++) {
		for(j=0; j<t2; j++);
	}
}

/**
 * @brief configuration of the necessary MCU peripherals
 * more details in program's documentation
 */
void MCUInit() {
	/* Turn on all port clocks */
	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;

	/* Set corresponding PTA pins (column activators of 74HC154) for GPIO functionality */
	PORTA->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );  // A0
	PORTA->PCR[10] = ( 0|PORT_PCR_MUX(0x01) ); // A1
	PORTA->PCR[6] = ( 0|PORT_PCR_MUX(0x01) );  // A2
	PORTA->PCR[11] = ( 0|PORT_PCR_MUX(0x01) ); // A3

	/* Set corresponding PTA pins (rows selectors of 74HC154) for GPIO functionality */
	PORTA->PCR[26] = ( 0|PORT_PCR_MUX(0x01) );  // R0
	PORTA->PCR[24] = ( 0|PORT_PCR_MUX(0x01) );  // R1
	PORTA->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );   // R2
	PORTA->PCR[25] = ( 0|PORT_PCR_MUX(0x01) );  // R3
	PORTA->PCR[28] = ( 0|PORT_PCR_MUX(0x01) );  // R4
	PORTA->PCR[7] = ( 0|PORT_PCR_MUX(0x01) );   // R5
	PORTA->PCR[27] = ( 0|PORT_PCR_MUX(0x01) );  // R6
	PORTA->PCR[29] = ( 0|PORT_PCR_MUX(0x01) );  // R7

	/* Set corresponding PTE pins (output enable of 74HC154) for GPIO functionality */
	PORTE->PCR[28] = ( 0|PORT_PCR_MUX(0x01) ); // #EN

	/* Change corresponding PTA port pins as outputs */
	PTA->PDDR = GPIO_PDDR_PDD(0x3F000FC0);

	/* Change corresponding PTE port pins as outputs */
	PTE->PDDR = GPIO_PDDR_PDD( GPIO_PIN(28) );

	MCG->C4 |= (MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01)); /* Set the clock subsystem */
	SIM->CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);

	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;
	uint8_t i;
	int buttons[5] = {10,11,12,26,27};
	for(i=0; i<5; i++) {
			PORTE->PCR[buttons[i]] = ( PORT_PCR_ISF(0x01) /* Reset ISF (Interrupt Status Flag) */
						| PORT_PCR_IRQC(0x0A) /* Interrupt enable on failing edge */
						| PORT_PCR_MUX(0x01) /* Pin Mux Control to GPIO */
						| PORT_PCR_PE(0x01) /* Pull resistor enable... */
						| PORT_PCR_PS(0x01)); /* ...select Pull-Up */
	}
	NVIC_ClearPendingIRQ(PORTE_IRQn); /* Reset the interrupt signal from the port E */
	NVIC_EnableIRQ(PORTE_IRQn);       /* Enable interrupt from port E */

	SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK; // Enable clock to LPTMR
	LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;   // Turn OFF LPTMR to perform setup
	LPTMR0_PSR = ( LPTMR_PSR_PRESCALE(0) // 0000 is div 2
				 | LPTMR_PSR_PBYP_MASK   // LPO feeds directly to LPT
				 | LPTMR_PSR_PCS(1)) ;   // use the choice of clock
	LPTMR0_CMR = 0x35;                   // Set compare value
	LPTMR0_CSR =(  LPTMR_CSR_TCF_MASK    // Clear any pending interrupt (now)
				 | LPTMR_CSR_TIE_MASK    // LPT interrupt enabled
				);
	NVIC_EnableIRQ(LPTMR0_IRQn);         // enable interrupts from LPTMR0
	LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;    // Turn ON LPTMR0 and start counting
}

/**
 * @brief function implementing and handling timer interruptions
 *
 */
void LPTMR0_IRQHandler(void)
{
    // Set new compare value set by up/down buttons
    LPTMR0_CMR = 0x10;                // !! the CMR reg. may only be changed while TCF == 1
    LPTMR0_CSR |=  LPTMR_CSR_TCF_MASK;   // writing 1 to TCF tclear the flag
    shift_col = 0;
}

/**
 * @brief function implementing and handling button interruptions
 *
 */
void PORTE_IRQHandler(void) {
	// disables current running text on the display
	button_sw2 = 0;
	button_sw3 = 0;
	button_sw4 = 0;
	button_sw5 = 0;
	button_sw6 = 0;
	from_start = 1;

	if (PORTE->ISFR & BTN_SW2) {
		button_sw2 = 1;

	} else if (PORTE->ISFR & BTN_SW3) {
		button_sw3 = 1;

	} else if (PORTE->ISFR & BTN_SW4) {
		button_sw4 = 1;

	} else if (PORTE->ISFR & BTN_SW5) {
		button_sw5 = 1;

	} else if (PORTE->ISFR & BTN_SW6) {
		button_sw6 = 1;

	}

	// cleaning interrupts
	PORTE->ISFR = BTN_SW2 | BTN_SW3 | BTN_SW4 | BTN_SW5 | BTN_SW6;
}

/**
 *
 * @brief conversion of requested column number into the 4-to-16 decoder control.
 *
 * @param col_num number of column in decimal
 */
void column_select(unsigned int col_num) {
	unsigned i, result, col_sel[4];

	for (i =0; i<4; i++) {
		result = col_num / 2;  // Whole-number division of the input number
		col_sel[i] = col_num % 2;
		col_num = result;

		switch(i) {

		    case 0:  // Selection signal A0
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(8))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(8)));
				break;

			case 1:  // Selection signal A1
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(10))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(10)));
				break;

			case 2:  // Selection signal A2
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(6))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(6)));
				break;

			case 3: // Selection signal A3
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(11))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(11)));
				break;

			default:
				break;
		}
	}
}

/**
 * @brief conversion of requested rows number in decimal into binary form where number 1 indicates row to be turn on
 *
 * @param rows number in decimal in range from 0 to 255
 */
void rows_selects(unsigned int row)
{
	unsigned i, result, row_sel[8];

	for (i = 0; i < 8; i++) {
		result = row / 2;
		row_sel[i] = row % 2;
		row = result;


		switch(i) {

				case 0:  // Selection signal R0
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(26))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(26)));
					break;

				case 1:  // Selection signal R1
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(24))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(24)));
					break;

				case 2:  // Selection signal R2
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(9))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(9)));
					break;

				case 3:  // Selection signal R3
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(25))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(25)));
					break;

				case 4:  // Selection signal R4
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(28))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(28)));
					break;

				case 5:  // Selection signal R5
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(7))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(7)));
					break;

				case 6:  // Selection signal R6
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(27))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(27)));
					break;

				case 7:  // Selection signal R7
					((row_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(29))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(29)));
					break;
		}
	}
}

/**
 * @brief function to display text on the led display
 *
 * @param column number of column
 * @param text text to be displayed
 */
void display(int column, int text[], int size_of_the_text) {

	// this function runs every time when the text should be shifted
	for (int shift = 0; shift < size_of_the_text; shift++) {

		// ensuring the borders so that the text does not overflow from left to right
		// constant 16 represents the size of the display ( == number of physical columns)
		if (column >= shift && column < 16 + shift) {
			column_select(column - shift);
			rows_selects(text[shift]);
			delay(tdelay1, tdelay2);
		}
	}
}


/**
 *  @brief main function
 */
int main(void)
{
	MCUInit();

	PTA->PDOR |= GPIO_PDOR_PDO(0x3F000280); // turning the pixels of a particular row ON
	delay(tdelay1, tdelay2);

	int i = 0; // represents columns

    for (;;) {

    	// start rendering from the first column
    	if (i == 150) { // 150 is +- the size of the longest text
    		i = 0;
    	}

    	// start rendering from the first column
    	if (from_start == 1) {
    		i = 0;
    		from_start = 0;
    	}

    	/* Predefined texts, composed from letters defined in the beginning of this source code, 0 represents spaces */
    	int text_hello_world[] = {255,8,8,8,255, 0  , 255,137,137,137,129, 0 , 255,1,1,1,1, 0, 255,1,1,1,1, 0, 126,129,129,129,126, 0, 0, 0, 254,1,127,1,254, 0, 126,129,129,129,126, 0, 255,136,140,138,249, 0, 255,1,1,1,1, 0,  255,129,129,129,126};
    	int text_imp_project[] = {255, 0, 255,64,48,64,255, 0, 255,136,136,136,248, 0, 0, 0, 255,136,136,136,248, 0, 255,136,140,138,249, 0, 126,129,129,129,126, 0, 130,129,129,129,254, 0, 255,137,137,137,129, 0,  126,129,129,129,129, 0, 128,128,255,128,128};
    	int text_vut_fit_2022[] = {248,6,1,6,248, 0, 254,1,1,1,254, 0, 128,128,255,128,128, 0, 0, 0, 255,136,136,136,128, 0, 255, 0, 128,128,255,128,128, 0};
    	int text_merry_xmas[] = {255,64,48,64,255, 0, 255,137,137,137,129, 0, 255,136,140,138,249, 0, 255,136,140,138,249, 0, 224,16,15,16,224, 0, 0, 0, 231,24,24,24,231, 0, 255,64,48,64,255, 0, 127,136,136,136,127, 0, 249,137,137,137,143  };
    	int text_tram[] = {16,16,16,84,56,16, 0, 255,145,145,145,255, 0, 1, 0, 1, 0, 1, 0, 1, 0, 255,64,48,12,255, 0, 255,137,137,137,129,   0, 255,64,48,64,255, 0, 126,129,129,129,126, 0, 126,129,129,129,129, 0, 255,64,48,12,255, 0, 255, 0, 126,129,129,129,129, 0, 255,137,137,137,129, 0,0,0, 255,137,137,137,118, 0, 126,129,129,129,126, 0, 255,8,8,8,255, 0,  254,1,1,1,254, 0, 255,64,48,12,255, 0, 255, 0, 126,129,129,129,129, 0, 255,137,137,137,129    };
    	int text_tadeas[] = {128,128,255,128,128, 0, 127,136,136,136,127, 0, 255,129,129,129,126, 0, 255,137,137,137,129, 0, 127,136,136,136,127, 0, 249,137,137,137,143  };
    	int text_bobanek[] = {255,137,137,137,118, 0, 126,129,129,129,126, 0, 255,137,137,137,118, 0, 127,136,136,136,127, 0, 255,64,48,12,255, 0, 255,137,137,137,129, 0, 255,24,36,66,129, 0, 120,132,130,65,130,132,120    };


    	if (shift_col == 0) { // when interrupt from LPTMR timer is triggered

			if (button_sw2 == 1) {
				for (int j = 0; j < 10; j++){ // this loop ensures the visibility of the text
					int size_of_the_text = sizeof(text_imp_project) / sizeof(int);
					display(i, text_imp_project, size_of_the_text);
				}
			}

    		if (button_sw3 == 1){
				for (int j = 0; j < 10; j++){ // this loop ensures the visibility of the text
					int size_of_the_text = sizeof(text_vut_fit_2022) / sizeof(int);
					display(i, text_vut_fit_2022, size_of_the_text);
				}
    		}

    		if (button_sw4 == 1){
				for (int j = 0; j < 10; j++){ // ==||==
					int size_of_the_text = sizeof(text_tram) / sizeof(int);
					display(i, text_tram, size_of_the_text);
				}
    		}

    		if (button_sw5 == 1){
				for (int j = 0; j < 20; j++){ // ==||==
					int size_of_the_text = sizeof(text_bobanek) / sizeof(int);
					display(i, text_bobanek, size_of_the_text);
				}
			 }

    		if (button_sw6 == 1){
				for (int j = 0; j < 10; j++){ // ==||==
					int size_of_the_text = sizeof(text_hello_world) / sizeof(int);
					display(i, text_hello_world, size_of_the_text);
			}
		}

		i++;
		shift_col = 1;

    	}

    	rows_selects(0);
    }

    return 0;
}

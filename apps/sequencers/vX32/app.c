/*
vX32 pre-alpha
not for any use whatsoever
copyright stryd_one
bite me corp 2008

big props to nILS for being my fourth eye and TK for obvious reasons
*/


/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include "app.h"

#include "graph.h"
#include "mclock.h"
#include "modules.h"

#include <FreeRTOS.h>
#include <portmacro.h>

#if 0
// TK: not used and therefore disabled, as it clashes with CLCDs
#include <glcd_font.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Global Variables
/////////////////////////////////////////////////////////////////////////////
unsigned char testmodule1; //FIXME TESTING
unsigned char testmodule2; //FIXME TESTING
edge_t *testedge1; //FIXME TESTING
/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void) {
MIOS32_DELAY_Init(0);
	MIOS32_BOARD_LED_Init(0xffffffff); // initialize all LEDs
	//MIOS32_BOARD_LED_Set(0xffffffff, 0);
  
#if 0
	// TK: GLCD_FONT_NORMAL not available for CLCDs
	// for GLCDs the "normal font" is selected by default anyhow,
	// therefore this function call can be safely disabled
	MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
#endif
	MIOS32_LCD_BColourSet(0x00, 0x00, 0x00);
	MIOS32_LCD_Clear();

	MIOS32_LCD_CursorSet(0, 1);
	MIOS32_LCD_PrintString("stryd_one");

	MIOS32_LCD_FColourSet(0x00, 0x00, 0xff);
	MIOS32_LCD_CursorSet(2, 2);
	MIOS32_LCD_PrintString("vX0.0");

	graph_init();
	clocks_init();
	
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void) {

	MIOS32_BOARD_LED_Set(0xffffffff, 1); //FIXME TESTING
	mod_tick();				 										// send the output queues
	MIOS32_BOARD_LED_Set(0xffffffff, 0); //FIXME TESTING
	MIOS32_BOARD_LED_Set(0xffffffff, 2); //FIXME TESTING
	mclock_tick();													// check for sclock ticks
	MIOS32_BOARD_LED_Set(0xffffffff, 0); //FIXME TESTING

}


/////////////////////////////////////////////////////////////////////////////
//  This hook is called when a complete MIDI event has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedEvent(mios32_midi_port_t port, mios32_midi_package_t midi_package) {
	// buffer up the incoming events
	// notify each host node by incrementing the process_req flag
	// process all the nodes downstream of the host node
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a SysEx byte has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedSysEx(mios32_midi_port_t port, u8 sysex_byte) {
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a byte has been received via COM interface
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedCOM(mios32_com_port_t port, u8 byte) {
// FIXME TESTING
	if (byte == 't') {
sclock_init(0, 16, 2, 1, 1);

sclock[0].status = 0x80; 
	} else if (byte == 'T'){
sclock_init(1, 16, 2, 3, 1);

sclock[1].status = 0x80; 
	
	} else if (byte == 'g'){
	

	testmodule1 = node_add(0); //FIXME TESTING
	testmodule2 = node_add(1); //FIXME TESTING
	mod_set_clocksource(testmodule1, 0); //FIXME TESTING
	mod_set_clocksource(testmodule2, 1); //FIXME TESTING

	
	} else if (byte == 'p'){
	
	mclock.status = 0x80;
	mclock.ticked++;
	
	} else if (byte == 'n'){
	
	testedge1 = edge_add(testmodule1, 0, testmodule2, 3);
	} else if (byte == 'N'){
	
	edge_add(testmodule2, 5, testmodule1, 5);
	} else if (byte == 'd'){
	
	edge_del(testmodule1, testedge1, 1);
	} else if (byte == 'D'){
	
	node_del(testmodule1);
	} else if (byte == 'b'){
	
	mclock_setbpm(45);
	} else if (byte == 'B'){
	
	mclock_setbpm(180);
	} else if (byte == 'm'){
	
	node_add(1);
	} else if (byte == 'M'){
	
	node_add(0);
	} else {
	MIOS32_COM_SendChar(port, 0x30+testmodule1);
	MIOS32_COM_SendChar(port, 0x30+testmodule2);	
	nodes_proc(dead_nodeid); //FIXME TESTING
	}
	
// FIXME TESTING
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void) {
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void) {
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value) {
	// jump to the CS handler
	// edit a value of one of the modules according to the menus
	// notify modified node by incrementing the process_req flag
	// process all the nodes downstream of the host node
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer) {
	// see APP_DIN_NotifyToggle
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void APP_AIN_NotifyChange(u32 pin, u32 pin_value) {
}

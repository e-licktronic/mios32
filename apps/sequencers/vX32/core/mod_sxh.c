/* $Id:  $ */
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

#include <FreeRTOS.h>
#include <portmacro.h>

#include <mios32.h>

#include "app.h"
#include "graph.h"
#include "mclock.h"
#include "modules.h"
#include "patterns.h"
#include "utils.h"
#include "ui.h"

#include <seq_midi_out.h>



void mod_init_sxh(unsigned char nodeid) {						// initialize a sample and hold module
}

void mod_proc_sxh(unsigned char nodeid) { 						// do stuff with inputs and push to the outputs 
	
}

void mod_uninit_sxh(unsigned char nodeid) { 					// uninitialize a sample and hold module
}

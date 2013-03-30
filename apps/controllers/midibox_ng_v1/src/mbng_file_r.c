// $Id$
//! \defgroup MBNG_FILE_R
//! Config File access functions
//! 
//! NOTE: before accessing the SD Card, the upper level function should
//! synchronize with the SD Card semaphore!
//!   MUTEX_SDCARD_TAKE; // to take the semaphore
//!   MUTEX_SDCARD_GIVE; // to release the semaphore
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2012 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
//! Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <string.h>
#include <uip_task.h>
#include <midi_port.h>

#include "tasks.h"
#include "file.h"
#include "mbng_file.h"
#include "mbng_file_r.h"
#include "mbng_event.h"
#include "mbng_lcd.h"


/////////////////////////////////////////////////////////////////////////////
//! for optional debugging messages via DEBUG_MSG (defined in mios32_config.h)
/////////////////////////////////////////////////////////////////////////////

// Note: verbose level 1 is default - it prints error messages!
#define DEBUG_VERBOSE_LEVEL 1


/////////////////////////////////////////////////////////////////////////////
//! Local definitions
/////////////////////////////////////////////////////////////////////////////

// in which subdirectory of the SD card are the files located?
// use "/" for root
// use "/<dir>/" for a subdirectory in root
// use "/<dir>/<subdir>/" to reach a subdirectory in <dir>, etc..

#define MBNG_FILES_PATH "/"
//#define MBNG_FILES_PATH "/MySongs/"


/////////////////////////////////////////////////////////////////////////////
//! Local types
/////////////////////////////////////////////////////////////////////////////

// file informations stored in RAM
typedef struct {
  unsigned valid: 1;   // file is accessible
} mbng_file_r_info_t;


// script variables
typedef struct {
  s16 value;
  u8 section;
  u8 bank;
} mbng_file_r_var_t;


/////////////////////////////////////////////////////////////////////////////
//! Local prototypes
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//! Local variables
/////////////////////////////////////////////////////////////////////////////

static mbng_file_r_info_t mbng_file_r_info;


/////////////////////////////////////////////////////////////////////////////
//! Global variables
/////////////////////////////////////////////////////////////////////////////
char mbng_file_r_script_name[MBNG_FILE_R_FILENAME_LEN+1];
mbng_file_r_req_t mbng_file_r_req;


/////////////////////////////////////////////////////////////////////////////
//! Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_Init(u32 mode)
{
  // invalidate file info
  MBNG_FILE_R_Unload();

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Loads script
//! Called from MBNG_FILE_RheckSDCard() when the SD card has been connected
//! \returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_Load(char *filename)
{
  s32 error;
  error = MBNG_FILE_R_Read(filename, 0, 0);
#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_R] Tried to open script %s, status: %d\n", filename, error);
#endif

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! Unloads script file
//! Called from MBNG_FILE_RheckSDCard() when the SD card has been disconnected
//! \returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_Unload(void)
{
  mbng_file_r_info.valid = 0;
  mbng_file_r_req.ALL = 0;

  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
//! \Returns 1 if current script file valid
//! \Returns 0 if current script file not valid
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_Valid(void)
{
  return mbng_file_r_info.valid;
}


/////////////////////////////////////////////////////////////////////////////
//! help function which removes the quotes of an argument (e.g. .csv file format)
//! can be cascaded with strtok_r
/////////////////////////////////////////////////////////////////////////////
static char *remove_quotes(char *word)
{
  if( word == NULL )
    return NULL;

  if( *word == '"' )
    ++word;

  int len = strlen(word);
  if( len && word[len-1] == '"' )
    word[len-1] = 0;

  return word;
}

/////////////////////////////////////////////////////////////////////////////
//! help function which parses a decimal or hex value
//! \returns >= 0 if value is valid
//! \returns -1000000000 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 get_dec(char *word)
{
  if( word == NULL )
    return -1000000000;

  char *next;
  long l = strtol(word, &next, 0);

  if( word == next )
    return -1000000000;

  return l; // value is valid
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses an id (xxx:<number>)
//! \returns > 0 if id is valid
//! \returns == 0 if id is invalid
/////////////////////////////////////////////////////////////////////////////
static mbng_event_item_id_t parseId(char *parameter)
{
  char *brkt;
  const char *separator_colon = ":";

  mbng_event_item_id_t id;
  char *value_str;
  if( !(value_str = strtok_r(parameter, separator_colon, &brkt)) ||
      (id=MBNG_EVENT_ItemIdFromControllerStrGet(value_str)) == MBNG_EVENT_CONTROLLER_DISABLED ) {
    return 0;
  }

  int id_lower = 0;
  value_str = brkt;
  if( (id_lower=get_dec(value_str)) < 1 || id_lower > 0xfff ) {
    return 0;
  }

  return id | id_lower;
}


/////////////////////////////////////////////////////////////////////////////
//! help function which determine a value of a condition
//! \returns >= 0 if value is valid
//! \returns -1000000000 if value is invalid
/////////////////////////////////////////////////////////////////////////////
static s32 parseValue(u32 line, char *command, char *value_str, mbng_file_r_var_t *vars)
{
  if( value_str == NULL || value_str[0] == 0 )
    return -1000000000;

  if( value_str[0] == '^' ) {
    if( strcasecmp((char *)&value_str[1], "SECTION") == 0 ) {
      return vars->section;
    } else if( strcasecmp((char *)&value_str[1], "VALUE") == 0 ) {
      return vars->value;
    } else if( strcasecmp((char *)&value_str[1], "BANK") == 0 ) {
      return vars->bank;
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: invalid or unsupported variable '%s' in '%s' command!", line, value_str, command);
#endif
      return -1000000000;
    }
  }

  mbng_event_item_id_t id;
  if( (id=parseId(value_str)) ) {
    // search for items with matching ID
    mbng_event_item_t item;
    u32 continue_ix = 0;
    if( MBNG_EVENT_ItemSearchByHwId(id, &item, &continue_ix) >= 0 ) {
      return item.value;
    } else {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: '%s' not found in event pool by '%s' command!", line, value_str, command);
#endif
      return -1000000000;
    }
  }

  return get_dec(value_str);
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses an IF condition
//! \returns >= 0 if condition is valid
//! \returns < 0 if condition is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_R_Read - this will blow up the stack usage too much!
s32 parseCondition(u32 line, char *command, char **brkt, mbng_file_r_var_t *vars)
{
  const char *separators = " \t";
  char *lvalue_str, *condition_str, *rvalue_str;

  if( !(lvalue_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: missing left value of expression in '%s' command!", line, command);
#endif
    return -1;
  }

  if( !(condition_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: missing condition of expression in '%s' command!", line, command);
#endif
    return -2;
  }

  if( !(rvalue_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: missing right value of expression in '%s' command!", line, command);
#endif
    return -3;
  }

  s32 lvalue = parseValue(line, command, lvalue_str, vars);
  if( lvalue <= -1000000000 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: invalid left value '%s' in '%s' command!", line, lvalue_str, command);
#endif
    return -4;
  }

  s32 rvalue = parseValue(line, command, rvalue_str, vars);
  if( rvalue <= -1000000000 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: invalid right value '%s' in '%s' command!", line, rvalue_str, command);
#endif
    return -5;
  }

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_C:%d] condition: %s %s %s (%d %s %d)\n", line, lvalue_str, condition_str, rvalue_str, lvalue, condition_str, rvalue);
#endif

  if( strcasecmp(condition_str, "==") == 0 ) {
    return lvalue == rvalue;
  } else if( strcasecmp(condition_str, "!=") == 0 ) {
    return lvalue != rvalue;
  } else if( strcasecmp(condition_str, ">=") == 0 ) {
    return lvalue >= rvalue;
  } else if( strcasecmp(condition_str, "<=") == 0 ) {
    return lvalue <= rvalue;
  } else if( strcasecmp(condition_str, ">") == 0 ) {
    return lvalue > rvalue;
  } else if( strcasecmp(condition_str, "<") == 0 ) {
    return lvalue < rvalue;
  }

#if DEBUG_VERBOSE_LEVEL >= 1
  DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: invalid or unsupported condition '%s' in '%s' command!", line, condition_str, command);
#endif

  return -10; // invalid condition
}


/////////////////////////////////////////////////////////////////////////////
//! help function which parses a SEND command
//! \returns >= 0 if condition is valid
//! \returns < 0 if condition is invalid
/////////////////////////////////////////////////////////////////////////////
//static // TK: removed static to avoid inlining in MBNG_FILE_R_Read - this will blow up the stack usage too much!
s32 parseSend(u32 line, char *command, char **brkt, mbng_file_r_var_t *vars)
{
  const char *separators = " \t";

  char *event_str;
  if( !(event_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: missing event type in '%s' command!", line, command);
#endif
    return -1;
  }

  mbng_event_type_t event_type;
  if( (event_type=MBNG_EVENT_ItemTypeFromStrGet(event_str)) == MBNG_EVENT_TYPE_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: unknown event type '%s' in '%s' command!", line, event_str, command);
#endif
    return -1;
  }


  char *port_str;
  if( !(port_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: missing MIDI port in '%s' command!", line, command);
#endif
    return -1;
  }

  mios32_midi_port_t port;
  {
    int out_port = 0xff;
    int port_ix;

    for(port_ix=0; port_ix<MIDI_PORT_OutNumGet(); ++port_ix) {
      // terminate port name at first space
      char port_name[10];
      strcpy(port_name, MIDI_PORT_OutNameGet(port_ix));
      int i; for(i=0; i<strlen(port_name); ++i) if( port_name[i] == ' ' ) port_name[i] = 0;
    
      if( strcasecmp(port_str, port_name) == 0 ) {
	out_port = MIDI_PORT_OutPortGet(port_ix);
	break;
      }
    }
    
    if( out_port == 0xff && ((out_port=parseValue(line, command, port_str, vars)) < 0 || out_port > 0xff) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_C:%d] ERROR: invalid MIDI port '%s' in '%s' command!", line, port_str, command);
#endif
      return -1;
    }

    port = (mios32_midi_port_t)out_port;
  }



  int num_values = 0;
#define STREAM_MAX_SIZE 256
  u8 stream[STREAM_MAX_SIZE];
  int stream_size = 0;

  switch( event_type ) {
  case MBNG_EVENT_TYPE_NOTE_OFF:       num_values = 3; break;
  case MBNG_EVENT_TYPE_NOTE_ON:        num_values = 3; break;
  case MBNG_EVENT_TYPE_POLY_PRESSURE:  num_values = 3; break;
  case MBNG_EVENT_TYPE_CC:             num_values = 3; break;
  case MBNG_EVENT_TYPE_PROGRAM_CHANGE: num_values = 2; break;
  case MBNG_EVENT_TYPE_AFTERTOUCH:     num_values = 2; break;
  case MBNG_EVENT_TYPE_PITCHBEND:      num_values = 2; break;
  case MBNG_EVENT_TYPE_NRPN:           num_values = 3; break;

  case MBNG_EVENT_TYPE_SYSEX: { // extra handling
    char *stream_str;
    char *brkt_local = *brkt;
    u8 *stream_pos = (u8 *)&stream[0];
    // check for STREAM_MAX_SIZE-2, since a meta entry allocates up to 3 bytes
    while( stream_size < (STREAM_MAX_SIZE-2) && (stream_str = strtok_r(NULL, separators, &brkt_local)) ) {
      if( *stream_str == '^' ) {
	mbng_event_sysex_var_t sysex_var = MBNG_EVENT_ItemSysExVarFromStrGet((char *)&stream_str[1]);
	if( sysex_var == MBNG_EVENT_SYSEX_VAR_UNDEFINED ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: unknown SysEx variable '%s' in command '%s'\n", line, stream_str, command);
#endif
	  return -1;
	} else {
	  *stream_pos = 0xff; // meta indicator
	  ++stream_pos;
	  ++stream_size;
	  *stream_pos = (u8)sysex_var;
	}
      } else {
	int value;
	if( (value=parseValue(line, command, stream_str, vars)) < 0 || value > 0xff ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	  DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: invalid SysEx value '%s' in command '%s', expecting 0..255 (0x00..0xff)\n", line, stream_str, command);
#endif
	  return -1;
	} else {
	  *stream_pos = (u8)value;
	}
      }

      ++stream_pos;
      ++stream_size;
    }

    if( !stream_size ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing SysEx string in command '%s'\n", line, command);
#endif
      return -1;
    }

  } break;

  case MBNG_EVENT_TYPE_META: {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: sending meta events isn't supported (yet)!", line);
#endif
      return -1;
  }
  }

  s16 values[3];
  int i;
  for(i=0; i<num_values; ++i) {
    char *value_str;
    if( !(value_str = strtok_r(NULL, separators, brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing value for '%s' event in '%s' command!", line, event_str, command);
#endif
      return -1;
    }

    s32 value;
    if( (value=parseValue(line, command, value_str, vars)) < -16384 || value >= 16383 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: invalid value for '%s' event in '%s' command!", line, event_str, command);
#endif
      return -1;
    }

    values[i] = value;
  }

  MUTEX_MIDIOUT_TAKE;
  switch( event_type ) {
  case MBNG_EVENT_TYPE_NOTE_OFF:       MIOS32_MIDI_SendNoteOff(port, (values[0]-1) & 0xf, values[1] & 0x7f, values[2] & 0x7f); break;
  case MBNG_EVENT_TYPE_NOTE_ON:        MIOS32_MIDI_SendNoteOn(port, (values[0]-1) & 0xf, values[1] & 0x7f, values[2] & 0x7f); break;
  case MBNG_EVENT_TYPE_POLY_PRESSURE:  MIOS32_MIDI_SendPolyPressure(port, (values[0]-1) & 0xf, values[1] & 0x7f, values[2] & 0x7f); break;
  case MBNG_EVENT_TYPE_CC:             MIOS32_MIDI_SendCC(port, (values[0]-1), values[1] & 0x7f, values[2] & 0x7f); break;
  case MBNG_EVENT_TYPE_PROGRAM_CHANGE: MIOS32_MIDI_SendProgramChange(port, (values[0]-1) & 0xf, values[1] & 0x7f); break;
  case MBNG_EVENT_TYPE_AFTERTOUCH:     MIOS32_MIDI_SendAftertouch(port, (values[0]-1) & 0xf, values[1] & 0x7f); break;
  case MBNG_EVENT_TYPE_PITCHBEND:      MIOS32_MIDI_SendPitchBend(port, (values[0]-1) & 0xf, (values[1] + 8192) & 0x3fff); break;
  case MBNG_EVENT_TYPE_NRPN:           MBNG_EVENT_SendOptimizedNRPN(port, (values[0]-1) & 0xf, values[1], values[2]); break;

  case MBNG_EVENT_TYPE_SYSEX: {
    if( !stream_size ) {
#if DEBUG_VERBOSE_LEVEL >= 1
      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: unexpected condition for 'send SysEx' - please inform TK!", line);
#endif
      return -1;
    }
    MBNG_EVENT_SendSysExStream(port, stream, stream_size, vars->value);
  } break;

  //case MBNG_EVENT_TYPE_META: // extra handling
  }
  MUTEX_MIDIOUT_GIVE;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! reads the config file content (again)
//! \returns < 0 on errors (error codes are documented in mbng_file.h)
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_Read(char *filename, u8 section, s16 value)
{
  s32 status = 0;
  mbng_file_r_info_t *info = &mbng_file_r_info;
  file_t file;

  info->valid = 0; // will be set to valid if file content has been read successfully

  // store current file name in global variable for UI
  memcpy(mbng_file_r_script_name, filename, MBNG_FILE_R_FILENAME_LEN+1);

  char filepath[MAX_PATH];
  sprintf(filepath, "%s%s.NGR", MBNG_FILES_PATH, mbng_file_r_script_name);

#if DEBUG_VERBOSE_LEVEL >= 2
  DEBUG_MSG("[MBNG_FILE_R] Open config '%s'\n", filepath);
#endif

  if( (status=FILE_ReadOpen(&file, filepath)) < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_R] %s (optional run script) not found\n", filepath);
#endif
    return status;
  }

  // allocate 1024 bytes from heap
  u32 line_buffer_size = 1024;
  char *line_buffer = pvPortMalloc(line_buffer_size);
  u32 line_buffer_len = 0;
  if( !line_buffer ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_R] FATAL: out of heap memory!\n");
#endif
    return -1;
  }

  // read commands
  u8 exit = 0;
  u32 line = 0;
#define IF_MAX_NESTING_LEVEL 16
  u8 nesting_level = 0;
  u8 if_state[IF_MAX_NESTING_LEVEL];
  do {
    ++line;
    status=FILE_ReadLine((u8 *)(line_buffer+line_buffer_len), line_buffer_size-line_buffer_len);

    if( status > 1 ) {
#if DEBUG_VERBOSE_LEVEL >= 3
      if( line_buffer_len )
	MIOS32_MIDI_SendDebugString("+++");
      MIOS32_MIDI_SendDebugString(line_buffer);
#endif

      // concatenate?
      u32 new_len = strlen(line_buffer);
      // remove spaces
      while( new_len >= 1 && line_buffer[new_len-1] == ' ' ) {
	line_buffer[new_len-1] = 0;
	--new_len;
      }
      if( new_len >= 1 && line_buffer[new_len-1] == '\\' ) {
	line_buffer[new_len-1] = 0;
	line_buffer_len = new_len - 1;
	continue; // read next line
      } else {
	line_buffer_len = 0; // for next round we start at 0 again
      }

      // sscanf consumes too much memory, therefore we parse directly
      const char *separators = " \t;";
      char *brkt;
      char *parameter;

      mbng_file_r_var_t vars;
      vars.section = section;
      vars.value = value;
      vars.bank = MBNG_EVENT_SelectedBankGet(); // this can change with each line!

      if( (parameter = remove_quotes(strtok_r(line_buffer, separators, &brkt))) ) {
	
	if( *parameter == 0 || *parameter == '#' ) {
	  // ignore comments and empty lines
	  continue;
	}

	if( strcasecmp(parameter, "IF") == 0 ) {
	  if( nesting_level >= IF_MAX_NESTING_LEVEL ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: max nesting level (%d) for if commands reached!\n", line, IF_MAX_NESTING_LEVEL);
#endif
	    exit = 2; // due to error
	  } else {
	    ++nesting_level;

	    if( nesting_level >= 2 && if_state[nesting_level-2] == 0 ) { // this IF is executed inside a non-matching block
	      if_state[nesting_level-1] = 0;
	    } else {
	      s32 match = parseCondition(line, parameter, &brkt, &vars);
	      if( match < 0 ) {
		exit = 2; // exit due to error
	      } else {
		if_state[nesting_level-1] = match ? 1 : 0;
	      }
	    }
	  }
	  continue; // read next line
	}

	if( strcasecmp(parameter, "ELSEIF") == 0 || strcasecmp(parameter, "ELSIF") == 0 ) {
	  if( nesting_level == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: unexpected %s statement!\n", line, parameter);
#endif
	  } else {
	    if( nesting_level >= 2 && if_state[nesting_level-2] == 0 ) { // this ELSIF is executed inside a non-matching block
	      if_state[nesting_level-1] = 0;
	    } else {
	      if( if_state[nesting_level-1] == 0 ) { // no matching IF condition yet?
		s32 match = parseCondition(line, parameter, &brkt, &vars);
		if( match < 0 ) {
		  exit = 2; // exit due to error
		} else {
		  if_state[nesting_level-1] = match ? 1 : 0;
		}
	      } else {
		if_state[nesting_level-1] = 2; // IF has been processed
	      }
	    }
	  }
	  continue; // read next line
	}

	if( strcasecmp(parameter, "ENDIF") == 0 ) {
	  if( nesting_level == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: unexpected %s statement!\n", line, parameter);
#endif
	  } else {
	    --nesting_level;
	  }
	  continue; // read next line
	}

	if( strcasecmp(parameter, "ELSE") == 0 ) {
	  if( nesting_level == 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	    DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: unexpected %s statement!\n", line, parameter);
#endif
	  } else {
	    if( nesting_level >= 2 && if_state[nesting_level-2] == 0 ) { // this ELSIF is executed inside a non-matching block
	      if_state[nesting_level-1] = 0;
	    } else {
	      if( if_state[nesting_level-1] == 0 ) { // no matching IF condition yet?
		if_state[nesting_level-1] = 1; // matching condition
	      } else {
		if_state[nesting_level-1] = 2; // IF has been processed
	      }
	    }
	  }
	  continue; // read next line
	}

	if( nesting_level == 0 || if_state[nesting_level-1] == 1 ) {
	  if( strcasecmp(parameter, "LCD") == 0 ) {
	    char *str = brkt;
	    if( !(str=remove_quotes(str)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing string after LCD message!\n", line);
#endif
	    } else {
	      // print from a dummy item
	      mbng_event_item_t item;
	      MBNG_EVENT_ItemInit(&item, MBNG_EVENT_CONTROLLER_DISABLED);
	      item.label = str;
	      MBNG_LCD_PrintItemLabel(&item);
	    }
	  } else if( strcasecmp(parameter, "LOG") == 0 ) {
	    char *str = brkt;
	    if( !(str=remove_quotes(str)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing string after LOG message!\n", line);
#endif
	    } else {
	      MIOS32_MIDI_SendDebugString(str);
	    }
	  } else if( strcasecmp(parameter, "SEND") == 0 ) {
	    parseSend(line, parameter, &brkt, &vars);
	  } else if( strcasecmp(parameter, "EXIT") == 0 ) {
	    exit = 1;
	  } else if( strcasecmp(parameter, "SET") == 0 || strcasecmp(parameter, "TRIGGER") == 0 ) {
	    u8 trigger = strcasecmp(parameter, "TRIGGER") == 0;

	    mbng_event_item_id_t id;
	    char *value_str;

	    if( !(value_str = strtok_r(NULL, separators, &brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing id in '%s' command!\n", line, parameter);
#endif
	    } else if( !(id=parseId(value_str)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: invalid id '%s %s'!\n", line, parameter, value_str);
#endif
	    } else {
	      s32 value = 0;
	      if( !trigger && !(value_str = strtok_r(NULL, separators, &brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
		DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing value after '%s %s:%d' command!\n", line, parameter, MBNG_EVENT_ItemControllerStrGet(id), id & 0xfff);
#endif
	      } else if( !trigger && ((value=parseValue(line, parameter, value_str, &vars)) < -16384 || value >= 16383) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
		DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: invalid value in '%s %s:%d %s' command (expecting -16384..16383!\n", line, parameter, MBNG_EVENT_ItemControllerStrGet(id), id & 0xfff, value_str);
#endif
	      } else {
#if DEBUG_VERBOSE_LEVEL >= 2
		DEBUG_MSG("[MBNG_FILE_R:%d] %s:%d = %d\n", line, MBNG_EVENT_ItemControllerStrGet(id), id & 0xfff, value);
#endif

		// search for items with matching ID
		mbng_event_item_t item;
		u32 continue_ix = 0;
		u32 num_set = 0;
		do {
		  if( MBNG_EVENT_ItemSearchById(id, &item, &continue_ix) < 0 ) {
		    break;
		  } else {
		    ++num_set;

		    // notify item
		    if( !trigger )
		      item.value = value;
		    if( MBNG_EVENT_NotifySendValue(&item) == 2 )
		      break; // stop has been requested
		  }
		} while( continue_ix );

		if( !num_set ) {
#if DEBUG_VERBOSE_LEVEL >= 1
		  DEBUG_MSG(trigger ? "[MBNG_FILE_R:%d] '%s %s:%d' failed - item not found!\n" : "[MBNG_FILE_R:%d] '%s %s:%d %d' failed - item not found!\n", line, parameter, MBNG_EVENT_ItemControllerStrGet(id), id & 0xfff, value);
#endif
		}
	      }
	    }

	  } else if( strcasecmp(parameter, "DELAY_MS") == 0 ) {
	    char *value_str;
	    s32 value;
	    if( !(value_str = strtok_r(NULL, separators, &brkt)) ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: missing value after '%s' command!\n", line, parameter);
#endif
	    } else if( (value=parseValue(line, parameter, value_str, &vars)) < 0 || value > 100000 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
	      DEBUG_MSG("[MBNG_FILE_R:%d] ERROR: invalid value in '%s %s' command (expecting 0..100000)!\n", line, parameter, value_str);
#endif
	    } else {
#if DEBUG_VERBOSE_LEVEL >= 2
	      DEBUG_MSG("[MBNG_FILE_R:%d] DELAY_MS %d\n", line, value);
#endif
	      int i;
	      for(i=0; i<value; ++i) {
		vTaskDelay(1 / portTICK_RATE_MS);
	      }
	    }
	  } else {
#if DEBUG_VERBOSE_LEVEL >= 1
	    // changed error to warning, since people are sometimes confused about these messages
	    // on file format changes
	    DEBUG_MSG("[MBNG_FILE_R:%d] WARNING: unknown command: %s", line, line_buffer);
#endif
	  }
	} else {
#if DEBUG_VERBOSE_LEVEL >= 2
	  // no real error, can for example happen in .csv file
	  DEBUG_MSG("[MBNG_FILE_R:%d] ERROR no space or semicolon separator in following line: %s", line, line_buffer);
#endif
	}
      }
    }

  } while( !exit && status >= 1 );

  // release memory from heap
  vPortFree(line_buffer);

#if DEBUG_VERBOSE_LEVEL >= 1
  if( exit >= 2 ) {
    DEBUG_MSG("[MBNG_FILE_R:%d] stopped script execution due to previous error!\n", line);
  } else if( nesting_level > 0 ) {
    DEBUG_MSG("[MBNG_FILE_R:%d] WARNING: missing ENDIF command!\n", line);
  }
#endif

  // close file
  status |= FILE_ReadClose(&file);

  if( status < 0 ) {
#if DEBUG_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[MBNG_FILE_R] ERROR while reading file, status: %d\n", status);
#endif
    return MBNG_FILE_R_ERR_READ;
  }

  // file is valid! :)
  info->valid = 1;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! request to read the script file from run thread
//! \returns < 0 on errors (error codes are documented in mbng_file.h)
/////////////////////////////////////////////////////////////////////////////
s32 MBNG_FILE_R_ReadRequest(char *filename, u8 section, s16 value, u8 notify_done)
{
  // store current file name in global variable for UI
  memcpy(mbng_file_r_script_name, filename, MBNG_FILE_R_FILENAME_LEN+1);

  // set request (and value)
  mbng_file_r_req.section = section;
  mbng_file_r_req.value = value;
  mbng_file_r_req.notify_done = notify_done;
  mbng_file_r_req.load = 1;

  return 0;
}

//! \}

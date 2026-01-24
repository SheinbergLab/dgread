/*************************************************************************
 *
 *  NAME
 *    df.c
 *
 *  DESCRIPTION
 *    Functions for creating, loading, and dumping common data format
 *  for behavioral and spike data.
 *    
 *  DETAILS 
 *    The design principles for the data format were threefold.
 *  First, we wanted a binary format which was as compact as possible,
 *  to minimize storage space.  We also wanted the binary file to be
 *  readable on different machine architectures (e.g. RISC & CISC
 *  based machines such as the sparc and i486).  Second, we wanted to
 *  be able to store embedded structures within the file, which would
 *  mirror an actual C structure containing the data.  Thus, the files
 *  are designed to be dumped from and read into an actual C structure
 *  (the DATA_FILE).  Finally, and perhaps most importantly, we wanted
 *  to pick a format which could evolve over time without making all
 *  previous versions of the data out of date.  This would be
 *  impossible if structures themselves were simply dumped, because at
 *  the moment that a new field was added (or deleted) from a
 *  structure, old files would become unreadable.  To achieve this
 *  goal of extensibility, we chose a tagged file format.  Tags are
 *  single bytes, and thus the only limitation is that there are no
 *  more than 255 entries per structure.  This isn't really a
 *  major problem, since new structures can be introduced.
 *
 *  ADDING NEW STRUCTURE FIELDS
 *    One of the main reasons one might have to deal with this code is to
 *  add new fields to structures.  Here I describe the steps that would
 *  be necessary to add a new field to the Event Data (EV_DATA) 
 *  structure.  Keep in mind that the only place where order makes a
 *  difference is within the enumerated defines.  Those are the defines
 *  which tag different data types and structures.  The tags themselves
 *  are arbitrary, *BUT* once a tag is inserted, it's id cannot change,
 *  since this id is the basis for interpreting all data files.  Here are
 *  the steps for adding a hypothetical field called "smilet" to the
 *  EV_DATA structure (which would correspond to those times during 
 *  an observation period at which the monkey smiled).
 *
 *   1.  Go to the file df.h and find the structure definition for
 *       EV_DATA.  Insert your new entry, smilet, into the structure
 *       (at the bottom would be nice, but not necessary).  It should
 *       probably be of type EV_LIST, which would allow there to be
 *       multiple times stored in the smilet entry. (It could be a long,
 *       or short, or an array if you really wanted).
 *
 *   2.  For consistency, add a #define macro for accessing your
 *       new data field from the structure, e.g.:
 *
 *             #define EV_SMILET(e)   (&(e)->smilet)
 *
 *       This cleans up the interface a bit and hides the specifics of
 *       the implementation from the rest of the program.  Note that
 *       all defines which access structures return pointers to those
 *       structures (hence the &).  This is important, because it makes
 *       access to the data consistent (e.g. one define can be used as the
 *       argument to another, since all defines use the -> notation).
 *
 *   3.  Add a new tag to the enum found just above the structure 
 *       definition.  This tag MUST be added at the end of the list, but
 *       just before the E_NEVENT_TAGS.
 *
 *   4.  Go to the file df.c and find the TagTable for the event data
 *       structure.  At the bottom, add a line for your new smilet entry
 *       which would be similar to the others, but would have the tag
 *       name you had picked, and a name, in quotes, like "SMILE_TIME".
 *
 *   5.  Find the function which "records" the event data struct (named
 *       dfRecordEvData).  A function call must be added to dump your
 *       new data field.  In this case dfRecordEvList will work fine.  Copy
 *       any of the other lines and change the tag name.  Also be sure to
 *       supply the macro which accesses your new entry (e.g. EV_SMILET())
 *       as the second argument to dfRecordEvList.
 *
 *   6.  Go the the file dfutils.c.  This is where you supply information
 *       about how to "interpret" the newly defined tag.  There are two
 *       functions which process the Event Data tags: "dfuFileToEvData()" and
 *       "dfuBufferToEvData()".  The former reads from a file and the latter
 *       from a buffer already loaded in memory.  To ensure that your
 *       new field is properly interpreted when the tagged data is read in
 *       you must add an entry to the switch statements in each of the
 *       functions.  For both functions, the case statement should look
 *       very similar to other entries around it.  Note that order in
 *       these switches is NOT important.
 *
 *   7.  Find the free function, in this case called "dfuFreeEvData()",
 *       and add a call to free up your data.  This is only necessary
 *       space is allocated (via calloc or malloc) for your data.  In
 *       this case it is (because an EV_LIST has a pointer within it
 *       to allocated data.  Otherwise, there would be a limit on how
 *       many smiles could be recorded during one observation period.)
 *       If you only add a long or float entry, then no explicit freeing
 *       is required.
 *
 *   8.  For new event tags, add support for dynamically keeping track of
 *       events as they are parsed, by updating the dfuAddEvData() function.
 *
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <string.h>
#include "df.h"

char dfMagicNumber[] = { 0x20, 0x10, 0x30, 0x60 };
float dfVersion = 1.0;

#define DF_DATA_BUFFER_SIZE 64000

static void dfDumpBuffer(unsigned char *buffer, int n, int type, FILE *fp);
static unsigned char *DfBuffer = NULL;
static int DfBufferIndex = 0;
static int DfBufferSize;
static int DfRecording = 0;

/* Keep track of which structure we're in using a stack */
static int DfCurStruct = TOP_LEVEL;
static char *DfCurStructName = "TOP_LEVEL";
static TAG_INFO *DfStructStack = NULL;
static int DfStructStackIncrement = 10;
static int DfStructStackSize = 0;
static int DfStructStackIndex = -1;

static void send_event(unsigned char type, unsigned char *data);
static void send_bytes(int n, unsigned char *data);
static void push(unsigned char *data, int, int);


/***********************************************************************/
/*                        Structure Tag Tables                         */
/***********************************************************************/

/*                                                             */
/*   Tag_ID      Tag_Name     Tag_Type      Struct_Type        */
/* ------------  ----------  ------------  -------------       */

TAG_INFO TopLevelTags[] = {
  { T_VERSION_TAG,  "VERSION",     DF_VERSION,    TOP_LEVEL },
  { T_BEGIN_DF_TAG, "DATA_FILE",   DF_STRUCTURE,  DATA_FILE_STRUCT },
};

TAG_INFO DFTags[] = {
  { D_DFINFO_TAG, "DF_INFO",       DF_STRUCTURE,  DF_INFO_STRUCT },
  { D_NOBSP_TAG,  "NOBS_PERIODS",  DF_LONG,       TOP_LEVEL },
  { D_OBSP_TAG,   "OBS_PERIOD",    DF_STRUCTURE,  OBS_P_STRUCT },
  { D_NCINFO_TAG, "NCELL_INFOS",   DF_LONG,       TOP_LEVEL },
  { D_CINFO_TAG,  "CELL_INFO",     DF_STRUCTURE,  CELL_INFO_STRUCT },
};

TAG_INFO DFInfoTags[] = {
  { D_FILENAME_TAG,  "FILENAME",   DF_STRING, TOP_LEVEL },
  { D_TIME_TAG,      "TIME",       DF_LONG,   TOP_LEVEL },
  { D_FILENUM_TAG,   "FILENUM",    DF_LONG,   TOP_LEVEL },  
  { D_COMMENT_TAG,   "COMMENTS",   DF_STRING, TOP_LEVEL },
  { D_EXP_TAG,       "EXP_ID",     DF_LONG,   TOP_LEVEL }, 
  { D_TMODE_TAG,     "TEST_MODE",  DF_LONG,   TOP_LEVEL },
  { D_EMCOLLECT_TAG, "EM_ON",      DF_CHAR,   TOP_LEVEL },
  { D_SPCOLLECT_TAG, "SPIKE_ON",   DF_CHAR,   TOP_LEVEL },
  { D_NSTIMTYPES_TAG,"NSTIMTYPES", DF_LONG,   TOP_LEVEL },
  { D_AUXFILES_TAG,  "AUXFILES",   DF_STRING_ARRAY, TOP_LEVEL },
};

TAG_INFO ObspTags[] = {
  { O_INFO_TAG,   "OBSP_INFO",     DF_STRUCTURE, OBS_INFO_STRUCT },
  { O_EVDATA_TAG, "EVENT_DATA",    DF_STRUCTURE, EV_DATA_STRUCT },
  { O_SPDATA_TAG, "SPIKE_DATA",    DF_STRUCTURE, SP_DATA_STRUCT },
  { O_EMDATA_TAG, "EM_DATA",       DF_STRUCTURE, EM_DATA_STRUCT },
};

TAG_INFO ObsInfoTags[] = {
  { O_BLOCK_TAG,     "BLOCK_NUM",    DF_LONG,   TOP_LEVEL },
  { O_OBSP_TAG,      "OBS_NUM",      DF_LONG,   TOP_LEVEL }, 
  { O_STATUS_TAG,    "OBS_STATUS",   DF_LONG,   TOP_LEVEL },
  { O_DURATION_TAG,  "DURATION",     DF_LONG,   TOP_LEVEL },
  { O_NTRIALS_TAG,   "NTRIALS",      DF_LONG,   TOP_LEVEL },
  { O_FILENUM_TAG,   "FILENUM",      DF_LONG,   TOP_LEVEL },
  { O_INDEX_TAG,     "OBSINDEX",     DF_LONG,   TOP_LEVEL },
};

TAG_INFO EvDataTags[] = {
  { E_FIXON_TAG,   "FIXON",     DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_FIXOFF_TAG,  "FIXOFF",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_STIMON_TAG,  "STIMON",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_STIMOFF_TAG, "STIMOFF",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_RESP_TAG,    "RESPONSE",  DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PATON_TAG,   "PATON",     DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PATOFF_TAG,  "PATOFF",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_STIMTYPE_TAG,"STIMTYPE",  DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PATTERN_TAG, "PATTERN",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_REWARD_TAG,  "REWARD",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PROBEON_TAG, "PROBEON",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PROBEOFF_TAG,"PROBEOFF",  DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_SAMPON_TAG,  "SAMPON",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_SAMPOFF_TAG, "SAMPOFF",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_FIXATE_TAG,  "FIXATE",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_DECIDE_TAG,  "DECIDE",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_STIMULUS_TAG,"STIMULUS",  DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_DELAY_TAG,   "DELAY",     DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_ISI_TAG,     "ISI",       DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_UNIT_TAG,    "UNIT",      DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_INFO_TAG,    "INFO",      DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_CUE_TAG,     "CUE",       DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_TARGET_TAG,  "TARGET",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_DISTRACTOR_TAG, "DISTRACTOR", DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_CORRECT_TAG, "CORRECT",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_TRIALTYPE_TAG, "TRIALTYPE",   DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_ABORT_TAG,     "ABORT",       DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_WRONG_TAG,     "WRONG",       DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_PUNISH_TAG,    "PUNISH",      DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_BLANKING_TAG,  "BLANKING",    DF_STRUCTURE,   EV_LIST_STRUCT },
  { E_SACCADE_TAG,   "SACCADE",     DF_STRUCTURE,   EV_LIST_STRUCT },
};

TAG_INFO EvListTags[] = {
  { E_VAL_LIST_TAG,  "VALUES",  DF_LONG_ARRAY,   TOP_LEVEL },
  { E_TIME_LIST_TAG, "TIMES",   DF_LONG_ARRAY,   TOP_LEVEL },
};

TAG_INFO EMDataTags[] = {
  { E_ONTIME_TAG,    "EMSTART",   DF_LONG,             TOP_LEVEL },
  { E_RATE_TAG,      "EMRATE",    DF_FLOAT,            TOP_LEVEL },
  { E_FIXPOS_TAG,    "FIXPOS",    DF_SHORT_ARRAY,      TOP_LEVEL },
  { E_WINDOW_TAG,    "WINDOW",    DF_SHORT_ARRAY,      TOP_LEVEL },
  { E_PNT_DEG_TAG,   "PNT_DEG",   DF_LONG,             TOP_LEVEL },
  { E_H_EM_LIST_TAG, "H_EYE_POS", DF_SHORT_ARRAY,      TOP_LEVEL },
  { E_V_EM_LIST_TAG, "V_EYE_POS", DF_SHORT_ARRAY,      TOP_LEVEL },
  { E_WINDOW2_TAG,   "REFIX_WIN", DF_SHORT_ARRAY,      TOP_LEVEL },
};

TAG_INFO SPDataTags[] = {
  { S_NCHANNELS_TAG,  "NSP_CHANNELS",   DF_LONG,       TOP_LEVEL },
  { S_CHANNEL_TAG,    "SP_CHANNEL",     DF_STRUCTURE,  SP_CHANNEL_STRUCT },
};

TAG_INFO SPChannelTags[] = {
  { S_CH_DATA_TAG,    "SPIKE_DATA",    DF_FLOAT_ARRAY, TOP_LEVEL },
  { S_CH_SOURCE_TAG,  "SPIKE_SOURCE",  DF_CHAR,        TOP_LEVEL },
  { S_CH_CELLNUM_TAG, "CELLNUM",       DF_LONG,        TOP_LEVEL },
};

TAG_INFO CellTags[] = {
  { C_NUM_TAG,       "CELLNUM",           DF_LONG,             TOP_LEVEL },
  { C_DISCRIM_TAG,   "DISCRIM",           DF_FLOAT,            TOP_LEVEL },
  { C_EV_TAG,        "EV_COORD",          DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_XY_TAG,        "XY_COORD",          DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_RFCENTER_TAG,  "RF_COORD",          DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_DEPTH_TAG,     "DEPTH",             DF_FLOAT,            TOP_LEVEL },
  { C_TL_TAG,        "BOX: TOP LEFT",     DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_BL_TAG,        "BOX: BOTTOM LEFT",  DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_BR_TAG,        "BOX: BOTTOM RIGHT", DF_FLOAT_ARRAY,      TOP_LEVEL },
  { C_TR_TAG,        "BOX: TOP RIGHT",    DF_FLOAT_ARRAY,      TOP_LEVEL },
};

TAG_INFO *TagTable[] = {
  TopLevelTags, DFTags, DFInfoTags, ObspTags, ObsInfoTags,
  EvDataTags, EvListTags, EMDataTags, SPDataTags, SPChannelTags,
  CellTags,
};

/**************************************************************************/
/*                      Initialization Routines                           */
/**************************************************************************/

void dfInitBuffer(void)
{
  DfBufferSize = DF_DATA_BUFFER_SIZE;
  if (!(DfBuffer = (unsigned char *)
	calloc(DfBufferSize, sizeof(unsigned char)))) {
    fprintf(stderr,"Unable to allocate df buffer\n");
    exit(-1);
  }
  
  dfResetBuffer();
}

void dfResetBuffer(void)
{
  DfRecording = 1;
  DfBufferIndex = 0;
  
  dfPushStruct(TOP_LEVEL, "TOP_LEVEL");
  
  dfRecordMagicNumber();
  dfRecordFloat(T_VERSION_TAG, dfVersion);
}

void dfCloseBuffer(void)
{
  if (DfBuffer) free(DfBuffer);
  dfFreeStructStack();
  DfRecording = 0;
}

void dfWriteBuffer(char *filename, char format)
{
   FILE *fp = stdout;
   char *filemode = "wb+";
   
   switch (format) {
      case DF_BINARY:
	 filemode = "wb+";
	 break;
      default:
	 filemode = "w+";
	 break;
   }
   
   if (filename && filename[0]) {
     if (!(fp = fopen(filename,filemode))) {
       fprintf(stderr,"df: unable to open file \"%s\" for output\n",
	       filename);
       exit(-1);
     }
   }
   dfDumpBuffer(DfBuffer, DfBufferIndex, format, fp);

   if (filename && filename[0]) fclose(fp);
}

int dfReadDataFile(char *filename, DATA_FILE *df)
{
  FILE *fp = stdin;
  char *filemode = "rb";
  int status = 0;
  
  if (filename && filename[0]) {
    if (!(fp = fopen(filename,filemode))) {
      return(0);
    }
  }
  status = dfuFileToStruct(fp, df);
  
  if (filename && filename[0]) fclose(fp);
  return(status);
}


void dfLoadStructure(DATA_FILE *df)
{
  dfuBufferToStruct(DfBuffer, DfBufferIndex, df);
}

/*********************************************************************/
/*                   High Level Recording Funcs                      */
/*********************************************************************/

void dfRecordDataFile(DATA_FILE *df)
{
  int i = 0;

  dfBeginStruct(T_BEGIN_DF_TAG);
  dfRecordDFInfo(D_DFINFO_TAG, DF_DFINFO(df));

/* record the number of cell_infos and then the structures */
  dfRecordLong(D_NCINFO_TAG, DF_NCINFO(df));
  for (i = 0; i < DF_NCINFO(df); i++) 
    dfRecordCellInfo(D_CINFO_TAG, DF_CINFO(df,i));

/* record the number of observation periods and then the structures */
  dfRecordLong(D_NOBSP_TAG, DF_NOBSP(df));
  for (i = 0; i < DF_NOBSP(df); i++) 
    dfRecordObsPeriod(D_OBSP_TAG, DF_OBSP(df,i));

  dfEndStruct();
}

void dfRecordDFInfo(unsigned char tag, DF_INFO *dfinfo)
{
  dfBeginStruct(tag);
  dfRecordString(D_FILENAME_TAG, DF_FILENAME(dfinfo));
  dfRecordStringArray(D_AUXFILES_TAG, 
		      DF_NAUXFILES(dfinfo), DF_AUXFILES(dfinfo));
  dfRecordLong(D_TIME_TAG, DF_TIME(dfinfo));
  dfRecordLong(D_FILENUM_TAG, DF_FILENUM(dfinfo));
  dfRecordString(D_COMMENT_TAG, DF_COMMENT(dfinfo));
  dfRecordLong(D_EXP_TAG, DF_EXPERIMENT(dfinfo));
  dfRecordLong(D_TMODE_TAG, DF_TESTMODE(dfinfo));
  dfRecordLong(D_NSTIMTYPES_TAG, DF_NSTIMTYPES(dfinfo));
  dfRecordChar(D_EMCOLLECT_TAG, DF_EMCOLLECT(dfinfo));
  dfRecordChar(D_SPCOLLECT_TAG, DF_SPCOLLECT(dfinfo));
  dfEndStruct();
}

void dfRecordObsPeriod(unsigned char tag, OBS_P *obsp)
{
  dfBeginStruct(tag);
  dfRecordObsInfo(O_INFO_TAG, OBSP_INFO(obsp));
  dfRecordEvData(O_EVDATA_TAG, OBSP_EVDATA(obsp));
  dfRecordSpData(O_SPDATA_TAG, OBSP_SPDATA(obsp));
  dfRecordEmData(O_EMDATA_TAG, OBSP_EMDATA(obsp));
  dfEndStruct();
}

void dfRecordObsInfo(unsigned char tag, OBS_INFO *oinfo)
{
  dfBeginStruct(tag);
  dfRecordLong(O_FILENUM_TAG, OBS_FILENUM(oinfo));
  dfRecordLong(O_INDEX_TAG, OBS_INDEX(oinfo));
  dfRecordLong(O_BLOCK_TAG, OBS_BLOCK(oinfo));
  dfRecordLong(O_OBSP_TAG, OBS_OBSP(oinfo));
  dfRecordLong(O_STATUS_TAG, OBS_STATUS(oinfo));
  dfRecordLong(O_DURATION_TAG, OBS_DURATION(oinfo));
  dfRecordLong(O_NTRIALS_TAG, OBS_NTRIALS(oinfo));
  dfEndStruct();
}

void dfRecordEvData(unsigned char tag, EV_DATA *evdata)
{
  dfBeginStruct(tag);
  dfRecordEvList(E_FIXON_TAG, EV_FIXON(evdata));
  dfRecordEvList(E_FIXOFF_TAG, EV_FIXOFF(evdata));
  dfRecordEvList(E_STIMON_TAG, EV_STIMON(evdata));
  dfRecordEvList(E_STIMOFF_TAG, EV_STIMOFF(evdata));
  dfRecordEvList(E_RESP_TAG, EV_RESP(evdata));
  dfRecordEvList(E_PATON_TAG, EV_PATON(evdata));
  dfRecordEvList(E_PATOFF_TAG, EV_PATOFF(evdata));
  dfRecordEvList(E_STIMTYPE_TAG, EV_STIMTYPE(evdata));
  dfRecordEvList(E_PATTERN_TAG, EV_PATTERN(evdata));
  dfRecordEvList(E_REWARD_TAG, EV_REWARD(evdata));
  dfRecordEvList(E_PROBEON_TAG, EV_PROBEON(evdata));
  dfRecordEvList(E_PROBEOFF_TAG, EV_PROBEOFF(evdata));
  dfRecordEvList(E_SAMPON_TAG, EV_SAMPON(evdata));
  dfRecordEvList(E_SAMPOFF_TAG, EV_SAMPOFF(evdata));
  dfRecordEvList(E_FIXATE_TAG, EV_FIXATE(evdata));
  dfRecordEvList(E_DECIDE_TAG, EV_DECIDE(evdata));
  dfRecordEvList(E_STIMULUS_TAG, EV_STIMULUS(evdata));
  dfRecordEvList(E_DELAY_TAG, EV_DELAY(evdata));
  dfRecordEvList(E_ISI_TAG, EV_ISI(evdata));
  dfRecordEvList(E_CUE_TAG, EV_CUE(evdata));
  dfRecordEvList(E_TARGET_TAG, EV_TARGET(evdata));
  dfRecordEvList(E_DISTRACTOR_TAG, EV_DISTRACTOR(evdata));
  dfRecordEvList(E_CORRECT_TAG, EV_CORRECT(evdata));
  dfRecordEvList(E_UNIT_TAG, EV_UNIT(evdata));
  dfRecordEvList(E_INFO_TAG, EV_INFO(evdata));
  dfRecordEvList(E_TRIALTYPE_TAG, EV_TRIALTYPE (evdata));
  dfRecordEvList(E_ABORT_TAG, EV_ABORT(evdata));
  dfRecordEvList(E_WRONG_TAG, EV_WRONG(evdata));
  dfRecordEvList(E_PUNISH_TAG, EV_PUNISH(evdata));
  dfRecordEvList(E_SACCADE_TAG, EV_SACCADE(evdata));
  dfEndStruct();
}

void dfRecordEvList(unsigned char tag, EV_LIST *evlist)
{
  if (EV_LIST_N(evlist)) {
    dfBeginStruct(tag);
    dfRecordLongArray(E_VAL_LIST_TAG, 
		      EV_LIST_N(evlist), EV_LIST_VALS(evlist));
    dfRecordLongArray(E_TIME_LIST_TAG, 
		      EV_LIST_NTIMES(evlist), EV_LIST_TIMES(evlist));
    dfEndStruct();
  }
}

void dfRecordEmData(unsigned char tag, EM_DATA *emdata)
{
  dfBeginStruct(tag);
  dfRecordLong(E_ONTIME_TAG, EM_ONTIME(emdata));
  dfRecordFloat(E_RATE_TAG, EM_RATE(emdata));
  dfRecordShortArray(E_FIXPOS_TAG, 2, EM_FIXPOS(emdata));
  dfRecordShortArray(E_WINDOW_TAG, 4, EM_WINDOW(emdata));
  dfRecordShortArray(E_WINDOW2_TAG, 4, EM_WINDOW2(emdata));
  dfRecordLong(E_PNT_DEG_TAG, EM_PNT_DEG(emdata));
  dfRecordShortArray(E_H_EM_LIST_TAG, EM_NSAMPS(emdata), EM_SAMPS_H(emdata));
  dfRecordShortArray(E_V_EM_LIST_TAG, EM_NSAMPS(emdata), EM_SAMPS_V(emdata));
  dfEndStruct();
}

void dfRecordSpData(unsigned char tag, SP_DATA *spdata)
{
  int i;
  dfBeginStruct(tag);
  dfRecordLong(S_NCHANNELS_TAG, SP_NCHANNELS(spdata));
  for (i = 0; i < SP_NCHANNELS(spdata); i++) 
    dfRecordSpChData(S_CHANNEL_TAG, SP_CHANNEL(spdata,i));
  dfEndStruct();
}

void dfRecordSpChData(unsigned char tag, SP_CH_DATA *chdata)
{
  dfBeginStruct(tag);
  dfRecordLong(S_CH_CELLNUM_TAG, SP_CH_CELLNUM(chdata));
  dfRecordChar(S_CH_SOURCE_TAG, SP_CH_SOURCE(chdata));
  dfRecordFloatArray(S_CH_DATA_TAG, 
		     SP_CH_NSPTIMES(chdata), SP_CH_SPTIMES(chdata));

  dfEndStruct();
}

void dfRecordCellInfo(unsigned char tag, CELL_INFO *cinfo)
{
  dfBeginStruct(tag);
  dfRecordLong(C_NUM_TAG, CI_NUMBER(cinfo));
  dfRecordFloat(C_DISCRIM_TAG, CI_DISCRIM(cinfo));
  dfRecordFloatArray(C_EV_TAG, 2, CI_EVCOORDS(cinfo));
  dfRecordFloatArray(C_XY_TAG, 2, CI_XYCOORDS(cinfo));
  dfRecordFloatArray(C_RFCENTER_TAG, 2, CI_RF_CENTER(cinfo));
  dfRecordFloat(C_DEPTH_TAG, CI_DEPTH(cinfo));
  dfRecordFloatArray(C_TL_TAG, 2, CI_RF_QUAD_UL(cinfo));
  dfRecordFloatArray(C_BL_TAG, 2, CI_RF_QUAD_LL(cinfo));
  dfRecordFloatArray(C_BR_TAG, 2, CI_RF_QUAD_LR(cinfo));
  dfRecordFloatArray(C_TR_TAG, 2, CI_RF_QUAD_UR(cinfo));
  dfEndStruct();
}


/*********************************************************************/
/*                   Array Event Recording Funcs                     */
/*********************************************************************/

void dfBeginStruct(unsigned char tag)
{
  dfRecordFlag(tag);
  dfPushStruct(dfGetStructureType(tag), dfGetTagName(tag));
}

void dfEndStruct(void)
{
  dfRecordFlag(END_STRUCT);
  dfPopStruct();
}

void dfRecordString(unsigned char type, char *str)
{
  int length;
  if (!str) return;
  length = strlen(str) + 1;
  send_event(type, (unsigned char *) &length);
  send_bytes(length, (unsigned char *)str);
}

void dfRecordStringArray(unsigned char type, int n, char **s)
{
  int length, i;
  char *str;
  
  if (!s || !n) return;
  
  send_event(type, (unsigned char *) &n);
  for (i = 0; i < n; i++) {
    str = s[i];
    length = strlen(str) + 1;
    send_bytes(sizeof(int), (unsigned char *) &length);
    send_bytes(length, (unsigned char *)str);
  }
}

void dfRecordLongArray(unsigned char type, int n, int *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(int), (unsigned char *) a);
}

void dfRecordShortArray(unsigned char type, int n, short *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(short), (unsigned char *) a);
}

void dfRecordFloatArray(unsigned char type, int n, float *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(float), (unsigned char *) a);
}

/*********************************************************************/
/*                  Low Level Event Recording Funcs                  */
/*********************************************************************/

void dfRecordMagicNumber(void)
{
  send_bytes(DF_MAGIC_NUMBER_SIZE, (unsigned char *) dfMagicNumber);
}

void dfRecordFlag(unsigned char type)
{
  send_event(type, (unsigned char *) NULL);
}

void dfRecordChar(unsigned char type, unsigned char val)
{
  send_event(type, (unsigned char *) &val);
}

void dfRecordLong(unsigned char type, int val)
{
  send_event(type, (unsigned char *) &val);
}

void dfRecordShort(unsigned char type, short val)
{
  send_event(type, (unsigned char *) &val);
}

void dfRecordFloat(unsigned char type, float val)
{
  send_event(type, (unsigned char *) &val);
}


/*********************************************************************/
/*                    Keep Track of Current Structure                */
/*********************************************************************/

void dfPushStruct(int newstruct, char *name)
{
  if (!DfStructStack) 
    DfStructStack = 
      (TAG_INFO *) calloc(DfStructStackIncrement, sizeof(TAG_INFO));
  
  else if (DfStructStackIndex == (DfStructStackSize-1)) {
    DfStructStackSize += DfStructStackIncrement;
    DfStructStack = 
      (TAG_INFO *) realloc(DfStructStack, DfStructStackSize*sizeof(TAG_INFO));
  }
  DfStructStackIndex++;
  DfStructStack[DfStructStackIndex].struct_type = newstruct;
  DfStructStack[DfStructStackIndex].tag_name = name;
  DfCurStruct = newstruct;
  DfCurStructName = name;
}
    
int dfPopStruct(void)
{
  if (!DfStructStackIndex) {
    fprintf(stderr, "dfPopStruct(): popped to an empty stack\n");
    return(-1);
  }

  DfStructStackIndex--;
  DfCurStruct = DfStructStack[DfStructStackIndex].struct_type;
  DfCurStructName = DfStructStack[DfStructStackIndex].tag_name;

  return(DfCurStruct);
}

void dfFreeStructStack(void)
{
  if (DfStructStack) free(DfStructStack);
  DfStructStack = NULL;
  DfStructStackSize = 0;
  DfStructStackIndex = -1;
}

int dfGetCurrentStruct(void)
{
  return(DfCurStruct);
}

char *dfGetCurrentStructName(void)
{
  return(DfCurStructName);
}


char *dfGetTagName(int type)
{
  return(TagTable[DfCurStruct][type].tag_name);
}

int dfGetDataType(int type)
{
  return(TagTable[DfCurStruct][type].data_type);
}

int dfGetStructureType(int type)
{
  return(TagTable[DfCurStruct][type].struct_type);
}
/*********************************************************************/
/*               Local Byte Stream Handling Functions                */
/*********************************************************************/

static void send_event(unsigned char type, unsigned char *data)
{
/* First push the tag into the buffer */
  push((unsigned char *)&type, 1, 1);
  
/* The only "special" tag is the END_STRUCT tag, which means pop up */
  if (type == END_STRUCT) return;

/* All other tags may have data; check the current struct tag table  */
  switch(TagTable[DfCurStruct][type].data_type) {
  case DF_STRUCTURE:            /* data follows via tags             */
  case DF_FLAG:			/* no data associated with a flag    */
    break;
  case DF_STRING:		/* all of these start w/a long int   */
  case DF_STRING_ARRAY:
  case DF_LONG_ARRAY:
  case DF_SHORT_ARRAY:
  case DF_FLOAT_ARRAY:
  case DF_LONG:
    push(data, sizeof(int), 1);
    break;
  case DF_CHAR:
    push(data, sizeof(char), 1);
    break;
  case DF_SHORT:
    push(data, sizeof(short), 1);
    break;
  case DF_VERSION:
  case DF_FLOAT:
    push(data, sizeof(float), 1);
    break;
  default:
    fprintf(stderr,"Unrecognized event type: %d\n", type);
    break;
  }
}

static void send_bytes(int n, unsigned char *data)
{
  push(data, sizeof(unsigned char), n);
}

static void push(unsigned char *data, int size, int count)
{
   int nbytes, newsize;
   
   nbytes = count * size;
   
   if (DfBufferIndex + nbytes >= DfBufferSize) {
	 do {
	   newsize = DfBufferSize + DF_DATA_BUFFER_SIZE;
	   DfBuffer = (unsigned char *) realloc(DfBuffer, newsize);
	   DfBufferSize = newsize;
	 } while(DfBufferIndex + nbytes >= DfBufferSize);
   }
   
   memcpy(&DfBuffer[DfBufferIndex], data, nbytes);
   DfBufferIndex += nbytes;
}


/*************************************************************************/
/*                         Dump Helper Funcs                             */
/*************************************************************************/

static void dfDumpBuffer(unsigned char *buffer, int n, int type, FILE *fp)
{
  switch(type) {
  case DF_BINARY:
    fwrite(buffer, sizeof(unsigned char), n, fp);
    fflush(fp);
    break;
  case DF_ASCII:
    dfuBufferToAscii(buffer, n, fp);
    break;
  default:
    break;
  }
}

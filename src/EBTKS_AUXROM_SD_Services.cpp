//
//  08/30/2020  PMF   These Function implement all of the AUXROM functions
//                    for SD card access from the HP85 BASIC Programs
//  09/01/2020        Work on SDCD. Decide that path separator is '/' and paths have a trailing '/'
//                    Initial path is "/"
//

#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "Inc_Common_Headers.h"

#define MAX_SD_PATH_LENGTH          (256)
#define LAST_SEGMENT_LENGTH         ( 40)

static char         Current_Path[MAX_SD_PATH_LENGTH + 2] = "/";
static char         Resolved_Path[MAX_SD_PATH_LENGTH * 2 + 2] = "/";
static char         Resolved_Path_Last_Segment[LAST_SEGMENT_LENGTH] = "";
static bool         Resolved_Path_ends_with_slash;
static File         temp_file;

//
//  Set the current SD directory to the HP85 provided string
//    If the first character is '\' then it is an absolute path, otherwise it is relative to current path
//
//    Return true if the path is valid
//

void AUXROM_SDCD(void)
{




  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
  return;



  AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_SVAL.string_val.length , 4);    //  Get one HP85 string_val off the stack. This line gets the length and the address of teh string
  uint32_t  string_len = Parameter_blocks.Parameter_Block_SVAL.string_val.length;
  uint32_t  string_addr = Parameter_blocks.Parameter_Block_SVAL.string_val.address;         //  This is the address in HP85 memory. Need to copy it to 

  Serial.printf("String Length:  %d\n", string_len);
  Serial.printf("String Address: %06o\n", string_addr);
  HexDump_HP85_mem(string_addr, 16, true, true);

  if(string_len > MAX_SD_PATH_LENGTH)
  {
    goto error_exit;
  }

  //
  //  Use AR_Buffer_6 as a temp area
  //

  AUXROM_Fetch_Memory(AUXROM_RAM_Window.as_struct.AR_Buffer_6, string_addr, string_len);    //  Copy the Path String to a local buffer
  AUXROM_RAM_Window.as_struct.AR_Buffer_6[string_len] = 0x00;

  Serial.printf("As a C string [%s]\n", AUXROM_RAM_Window.as_struct.AR_Buffer_6);

  temp_file = SD.open((const char *)AUXROM_RAM_Window.as_struct.AR_Buffer_6, O_READ);
  if(!temp_file)
  {
    Serial.printf("Failed to open\n");
    goto error_exit;
  }

  Serial.printf("Is it a Directory?  %s\n", temp_file.isDirectory() ? "Yes":"No");


  AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;     //  Success
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
  return;

error_exit:
  AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 213;    //  Success
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6

  
}

//
//  This function takes New_Path and appends it to Current_Path (if it is a relative path) and
//  puts the result in Resolved_Path. It handles both Absolute and Relative strings in New_Path,
//  and can be used for directory paths as well as file paths. The result is in Resolved_Path, and
//  the last segment of the path (either a directory name or a file name) is in Resolved_Path_Last_Segment
//  It deals with embedded ../ and ./ which allows relative navigation within the directory tree.
//  The resultant path in Resolved_Path has all ../ and ./ removed, so the result is always an
//  absolute path, suitable for directory and file operations.
//
//  Mode 0: Dir           The expected result is a path to a directory, return false if it isn't
//          Chk Exists    The returned path in Resolved_Path both starts and ends with '/'
//                        Resolved_Path_Last_Segment will have the last directory name in the path
//                        
//  Mode 1: Dir           Same as mode 0, but don't test the path for being a valid directory
//                        (used for SDMKDIR, with the last segment being the directory to create)
//                        Resolved_Path_Last_Segment will have the last directory name in the path
//                        
//  Mode 2: File          The expected result is a path to a file, return false if the file does not exist
//          Chk Exists    The returned path in Resolved_Path starts with a '/' but there is no '/' at the end
//                        Resolved_Path_Last_Segment will have the file name
//                        
//  Mode 3: File          Same as mode 2, but don't test the path for being a valid filename
//                        (used for open for WRITE or CREATE when we are not expecting the file to already exist)
//                        The returned path in Resolved_Path starts with a '/' but there is no '/' at the end
//                        Resolved_Path_Last_Segment will have the file name
//
//  Returns true if no errors detected
//
//  Also sets Resolved_Path_ends_with_slash true if there was a trailing '/'. This indicates a properly formed directory path.
//  If this is false, either we have a badly formed directory path, or the last segment is a filename.
//  If it is a filename, it can be found in Resolved_Path_Last_Segment. It is undefined if there is an error exit.
//  
//

bool Resolve_Path(char *New_Path, uint8_t mode)
{
  char *    dest_ptr;                                         //  Points to the trailing 0x00 of the new path we are creating
  char *    dest_ptr2;                                        //  Used while processing ../
  char *    src_ptr;                                          //  Points to next character in New_Path to be processed
  char *    last_seg_ptr;                                     //  Points to next character in the last segment string.
  bool      directory_test;
  File      tempfile;

  if(New_Path[0] == '/')
	{
	  Resolved_Path[0] = '/';                                       //  Handle an absolute path
	  Resolved_Path[1] = 0x00;
	  dest_ptr = Resolved_Path + 1;
	  src_ptr = New_Path + 1;
	}
	else
	{
	  strlcpy(Resolved_Path, Current_Path, MAX_SD_PATH_LENGTH);     //  Handle relative path, so start by copying the Current_Path to the work area. Assume it does not contain any ../ or ./
	                                                            //  Also, since Current_Path is a directory path, it has both leading and trailing '/' (if root, just one '/')
	  dest_ptr = Resolved_Path + strlen(Resolved_Path);
	  src_ptr = New_Path;
	  if(Resolved_Path[0] != '/')
	  {
	    return false;                                           //  make sure that the first character of the path we are building is a '/'
	  }
	}

  Resolved_Path_ends_with_slash = true;

  //
  //  Parse the New_Path. Handle the following:
  //  /         separates directory names
  //  ./        current directory. just remove it
  //  ../       remove a directory level
  //  text      a directory level, or a file name (file name only allowed in final segment)
  //

  while(1)
  {
    //
    //  Look at next part of New_Path
    //
    //  If end of string stop
    //  If next char is "/" it is an error
    //  If next 2 chars are "./" just strip them
    //  If next 3 chars are "../" remove a path segment from the path under construction. Do not remove the first '/'. If nothing to remove, it is an error
    //  Collect characters upto and including the next '/' and append to the path under construction.
    //
    if(src_ptr[0] == 0x00)
    {
      break;                                          //  At end of the New_Path
    }
    if(src_ptr[0] == '/')
    {
      return false;                                   //  Can't have an embedded //. We would have picked the first one when copying a path segment
    }
    if(src_ptr[0] == '.' && src_ptr[1] == '/')
    {
      src_ptr += 2;                                   //  Found "./" which has no use, so just delete it
      continue;
    }
    if(src_ptr[0] == '.' && src_ptr[1] == '.' && src_ptr[2] == '/')
    {
      src_ptr += 3;                                   //  Found "../" which means up 1 directory, so need to delete a path segment
      if(strlen(Resolved_Path) <= 2)                  //  The minimal Resolved_Path that could have a directory to be removed is 3 characters"/x/"
      {                                               //    This could have been done with dest_ptr - Resolved_Path, strlen is clearer and less bug prone.
        return false;                                 //  The built path does not have a trailing directory to be removed
      }
      dest_ptr2 = dest_ptr - 1;                       //  Point at the last character of the path built so far. Should be a '/'
      if(dest_ptr2[0] != '/')
      {
        return false;                                 //  The built path must have a trailing '/'. This would indicate a bug somewhere in this function
      }
      dest_ptr2--;                                    //  Move just before the trailing '/'
      //
      //  We already have checked that the first character of Resolved_Path is '/' , so scan
      //  backwards from the second last character until we find a '/', and write 0x00 just after it
      //
      while(*dest_ptr2 != '/')
      {
        dest_ptr2--;                                  //  Scan backwards
      }
      dest_ptr[1] = 0x00;                             //  Found the prior '/', set the following character to 0x00 thus deleting a path segment
      Resolved_Path_ends_with_slash = true;
      continue;
    }
    //
    //  If we get here, none of the special cases apply, so just append a path segment, including its trailing '/'
    //  A New_Path that does not have a trailing '/' is also valid. This indicates the last segment is a filename
    //
    last_seg_ptr = Resolved_Path_Last_Segment;
    while(1)
    {
      if(src_ptr[0] == '/')                           //  This should not match on the first pass through this while(1) loop, as we have already checked for a leading '/'
      {
        *dest_ptr++ = *src_ptr++;                     //  Copy the '/' and we are done for this path segment
        *dest_ptr = 0x00;                             //  Mark the new end of the string, probably redundant
        Resolved_Path_ends_with_slash = true;
        break;
      }
      if(src_ptr[0] == 0x00)
      {
        *dest_ptr     = 0x00;                         //  Mark the new end of the string, probably redundant
        *last_seg_ptr = 0x00;                         //  Mark the new end of last segment, probably redundant
        Resolved_Path_ends_with_slash = false;        //  No trailing slash, maybe it's a filename
        break;
      }
      //
      //  Not a '/' , and not end of string, so just copy the character
      //
      *dest_ptr++     = *src_ptr;                     //  Copy the character
      *last_seg_ptr++ = *src_ptr++;                   //  Update last segment
      *dest_ptr       = 0x00;                         //  Keep it a valid string
      *last_seg_ptr   = 0x00;                         //  Keep it a valid string
    }
    //
    //  End of this piece of the SD path, loop back for more
    //
  }
  //
  //  We have finished Resolving the path. Now process mode
  //
  switch(mode)
  {
    case 0:                                           //  The expected result is a path to a directory that exists
      tempfile  = SD.open(Resolved_Path, FILE_READ);
      directory_test = tempfile.isDirectory();
      tempfile.close();
      return directory_test;                          //  If it is a directory, return true
      break;
    case 1:
      return true;                                    //  No test, just return true indicating no problem with parsing New_Path
      break;
    case 2:
      return SD.exists(Resolved_Path);                //  If file exists, return true
      break;
    case 3:
      return true;                                    //  No test, just return true indicating no problem with parsing New_Path
      break;
    default:
      return false;
      break;
  }
}



////////
//////////
//////////  USAGE Codes from the AUXROMs
//////////
////////                                            //  BASIC Keyword
////////#define  AUX_USAGE_WROM          (  1)      //  none                                              Write buffer to AUXROM#/ADDR, re-checksum
////////#define  AUX_USAGE_SDCD          (  2)      //  SDCD path$                                        Change the current SD directory
////////#define  AUX_USAGE_SDCUR         (  3)      //  SDCUR$                                            Return the current SD directory
////////#define  AUX_USAGE_SDCAT         (  4)      //  SDCAT [dst$Var, dstSizeVar, dstAttrVar, 0 | 1]    Get a directory listing entry for the current SD path (A.BOPT60=0 for "find first", =1 for "find next")
////////#define  AUX_USAGE_SDFLSH        (  5)      //  SDFLUSH file#                                     Flush everything or a specific file#
////////#define  AUX_USAGE_SDOPEN        (  6)      //  SDOPEN file#, filePath$, mode                     Open an SD file
////////#define  AUX_USAGE_SDREAD        (  7)      //  SDREAD dst$Var, bytesReadVar, maxBytes, file#     Read an SD file
////////#define  AUX_USAGE_SDCLOS        (  8)      //  SDCLOSE file#                                     Close an SD file
////////#define  AUX_USAGE_SDWRIT        (  9)      //  SDWRITE src$, file#                               Write to an SD file
////////#define  AUX_USAGE_SDSEEK        ( 10)      //  = SDSEEK(origin#, offset#, file#)                 Seek in an SD file
////////#define  AUX_USAGE_SDDEL         ( 11)      //  SDDEL fileSpec$                                   Check if mounted disk/tape & error, else delete the file
////////#define  AUX_USAGE_SDMKDIR       ( 12)      //  SDMKDIR folderName$                               Make a folder (only one level at a time, or error)
////////#define  AUX_USAGE_SDRMDIR       ( 13)      //  SDRMDIR folderName$                               Remove a folder (error if not empty)
////////#define  AUX_USAGE_3NUM          (256)      //  AUX3NUMS  varN, varN, varN                        Test keyword with 3 numeric values on R12
////////#define  AUX_USAGE_1SREF         (257)      //  AUX1STRREF var$                                   Test KEYWORD WITH 1 STRING REF ON R12
////////#define  AUX_USAGE_NFUNC         (258)      //  TESTNUMFUN(varN)                                  Numeric function with 1 numeric arg
////////#define  AUX_USAGE_SFUNC         (259)      //  TESTSTRFUN$(var$)                                 String function with 1 string arg
////////
////////#define  TRACE_TESTNUMFUN          (0)      //  Output logging info to Serial 
////////
////////
//////////      These apparent Keywords don't have assigned Usage codes
//////////
//////////RSECTOR      STMT - RSECTOR dst$Var, sec#, msus$ {read a sector from a LIF disk, dst$Var must be at least 256 bytes long}
//////////WSECTOR      STMT - WSECTOR src$, sec#, msus$ {write a sector to a LIF disk, src$ must be 256 bytes}
//////////AUXCMD       STMT - AUXCMD cmd#, buf#, usage#, buf$ {invokes AUXROM command cmd#, passes other three args to that command, command must not return a value}
//////////
//////////                    ROM Label
//////////                    AUXINT          AUXCMD 0, DC, DC, "DC"             - GENERATE FAKE SPAR1 INT      You probably don't want to do this unless you are debugging interrupts. Will crash HP85
//////////                    WMAIL           AUXCMD 1, 0, usage, string         - write string and usage to buf#, don't wait. Does generate HEYEBTKS, so I think it can emulate a command that is passed 1 string
//////////                    GENAUXER        AUXCMD(6, DC, error#, "error msg") - CALL AUXERR WITH error# AND error msg
//////////
//////////AUXERRN      FUNC - AUXERRN {return last custom error error#}
//////////SDATTR       FUNC - a = SDATTR(filePath$) {return bits indicating file attributes, -1 if error}
//////////SDSIZE       FUNC - s = SDSIZE(filePath$) {return size of file, or -1 if error}
//////////AUXINP$      FUNC - AUXINP$(cmd#, buf#, usage#, buf$) {invokes AUXROM command cmd#, passes other three args to that command, command must return a string value}
//////////
//////////                    ROM Label
//////////                    WMAILSTR        AUXINP$(2, 0, usage, string)        - write string and usage to buf#, waits, generate HEYEBTKS, so I think it can emulate a function call that is passed 1 string
//////////                    AUXREV          AUXINP$(4,0,0,"")                   - returns revision for each AUXROM
//////////                                                                          Test: DISP AUXINP$(4,0,0,"")      Result: "1:3 2:3"
//////////
//////////AUXINP       FUNC - AUXINP(cmd#, buf#, usage#, buf$) {invokes AUXROM command cmd#, passes other three args to that command, command must return a numeric value}
//////////
//////////                    ROM Label
//////////                    WMAILNUM        AUXINP(3, 0, usage, string)         - same as WMAIL but waits for returned BUFFER (8-byte number) and BUFLEN==8  and returns that on stack
//////////                    CMDCSUM         AUXINP(5,0,0,"")                    - return results of AUXROM checksums
//////////                                                                          Test: DISP AUXINP (5,0,0,"")      Result: 0    (if ok)
//////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
////////
////////
//////////
//////////  Write only 0xFFE0/0177740. AUXROM_Mailbox_has_Changed
//////////  Relative to the base of the I/O area, this is 0340(8) , 0xE0 , 224(10)
//////////
//////////  This write only I/O address will have a Maibox/Buffer number written by the AUXROM when there is
//////////  a new Keyword/Statement that requires EBTKS services
//////////
////////
////////void ioWriteAuxROM_Alert(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
////////{
////////  new_AUXROM_Alert        = true;                     //  Let the background Polling loop know we have a function to be processed
////////  Mailbox_to_be_processed = val;
////////}
////////
////////
////////
////////void AUXROM_Poll(void)
////////{
////////  int32_t     param_number;
////////  int         i;
////////  struct      S_HP85_String_Variable * HP85_String_Pointer;
////////  double      param_1_val;
////////  int         string_len;
////////  uint32_t    string_addr;
////////  //uint32_t    my_R12;
////////
////////  if(!new_AUXROM_Alert)
////////  {
////////    return;
////////  }
////////  //my_R12 = AUXROM_RAM_Window.as_struct.AR_R12_copy;
////////
////////  //LOGPRINTF_AUX("AUXROM Function called. Expected Mailbox 0, Got Mailbox # %d\n", Mailbox_to_be_processed);  
////////  //LOGPRINTF_AUX("AUXROM Got Usage %d\n", AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed]);
////////  //LOGPRINTF_AUX("R12, got %06o\n", my_R12);
////////  //Serial.printf("Showing 16 bytes prior to R12 address\n");
////////  //HexDump_HP85_mem(my_R12 - 16, 16, true, true);
////////
////////  switch(AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed])
////////  {
////////
////////    case AUX_USAGE_WROM:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_WROM
////////      break;
////////    case AUX_USAGE_SDCD:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDCD
////////      break;
////////    case AUX_USAGE_SDCUR:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDCUR
////////      break;
////////    case AUX_USAGE_SDCAT:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDCAT
////////      break;
////////    case AUX_USAGE_SDFLSH:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDFLSH
////////      break;
////////    case AUX_USAGE_SDOPEN:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDOPEN
////////      break;
////////    case AUX_USAGE_SDREAD:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDREAD
////////      break;
////////    case AUX_USAGE_SDCLOS:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDCLOS
////////      break;
////////    case AUX_USAGE_SDWRIT:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDWRIT
////////      break;
////////    case AUX_USAGE_SDSEEK:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDSEEK
////////      break;
////////    case AUX_USAGE_SDDEL:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDDEL
////////      break;
////////    case AUX_USAGE_SDMKDIR:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDMKDIR
////////      break;
////////    case AUX_USAGE_SDRMDIR:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SDRMDIR
////////      break;
////////
////////
////////
////////    case AUX_USAGE_3NUM:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_3NUM
////////      //  AUX3NUMS n1, n2, n3
////////      //
////////      //  From email with Everett, 8/9/2020 @ 11:33
////////      //
////////      //  AUX3NUMS n1, n2, n3
////////      //  a) When AUX3NUMS runtime gets called, leave the three 8-byte values
////////      //     on the stack, write the R12 value to A.MBR12.
////////      //  b) Send a message to EBTKS:
////////      //      Write "TEST" to BUF0
////////      //      Write 1 to USAGE0
////////      //      Write 1 to Mailbox0
////////      //  b') Write 0 to 177740  (HEYEBTKS)
////////      //  c) Wait for MailBox0==0
////////      //  d) If USAGE0==0, throw away 24-bytes from R12, else load R12
////////      //     from A.MBR12 (assuming EBTKS is "popping the values").
////////      //
////////      //LOGPRINTF_AUX("R12, got %06o   ", AUXROM_RAM_Window.as_struct.AR_R12_copy);
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0] , 3*8);
////////
////////      for(param_number = 0 ; param_number < 3 ; param_number++)
////////      {
////////        HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0], 8, false, false);
////////        Serial.printf("    ");
////////      }
////////      Serial.printf("\n");
////////
////////      for(param_number = 0 ; param_number < 3 ; param_number++)
////////      {
////////        if(Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[4] == 0xFF)
////////        { //  The parameter is a tagged integer
////////          LOGPRINTF_AUX("Integer %7d       ", cvt_R12_int_to_int32(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
////////        }
////////        else
////////        {
////////          LOGPRINTF_AUX("Real %20.14G ", cvt_HP85_real_to_IEEE_double(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
////////        }
////////      }
////////      LOGPRINTF_AUX("\n");
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;         //  Claim success
////////      break;
////////    case AUX_USAGE_1SREF:                      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_1SREF
////////      //  AUX1STRREF
////////      //
////////      //  From email with Everett, 8/9/2020 @ 11:33
////////      //
////////      //  AUX1STRREF A$
////////      //  a) When AUX1STRREF runtime gets called, leave the string reference 
////////      //    on the stack, write the R12 value to A.MBR12.
////////      //  b) Send a message to EBTKS:
////////      //      Write "TEST" to BUF0
////////      //      Write 2 to USAGE0
////////      //      Write 1 to Mailbbox0
////////      //  b') Write 0 to 177740  (HEYEBTKS)
////////      //  c) Wait for Mailbox==0
////////      //  d) If USAGE0==0, throw away the strref from R12, else load R12
////////      //     from A.MBR12.
////////      //
////////      //  Discovered: what is passed is the pointer to the text part of the string. There is a header of
////////      //  8 bytes prior to this point, documented on page 5-32 of the HP-85 Assembler manual. Of these
////////      //  8 bytes, the last 2 (just before the text area starts) is the Valid Length of the text. For
////////      //  a string variable like A$, if not dimensioned, 18 bytes are allocated for the text area, and
////////      //  the 4 bytes prior to the just mentioned Valid Length are two copies of the Max Length, which
////////      //  is 18 (22 octal). So we will be needing the Valid Length.
////////      //  Not documented: If the String Variable is un-initialized, its Actual_Length is -1
////////      //
////////
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_SREF.string.unuseable_abs_rel_addr , 6);
////////      LOGPRINTF_AUX("Unuseable Addr %06o   Length %06o  Address Passed %06o\n" ,
////////                        Parameter_blocks.Parameter_Block_SREF.string.unuseable_abs_rel_addr,
////////                        Parameter_blocks.Parameter_Block_SREF.string.length,
////////                        Parameter_blocks.Parameter_Block_SREF.string.address );
////////      //
////////      //  Per the above description, we need to collect the string starting 8 bytes lower in memory,
////////      //  and retrieve 8 more bytes than indicated by the passed length, which is the max length, not
////////      //  the Valid Length
////////      //
////////      AUXROM_Fetch_Memory(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], Parameter_blocks.Parameter_Block_SREF.string.address - 8, Parameter_blocks.Parameter_Block_SREF.string.length + 8);
////////      //
////////      //  these variables are getting unwieldly. try and make it simple
////////      //
////////      HP85_String_Pointer = (struct  S_HP85_String_Variable *)(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]);
////////      LOGPRINTF_AUX("String Header Flags bytes %03o , %03o TotalLen %4d MaxLen %4d Actual Len %5d\n", HP85_String_Pointer->flags_1, HP85_String_Pointer->flags_2,
////////                    HP85_String_Pointer->Total_Length, HP85_String_Pointer->Max_Length, HP85_String_Pointer->Actual_Length);
////////      if(HP85_String_Pointer->Actual_Length == -1)
////////      {
////////        LOGPRINTF_AUX("String Variable is uninitialized\n");
////////      }
////////      else
////////      {
////////        LOGPRINTF_AUX("[%.*s]\n", HP85_String_Pointer->Actual_Length, HP85_String_Pointer->text);
////////        LOGPRINTF_AUX("Dump of Header and Text area\n");
////////        for(i = 0 ; i < HP85_String_Pointer->Total_Length + 10 ; i++)
////////        {
////////          LOGPRINTF_AUX("%02X ", AUXROM_RAM_Window.as_struct.AR_Buffer_6[i]);
////////          if(i == 7) LOGPRINTF_AUX("\n");        //  a new line after the header portion
////////        }
////////        LOGPRINTF_AUX("\n");
////////      }
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;         //  Claim success
////////      break;
////////    case AUX_USAGE_NFUNC:                            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_NFUNC
////////      //  HP85 function is TESTNUMFUN(123.456) returns the value after two conversions
////////      //  Inbound number is on the R12 stack, result goes into Buffer 6 (8 bytes)
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0] , 8);    //  Get one HP85 Real 8 byte number off the stack
////////      //
////////      //  Convert the HP85 number to IEEE Double, and convert it back and put in Buf 6
////////      //
////////      param_1_val = cvt_HP85_real_to_IEEE_double(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0]);
////////      cvt_IEEE_double_to_HP85_number(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], param_1_val);
////////
////////#if TRACE_TESTNUMFUN
////////      Serial.printf("Input number: ");
////////      HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0], 8, false, false);
////////      Serial.printf("     Output number: ");
////////      HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], 8, false, false);
////////      Serial.printf("difference  %.11e  ", param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]) );
////////      Serial.printf(" Value %20.14G\n", param_1_val);
////////#else
////////      if((param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0])) != 0)
////////      {
////////        Serial.printf("TESTNUMFUN ERROR converting  %20.14G\n", param_1_val);
////////      }
////////#endif
////////
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;     //  Success
////////      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
////////      break;
////////    case AUX_USAGE_SFUNC:                            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  AUX_USAGE_SFUNC
////////      //  show_mailboxes_and_usage();
////////      //  HP85 function is TESTSTRFUN$("Some Text")) returns the string in Buffer 6
////////      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_SVAL.string_val.length , 4);    //  Get one HP85 string_val off the stack
////////      string_len = Parameter_blocks.Parameter_Block_SVAL.string_val.length;
////////      string_addr = Parameter_blocks.Parameter_Block_SVAL.string_val.address;
////////      //  Serial.printf("String Length:  %d\n", string_len);
////////      //  Serial.printf("String Address: %06o\n", string_addr);
////////      //  HexDump_HP85_mem(string_addr, 16, true, true);
////////      AUXROM_Fetch_Memory(AUXROM_RAM_Window.as_struct.AR_Buffer_6, string_addr, string_len);
////////
//////////#if TRACE_TESTNUMFUN
//////////      Serial.printf("Input number: ");
//////////      HexDump_T41_mem((uint32_t)&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0], 8, false, false);
//////////      Serial.printf("     Output number: ");
//////////      HexDump_T41_mem((uint32_t)&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], 8, false, false);
//////////      Serial.printf("difference  %.11e  ", param_1_val - cvt_HP85_real_to_IEEE_double(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]) );
//////////      Serial.printf(" Value %20.14G\n", param_1_val);
//////////#endif
////////
////////      AUXROM_RAM_Window.as_struct.AR_Lengths[6] = string_len;
////////      AUXROM_RAM_Window.as_struct.AR_Usages[6] = string_len;
////////
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;     //  Success
////////      AUXROM_RAM_Window.as_struct.AR_Mailboxes[6] = 0;                        //  Release mailbox 6
////////      break;
////////
////////    default:
////////      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;     //  Failure, unrecognized Usage code
////////  }
////////
////////  new_AUXROM_Alert = false;
////////  AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed] = 0;      //  Relinquish control of the mailbox
////////  show_mailboxes_and_usage();
////////
////////}
////////
//////////
//////////  Fetch num_bytes for HP-85 memory. Made difficult because they might be in
//////////  the built-in DRAM (access via DMA) or they might be in RAM that EBTKS
//////////  supplies (but only for HP85A)
//////////
//////////  The efficient solution would be identify the three scenarios:
//////////    all in built-in DRAM
//////////    all in EBTKS memory
//////////    mixed
//////////  and then write block transfer for each, the last being a total pain
//////////
//////////  The expediant (and slower and less bug prone) is just transfer 1 byte at
//////////  a time and do a test for each. Doing this for now.
//////////
////////
////////void AUXROM_Fetch_Memory(uint8_t * dest, uint32_t src_addr, uint16_t num_bytes)
////////{
////////  while(num_bytes--)
////////  {
////////    if(config.ram16k)
////////    {
////////      //
////////      //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
////////      //
////////      if ((src_addr >= HP85A_16K_RAM_module_base_addr) && (src_addr < IO_ADDR))
////////      {
////////        *dest++ = HP85A_16K_RAM_module[src_addr++ & 0x3FFFU];   //  Got 85A EBTKS RAM, and address match
////////        continue;
////////      }
////////      //  If we get here, got 85A EBTKS RAM, and address does not match, so must be built-in DRAM,
////////      //  so just fall into that code
////////    }
////////    //
////////    //  built-in DRAM
////////    //
////////    *dest++ = DMA_Peek8(src_addr++);
////////  }
////////}
////////
//////////
//////////  Fetch num_bytes for HP-85 memory which are parameters on the R12 stack
//////////
//////////  The parameters to be fetched are on the R12 stack with R12 already in
//////////  AUXROM_RAM_Window.as_struct.AR_R12_copy . This is a growing to higher
//////////  addresses stack, and R12 is pointing at the first free byte.
//////////
////////
////////void AUXROM_Fetch_Parameters(void * Parameter_Block_XXX , uint16_t num_bytes)
////////{
////////  AUXROM_Fetch_Memory(( uint8_t *)Parameter_Block_XXX, AUXROM_RAM_Window.as_struct.AR_R12_copy - num_bytes, num_bytes);
////////}
////////
//////////
//////////  Process a Real from the R12 stack. 12 digit sign magnitude mantissa, the exponent is 3 digits
//////////  in 10's complement format exponent is +499 to -499.
//////////
//////////  IEEE 754 Double precision starts to run out of steam at 10E308 and 10E-308 . Some de-normalized
//////////  numbers extend the range out to about E320 and E-320 but you are no longer playing with 12 digits precision
//////////  Decided to just clip it at E+307/E-307
//////////
////////
////////double cvt_HP85_real_to_IEEE_double(uint8_t number[])
////////{
////////  int         exponent;   // 3 digits, 10's complement
////////  double      result;
////////  bool        negative_mantisa;
////////  char        numtext[25];
////////
////////  if(number[4] == 0377)
////////  {   //  We have a Tagged Integer
////////    return (double)cvt_R12_int_to_int32(number);
////////  }
////////
////////  //
////////  //  Handle a 12 digit Real, with an exponent between -499 and +499
////////  //  IEEE Double only supports 10E308 and 10E-308 , so we can't represent all numbers.
////////  //  Do our best
////////  //
////////  exponent =  (((number[0] & 0x0F)     ) * 1.0);
////////  exponent += (((number[0] & 0xF0) >> 4) * 10.0);
////////  exponent += (((number[1] & 0xF0) >> 4) * 100.0);
////////
////////  //  Serial.printf("10's com exp is %5d ", exponent);      //  Diagnostic
////////
////////  if(exponent > 499)
////////  {
////////    exponent = exponent - 1000;
////////  }
////////
////////  //
////////  //  Limit the exponent, even though this makes the conversion less accurate
////////  //
////////
////////  if(exponent > 307) exponent = 307;
////////  if(exponent < -307) exponent = -307;
////////
////////  //  Serial.printf("limit exp is %5d ", exponent);      //  Diagnostic
////////
////////  //
////////  //  Now deal with the mantissa, which is Sign Magnitude, not 10's complement
////////  //  Not the prettiest solution, but easy to understand. Convert the BCD nibbles
////////  //  to text and then let sscanf() sort things out
////////  //
////////
////////  negative_mantisa = ((number[1] & 0x0F) == 9);
////////  numtext[ 0] = negative_mantisa ? '-' : ' ';
////////
////////  numtext[ 1] = ((number[7] & 0xF0) >> 4) + '0';
////////  numtext[ 2] = '.';
////////  numtext[ 3] = ((number[7] & 0x0F)     ) + '0';
////////
////////  numtext[ 4] = ((number[6] & 0xF0) >> 4) + '0';
////////  numtext[ 5] = ((number[6] & 0x0F)     ) + '0';
////////
////////  numtext[ 6] = ((number[5] & 0xF0) >> 4) + '0';
////////  numtext[ 7] = ((number[5] & 0x0F)     ) + '0';
////////
////////  numtext[ 8] = ((number[4] & 0xF0) >> 4) + '0';
////////  numtext[ 9] = ((number[4] & 0x0F)     ) + '0';
////////
////////  numtext[10] = ((number[3] & 0xF0) >> 4) + '0';
////////  numtext[11] = ((number[3] & 0x0F)     ) + '0';
////////
////////  numtext[12] = ((number[2] & 0xF0) >> 4) + '0';
////////  numtext[13] = ((number[2] & 0x0F)     ) + '0';
////////
////////  snprintf(&numtext[14], 6, "E%04d", exponent);    //  fills 14..19, including a trailing 0x00
////////  //  Serial.printf("numtext is [%s] ", numtext);      //  Diagnostic
////////  sscanf(numtext , "%lf", &result);
////////  return result;
////////}
////////
//////////
//////////  Process a Tagged Integer from the R12 stack -99999 to 99999 in 10's complement format
//////////
////////
////////int32_t cvt_R12_int_to_int32(uint8_t number[])
////////{
////////  uint32_t    result;
////////
////////  result =  (((number[5] & 0x0F)     ) * 1);
////////  result += (((number[5] & 0xF0) >> 4) * 10);
////////
////////  result += (((number[6] & 0x0F)     ) * 100);
////////  result += (((number[6] & 0xF0) >> 4) * 1000);
////////
////////  result += (((number[7] & 0x0F)     ) * 10000);
////////  result += (((number[7] & 0xF0) >> 4) * 100000);   //  0 for positive, 9 for 10's complement negative
////////
////////  if(result > 99999)
////////  {
////////    result = result - 1000000;  //  Minus one million because the sign indicator is a 9, so neg numbers will be 9xxxxx
////////  }
////////  return result;
////////}
////////
//////////
//////////  This function converts a 32 bit integer to a HP Tagged Integer. Tested and then disabled,
//////////  because I don't think we need it, given the following function. Although, for numbers in
//////////  the Tagged Integer range, this would be faster, probably
//////////
//////////  Dest points to an 8 byte area that can hold a tagged integer
//////////
//////////  Examples of how HP85 numbers are encoded
//////////                  0                           1                          -1
//////////    00 00 00 00 FF 00 00 00     00 00 00 00 FF 01 00 00     00 00 00 00 FF 99 99 99     
//////////
//////////                10                         100                        1000
//////////    00 00 00 00 FF 10 00 00     00 00 00 00 FF 00 01 00     00 00 00 00 FF 00 10 00
//////////
//////////                999                         9999                     99999
//////////    00 00 00 00 FF 99 09 00     00 00 00 00 FF 99 99 00     00 00 00 00 FF 99 99 09
//////////
//////////                -10                        -100                       -1000
//////////    00 00 00 00 FF 90 99 99     00 00 00 00 FF 00 99 99     00 00 00 00 FF 00 90 99
//////////
//////////               -999                        -9999                    -99999
//////////    00 00 00 00 FF 01 90 99     00 00 00 00 FF 01 00 99     00 00 00 00 FF 01 00 90
//////////    Sign Digit           ^                           ^                           ^
//////////
//////////  Format "[%06d]" on ARM
//////////          0         1        -1
//////////    [000000]  [000001]  [-00001]
//////////
//////////         10       100      1000
//////////    [000010]  [000100]  [001000]
//////////
//////////        999      9999     99999
//////////    [000999]  [009999]  [099999]
//////////
//////////        -10      -100     -1000
//////////    [-00010]  [-00100]  [-01000]
//////////
//////////       -999     -9999    -99999
//////////    [-00999]  [-09999]  [-99999]
//////////
//////////  Tested with all the above values, and they all converted correctly
//////////
//////////void cvt_int32_to_HP85_tagged_integer(uint8_t * dest, int val)
//////////{
//////////  char  val_chars[20];
//////////  bool  negative;
//////////
//////////  if((val<100000) && (val > -100000))
//////////  {
//////////    //
//////////    //  The result can be formatted as a tagged integer
//////////    //
//////////    //  Handle negative numbers in 10's complement format
//////////    //
//////////    negative = val < 0;
//////////    if(negative)
//////////    {
//////////      val = 100000 + val;
//////////    }
//////////    dest += 4;                            //  We know we are doing a tagged integer, so skip the first 4 bytes
//////////    *dest++ = 0377;                       //  Tag it as an integer
//////////    sprintf(val_chars, "%06d", val);      //  Convert to decimal
//////////    *dest++ = (val_chars[5] & 0x0F) | ((val_chars[4] & 0x0F) << 4);
//////////    *dest++ = (val_chars[3] & 0x0F) | ((val_chars[2] & 0x0F) << 4);
//////////    *dest++ = (val_chars[1] & 0x0F) | (negative? 0x90 : 0x00);
//////////    return;
//////////  }
//////////}
////////
//////////
//////////  This function converts a 64 bit IEEE Double to a HP Real.  While some results could be represented
//////////  with a Tagged Integer, I don't think that it has to do that. Confirmed, no problem returning integers
//////////  between -99999 and +99999 as 8 byte Reals (non tagged), HP85 is fine with the results.
//////////
//////////  Dest points to an 8 byte area that can hold an 8 byte real
//////////
//////////  The conversion was helped by a code example here:
//////////    https://stackoverflow.com/questions/31331723/how-to-control-the-number-of-exponent-digits-after-e-in-c-printf-e
//////////
//////////  Tested with all the above values, and they all converted correctly
//////////
////////
//////////                      1 . yyyyyyyyyyy e 0 EEE \0
//////////                    - 1 . xxxxxxxxxxx e - EEE \0
////////#define ExpectedSize (1+1+1       +11  +1+1+ 3 + 1)
////////
////////void cvt_IEEE_double_to_HP85_number(uint8_t * dest, double val)
////////{
////////  bool  negative;
////////  char  buf[ExpectedSize + 10];
////////
////////  if((negative = val < 0))
////////  {
////////    val = -val;       //  We only want to format positive numbers
////////  }
////////
////////  snprintf(buf, sizeof buf, "%.11e", val);
////////  char *e = strchr(buf, 'e');                           // lucky 'e' not in "Infinity" nor "NaN"
////////  if (e)
////////  {
////////    e++;
////////    int expo = atoi(e);
////////    //
////////    //    But nothing is ever easy. The HP85 DECIMAL Exponent is in 10's complement format
////////    //
////////    if(expo < 0)
////////    {
////////      expo = 1000 + expo;
////////    }
////////    snprintf(e, sizeof buf - (e - buf), "%04d", expo);
////////  }
////////
//////////
//////////  The number is now formatted in buf. See page 3-11 of the HP85 Assembler manual for the Nibble codes
//////////                      Nibble
//////////    buf[0 ]   1         M0
//////////    buf[1 ]   .
//////////    buf[2 ]   2         M1
//////////    buf[3 ]   3         M2
//////////    buf[4 ]   4         M3
//////////    buf[5 ]   5         M4
//////////    buf[6 ]   6         M5
//////////    buf[7 ]   7         M6
//////////    buf[8 ]   8         M7
//////////    buf[9 ]   9         M8
//////////    buf[10]   10        M9
//////////    buf[11]   11        M10
//////////    buf[12]   12        M11
//////////    buf[13]   E
//////////    buf[14]   - or 0    MS  Mantissa Sign due to above code, always 0 in buf[14]
//////////    buf[15]   1         E0  most sig digit of exponent, 10's complement format
//////////    buf[16]   2         E1
//////////    buf[17]   3         E2  least sig digit of exponent
//////////    buf[18]
//////////    buf[19]
//////////    buf[20]
//////////
////////  *dest++ = ((buf[16] & 0x0F) << 4) | (buf[17]        & 0x0F);     //  E1  E2
////////  *dest++ = ((buf[15] & 0x0F) << 4) | (negative? 0x09 : 0x00);     //  E0  MS
////////  *dest++ = ((buf[11] & 0x0F) << 4) | (buf[12]        & 0x0F);     //  M10 M11
////////  *dest++ = ((buf[9]  & 0x0F) << 4) | (buf[10]        & 0x0F);     //  M8  M9
////////  *dest++ = ((buf[7]  & 0x0F) << 4) | (buf[8]         & 0x0F);     //  M6  M7
////////  *dest++ = ((buf[5]  & 0x0F) << 4) | (buf[6]         & 0x0F);     //  M4  M5
////////  *dest++ = ((buf[3]  & 0x0F) << 4) | (buf[4]         & 0x0F);     //  M2  M3
////////  *dest++ = ((buf[0]  & 0x0F) << 4) | (buf[2]         & 0x0F);     //  M0  M1
////////}
////////

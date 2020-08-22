//
//  08/08/2020  These Function implement all of the AUXROM functions
//

#include <Arduino.h>
#include <string.h>

#include "Inc_Common_Headers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 


//
//  Write only 0xFFE0/0177740. AUXROM_Mailbox_has_Changed
//  Relative to the base of the I/O area, this is 0340(8) , 0xE0 , 224(10)
//
//  This write only I/O address will have a Maibox/Buffer number written by the AUXROM when there is
//  a new Keyword/Statement that requires EBTKS services
//

void ioWriteAuxROM_Alert(uint8_t val)                 //  This function is running within an ISR, keep it short and fast.
{
  new_AUXROM_Alert        = true;                     //  Let the background Polling loop know we have a function to be processed
  Mailbox_to_be_processed = val;
}



void AUXROM_Poll(void)
{
  int32_t     param_number;
  int         i;
  struct  S_HP85_String_Variable * HP85_String_Pointer;

  if(!new_AUXROM_Alert)
  {
    return;
  }

  LOGPRINTF("AUXROM Function called. Expected Mailbox 0, Got Mailbox # %d\n", Mailbox_to_be_processed);  
  LOGPRINTF("AUXROM Got Usage %d\n", AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed]);
  LOGPRINTF("R12, got %06o\n", AUXROM_RAM_Window.as_struct.AR_R12_copy);

  switch(AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed])
  {
    case 1:   //  AUX3NUMS
    //
    //  From email with Everett, 8/9/2020 @ 11:33
    //
    //  AUX3NUMS n1, n2, n3
    //  a) When AUX3NUMS runtime gets called, leave the three 8-byte values
    //     on the stack, write the R12 value to A.MBR12.
    //  b) Send a message to EBTKS:
    //      Write "TEST" to BUF0
    //      Write 1 to USAGE0
    //      Write 1 to Mailbox0
    //  b') Write 0 to 177740  (HEYEBTKS)
    //  c) Wait for MailBox0==0
    //  d) If USAGE0==0, throw away 24-bytes from R12, else load R12
    //     from A.MBR12 (assuming EBTKS is "popping the values").
    //
      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;   //  Success
      //Serial.printf("R12, got %06o   ", AUXROM_RAM_Window.as_struct.AR_R12_copy);
      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[0].real_bytes[0] , 3*8);
      //for(param_number = 0 ; param_number < 3 ; param_number++)
      //{
      //  hexdump(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0], 8, false, false);
      //  Serial.printf("    ");
      //}
      //Serial.printf("\n                ");
      for(param_number = 0 ; param_number < 3 ; param_number++)
      {
        if(Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[4] == 0xFF)
        { //  The parameter is a tagged integer
          Serial.printf("  Integer %7d           ", cvt_R12_int_to_uint32(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
        }
        else
        {
          Serial.printf("  Real %20.14G ", cvt_R12_real_to_double(&Parameter_blocks.Parameter_Block_N_N_N_N_N_N.numbers[param_number].real_bytes[0]));
        }
      }
      Serial.printf("\n");
      AUXROM_RAM_Window.as_struct.AR_R12_copy -= 24;  //  If this is wrong, we get "Error 15 : System"
      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;         //  Claim success
      break;
    case 2:   //  AUX1STRREF
    //
    //  From email with Everett, 8/9/2020 @ 11:33
    //
    //  AUX1STRREF A$
    //  a) When AUX1STRREF runtime gets called, leave the string reference 
    //    on the stack, write the R12 value to A.MBR12.
    //  b) Send a message to EBTKS:
    //      Write "TEST" to BUF0
    //      Write 2 to USAGE0
    //      Write 1 to Mailbbox0
    //  b') Write 0 to 177740  (HEYEBTKS)
    //  c) Wait for Mailbox==0
    //  d) If USAGE0==0, throw away the strref from R12, else load R12
    //     from A.MBR12.
    //
    //  Discovered: what is passed is the pointer to the text part of the string. There is a header of
    //  8 bytes prior to this point, documented on page 5-32 of the HP-85 Assembler manual. Of these
    //  8 bytes, the last 2 (just before the text area starts) is the Valid Length of the text. For
    //  a string variable like A$, if not dimensioned, 18 bytes are allocated for the text area, and
    //  the 4 bytes prior to the just mentioned Valid Length are two copies of the Max Length, which
    //  is 18 (22 octal). So we will be needing the Valid Length.
    //  Not documented: If the String Variable is un-initialized, its Actual_Length is -1
    //
      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;         //  Success
      AUXROM_Fetch_Parameters(&Parameter_blocks.Parameter_Block_S.string.unuseable_abs_rel_addr , 6);
      Serial.printf("Unuseable Addr %06o   Length %06o  Address Passed %06o\n" ,
                        Parameter_blocks.Parameter_Block_S.string.unuseable_abs_rel_addr,
                        Parameter_blocks.Parameter_Block_S.string.length,
                        Parameter_blocks.Parameter_Block_S.string.address );
      //
      //  Per the above description, we need to collect the string starting 8 bytes lower in memory,
      //  and retrieve 8 more bytes than indicated by the passed length, which is the max length, not
      //  the Valid Length
      //
      AUXROM_Fetch_Memory(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0], Parameter_blocks.Parameter_Block_S.string.address - 8, Parameter_blocks.Parameter_Block_S.string.length + 8);
      //
      //  these variables are getting unwieldly. try and make it simple
      //
      HP85_String_Pointer = (struct  S_HP85_String_Variable *)(&AUXROM_RAM_Window.as_struct.AR_Buffer_6[0]);
      Serial.printf("String Header Flags bytes %03o , %03o TotalLen %4d MaxLen %4d Actual Len %5d\n", HP85_String_Pointer->flags_1, HP85_String_Pointer->flags_2,
                    HP85_String_Pointer->Total_Length, HP85_String_Pointer->Max_Length, HP85_String_Pointer->Actual_Length);
      if(HP85_String_Pointer->Actual_Length == -1)
      {
        Serial.printf("String Variable is uninitialized\n");
      }
      else
      {
        Serial.printf("[%.*s]\n", HP85_String_Pointer->Actual_Length, HP85_String_Pointer->text);
        Serial.printf("Dump of Header and Text area\n");
        for(i = 0 ; i < HP85_String_Pointer->Total_Length + 10 ; i++)
        {
          Serial.printf("%02X ", AUXROM_RAM_Window.as_struct.AR_Buffer_6[i]);
          if(i == 7) Serial.printf("\n");        //  a new line after the header portion
        }
        Serial.printf("\n");
      }

      AUXROM_RAM_Window.as_struct.AR_R12_copy -= 6;                               //  If this is wrong, we get "Error 15 : System"
      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 1;         //  Claim success
      break;

    default:
      AUXROM_RAM_Window.as_struct.AR_Usages[Mailbox_to_be_processed] = 0;   //  Failure, unrecognized Usage code
  }

  new_AUXROM_Alert = false;
  AUXROM_RAM_Window.as_struct.AR_Mailboxes[Mailbox_to_be_processed] = 0;      //  Relinquish control of the mailbox
}

//
//  Fetch num_bytes for HP-85 memory. Made difficult because they might be in
//  the built-in DRAM (access via DMA) or they might be in RAM that EBTKS
//  supplies (but only for HP85A)
//
//  The efficient solution would be identify the three scenarios:
//    all in built-in DRAM
//    all in EBTKS memory
//    mixed
//  and then write block transfer for each, the last being a total pain
//
//  The expediant (and slower and less bug prone) is just transfer 1 byte at
//  a time and do a test for each. Doing this for now.

void AUXROM_Fetch_Memory(uint8_t * dest, uint16_t src_addr, uint16_t num_bytes)
{
  while(num_bytes--)
  {
    if(config.ram16k)
    {
      //
      //  For HP-85 A, implement 16384 - 256 bytes of RAM, mapped at 0xC000 to 0xFEFF (if enabled)
      //
      if ((src_addr >= HP85A_16K_RAM_module_base_addr) && (src_addr < IO_ADDR))
      {
        *dest++ = HP85A_16K_RAM_module[src_addr++ & 0x3FFFU];   //  Got 85A EBTKS RAM, and address match
        continue;
      }
      //  If we get here, got 85A EBTKS RAM, and address does not match, so must be built-in DRAM,
      //  so just fall into that code
    }
    //
    //  built-in DRAM
    //
    *dest++ = DMA_Peek8(src_addr++);
  }
}

//
//  Fetch num_bytes for HP-85 memory which are parameters on the R12 stack
//
//  The parameters to be fetched are on the R12 stack with R12 already in
//  AUXROM_RAM_Window.as_struct.AR_R12_copy . This is a growing to higher
//  addresses stack, and R12 is pointing at the first free byte.
//

void AUXROM_Fetch_Parameters(void * Parameter_Block_XXX , uint16_t num_bytes)
{
  AUXROM_Fetch_Memory(( uint8_t *)Parameter_Block_XXX, AUXROM_RAM_Window.as_struct.AR_R12_copy - num_bytes, num_bytes);
}

//
//  Process a Real from the R12 stack. 12 digit sign magnitude mantissa, the exponent is 3 digits
//  in 10's complement format exponent is +499 to -499.
//
//  IEEE 754 Double precision starts to run out of steam at 10E308 and 10E-308 . Some de-normalized
//  numbers extend the range out to about E320 and E-320 but you are no longer playing with 12 digits precision
//

double cvt_R12_real_to_double(uint8_t number[])
{
  int         exponent;   // 3 digits, 10's complement
  double      result;
  bool        negative_mantisa;
  char        numtext[22];

  exponent =  (((*(number + 0) & 0x0F)     ) * 1.0);
  exponent += (((*(number + 0) & 0xF0) >> 4) * 10.0);
  exponent += (((*(number + 1) & 0xF0) >> 4) * 100.0);

  if(exponent > 499)
  {
    exponent = exponent - 1000;
  }

  negative_mantisa = ((number[1] & 0x0F) == 9);
  numtext[ 0] = negative_mantisa ? '-' : ' ';

  numtext[ 1] = ((number[7] & 0xF0) >> 4) + '0';
  numtext[ 2] = '.';
  numtext[ 3] = ((number[7] & 0x0F)     ) + '0';

  numtext[ 4] = ((number[6] & 0xF0) >> 4) + '0';
  numtext[ 5] = ((number[6] & 0x0F)     ) + '0';

  numtext[ 6] = ((number[5] & 0xF0) >> 4) + '0';
  numtext[ 7] = ((number[5] & 0x0F)     ) + '0';

  numtext[ 8] = ((number[4] & 0xF0) >> 4) + '0';
  numtext[ 9] = ((number[4] & 0x0F)     ) + '0';

  numtext[10] = ((number[3] & 0xF0) >> 4) + '0';
  numtext[11] = ((number[3] & 0x0F)     ) + '0';

  numtext[12] = ((number[2] & 0xF0) >> 4) + '0';
  numtext[13] = ((number[2] & 0x0F)     ) + '0';

  snprintf(&numtext[14], 6, "E%04d", exponent);    //  fills 14..19, including a trailing 0x00
  sscanf(numtext , "%lf", &result);
  return result;
}

//
//  Process a Tagged Integer from the R12 stack -99999 to 99999 in 10's complement format
//

uint32_t cvt_R12_int_to_uint32(uint8_t number[])
{
  uint32_t    result;

  result =  (((number[5] & 0x0F)     ) * 1);
  result += (((number[5] & 0xF0) >> 4) * 10);

  result += (((number[6] & 0x0F)     ) * 100);
  result += (((number[6] & 0xF0) >> 4) * 1000);

  result += (((number[7] & 0x0F)     ) * 10000);
  result += (((number[7] & 0xF0) >> 4) * 100000);   //  0 for positive, 9 for 10's complement negative

  if(result > 99999)
  {
    result = result - 1000000;  //  Minus one million because the sign indicator is a 9, so neg numbers will be 9xxxxx
  }
  return result;
}

//
//	06/27/2020	These Function Declarations were moved from EBTKS.cpp to here.
//

//
//  1MB5/disk functions
//
bool onReadStatus(void);
bool onReadIB(void);
void onWriteCCR(uint8_t val);
void onWriteOb(uint8_t val);
void onWriteInterrupt(uint8_t val);
bool onReadInterrupt(void);
void initTranslator(void);
void loopTranslator(void);


//
//  Tape functions
//
void tape_handle_command_flush(void);
void tape_handle_command_load(void);

//
//  DMA Functions
//

int32_t DMA_Read_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);
int32_t DMA_Write_Block(uint32_t DMA_Target_Address, uint8_t buffer[], uint32_t bytecount);

uint8_t  DMA_Peek8 (uint32_t address);
uint16_t DMA_Peek16(uint32_t address);

void    DMA_Poke8 (uint32_t address, uint8_t  val);
void    DMA_Poke16(uint32_t address, uint16_t val);

void    DMA_Test_1(void);
void    DMA_Test_2(void);
void    DMA_Test_3(void);
void    DMA_Test_4(void);
void    DMA_Test_5(void);

//
//  Memory Performance tests
//

void    MEM_Test_1(void);
void    MEM_Test_2(void);
void    MEM_Test_3(void);

//
//  CRT Functions
//

void ioWriteCrtSad(uint8_t val);
void ioWriteCrtBad(uint8_t val);
void ioWriteCrtCtrl(uint8_t val);
void ioWriteCrtDat(uint8_t val);
void dumpCrtAlpha(void);
void Write_on_CRT_Alpha(uint16_t row, uint16_t column, const char *  text);
void a3_to_a4(unsigned char *a4, unsigned char *a3);
int  base64_encode(char *output, char *input, int inputLen);
void dumpCrtAlphaAsJSON(void);
void writePixel(int x, int y, int color);
void writeLine(int x0, int y0, int x1, int y1, int color);

//
//  Bank Switched ROM support
//

void ioWriteRSELEC(uint8_t val);
bool readBankRom(uint16_t addr);

//
//  HP-85 Bus interface and ISR functions
//

inline bool onReadData(uint16_t addr);
inline void onPhi_2_Rise(void);
void release_DMA_request(void);
inline void onPhi_1_Rise(void);
inline void onWriteData(uint16_t addr, uint8_t data) __attribute__((always_inline, unused));
inline void onPhi_1_Fall(void);
FASTRUN void pinChange_isr(void);
void setupPinChange(void);
void mySystick_isr(void);

//
//  SD Card support
//

bool loadRom(const char *fname, int slotNum, const char * description);
void saveConfiguration(const char *filename, const Config &config);
bool loadConfiguration(const char *filename, Config &config);
void printDirectory(File dir, int numTabs);
void no_SD_card_message(void);

//
//  Log File support
//

bool open_logfile(void);
void append_to_logfile(const char * text);
void flush_logfile(void);
void close_logfile(void);
void show_logfile(void);
void clean_logfile(void);

//
//  AUXROM Support functions
//

void ioWriteAuxROM_Alert(uint8_t val);
void AUXROM_Poll(void);
void AUXROM_Fetch_Memory(uint8_t * dest, uint16_t src_addr, uint16_t num_bytes);
void AUXROM_Fetch_Parameters(void * Parameter_Block_XXX , uint16_t num_bytes);
double cvt_HP85_real_to_IEEE_double(uint8_t number[]);
int32_t cvt_R12_int_to_int32(uint8_t number[]);
//void cvt_int32_to_HP85_tagged_integer(uint8_t * dest, int val);
void cvt_IEEE_double_to_HP85_number(uint8_t * dest, double val);

//
//  Utility Functions
//

void TXD_Pulser(uint8_t count);

void EBTKS_delay_ns(int32_t count);

void get_serial_string_poll(void);
void serial_string_used(void);

void Serial_Command_Poll(void);

void str_tolower(char *p);
void hexdump(uint8_t *start_address, uint32_t count, bool show_addr, bool final_nl);

void Setup_Logic_Analyzer(void);
void Logic_analyzer_go(void);

void Simple_Graphics_Test(void);

bool is_HP85_idle(void);

#if ENABLE_THREE_SHIFT_DETECTION
bool test_for_three_shift_clicks(void);
#endif

//
//  Functions that Visual Studio Code can't find, but are in the Arduino library.
//  So this is to just shut up some warning messages
//

extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);


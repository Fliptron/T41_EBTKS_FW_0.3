#include "Arduino.h"

class Tape
{
    public:
    Tape();
    bool setFile(char *fname);
    void close(void);
   
    void flush(void);
    void init(void);
    void poll(void);
    void enable(bool enable);

    private:
    File _tapeFile;
    char _filename[30];
    uint32_t _tick;
    uint8_t _prevCtrl;
    uint32_t _downCount;

    bool blockRead(uint32_t blkNum);
    void blockWrite(int blkNum);
};

extern Tape tape;
//FASTRUN bool readTapeStatus(void);
//FASTRUN bool readTapeData(void);
//FASTRUN void writeTapeData(uint8_t val);
//FASTRUN void writeTapeCtrl(uint8_t val);

//void tapeInit(void);
//void tapePoll(void);

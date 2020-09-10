//
//  Philip's Class to handle unbounded long text to be sent to the HP85
//
//  References:
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_0.3\.pio\libdeps\teensy41\SdFat\src\FatLib\FatFile.h      line 405    bool ls(print_t* pr, uint8_t flags = 0, uint8_t indent = 0);
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_0.3\.pio\libdeps\teensy41\SdFat\src\common\SysCall.h      line  63    typedef Print print_t;
//    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\Print.cpp              line  42    size_t Print::write(const uint8_t *buffer, size_t size)
//    C:\Users\Philip Freidin\.platformio\packages\framework-arduinoteensy\cores\teensy4\Print.h                line  54    class Print
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_0.3\.pio\libdeps\teensy41\SdFat\src\FatLib\FatVolume.h    line 125
//    G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_0.3\.pio\libdeps\teensy41\SdFat\src\FatLib\FatFilePrint.cpp   line 80
//
//  G:\PlatformIO_Projects\Teensy_V4.1\T41_EBTKS_FW_0.3\.pio\libdeps\teensy41\SdFat\extras\html\index.html
//

#include <Arduino.h>

class Print_Splitter:public Print
{
  public:
	//virtual size_t write(const uint8_t *buffer, size_t size);
	virtual int availableForWrite(void)		{ return 0; }

  virtual size_t write(uint8_t b)
  {
    Serial.printf("%c", b);
    *_line_ptr++ = b;
    *_line_ptr   = 0x00;
    return 1;
  }

  virtual void flush()
  {
    _line_ptr = &_line[0];
  }

  char * get_ptr()
  {
    return _line;
  }

  private:  
  char        _line[258];
  char *      _line_ptr;

};



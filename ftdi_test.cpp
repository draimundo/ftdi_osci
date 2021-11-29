//g++ -I ../include/libftdi -I ../include/libusb-1.0 -I ../include/boost_1_77_0 ftdi_test.cpp -L ../lib64 -lftdi1 -lftdipp1 -lusb-1.0 -o ../bin64/ftdi_test -Wall

#include <ftdi.hpp>
#include <usb.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <math.h>


// UM232H development module
#define VENDOR 0x0403
#define PRODUCT 0x6014

using namespace Ftdi;

namespace Pin {
   // enumerate the AD bus for conveniance.
   enum bus_t {
      SK = 0x01, // ADBUS0, SPI data clock
      DO = 0x02, // ADBUS1, SPI data out
      DI = 0x04, // ADBUS2, SPI data in
      CS = 0x08, // ADBUS3, SPI chip select
      L0 = 0x10, // ADBUS4, general-ourpose i/o, GPIOL0
      L1 = 0x20, // ADBUS5, general-ourpose i/o, GPIOL1
      L2 = 0x40, // ADBUS6, general-ourpose i/o, GPIOL2
      l3 = 0x80  // ADBUS7, general-ourpose i/o, GPIOL3
   };
}

// Set these pins high
const unsigned char pinInitialState = Pin::CS|Pin::L0|Pin::L1|Pin::L2;
// Use these pins as outputs
const unsigned char pinDirection    = Pin::SK|Pin::DO|Pin::CS|Pin::L0|Pin::L1|Pin::L2;

const uint8_t DAC_DATA = 0x08;


void write_DAC80501(unsigned char* buf, unsigned int* icmd, uint16_t val){
   buf[(*icmd)++] = SET_BITS_LOW;
   buf[(*icmd)++] = pinInitialState & ~Pin::CS;
   buf[(*icmd)++] = pinDirection;

   buf[(*icmd)++] = MPSSE_DO_WRITE;
   buf[(*icmd)++] = 0x02; // length: low byte, 0x0002 ==> 3 bytes
   buf[(*icmd)++] = 0x00; // length: high byte
   buf[(*icmd)++] = DAC_DATA; 
   buf[(*icmd)++] = (uint8_t) ((val >> 8) & 0x00FF);
   buf[(*icmd)++] = (uint8_t) (val & 0x00FF);

   buf[(*icmd)++] = SET_BITS_LOW;
   buf[(*icmd)++] = pinInitialState;
   buf[(*icmd)++] = pinDirection;
   }


void sine_dac(unsigned char* buf, unsigned int* icmd){
   float t0 = 1.5E-6;
   int nsamples = 20000;
   int nperiods = 3000;
   for(int t = 0; t<nsamples; t++){
      uint16_t fval = (uint16_t)((0.5 * sin(2.0*M_PI*((float)nperiods)*((float)t)/((float) nsamples)) + 0.5)*0xFFFF);
      //std::cout << std::hex << fval << "\n";
      write_DAC80501(buf, icmd,  fval);
   }
}

void write_DAC60501(unsigned char* buf, unsigned int* icmd, uint16_t val){
   buf[(*icmd)++] = MPSSE_DO_WRITE;
   buf[(*icmd)++] = 0x02; // length: low byte, 0x0002 ==> 3 bytes
   buf[(*icmd)++] = 0x00; // length: high byte
   buf[(*icmd)++] = DAC_DATA; 
   buf[(*icmd)++] = (unsigned char) (((val & 0x0FF0) << 1) & 0xFF00);
   buf[(*icmd)++] = (unsigned char) (((val & 0x000F) << 1) & 0x00F0); //60501 has 12bits, and needs the 4 last ones to be 0
}

void read_LTC230x(unsigned char* buf, unsigned int* icmd, int* iread){
   buf[(*icmd)++] = MPSSE_DO_READ;
   buf[(*icmd)++] = 0x01; // length low byte, 0x0001 ==> 2 bytes
   buf[(*icmd)++] = 0x00; // length high byte
   (*iread) += 2;
}

void read_LTC230x_bitwise(unsigned char* buf, unsigned int* icmd, int* iread){
   buf[(*icmd)++] = MPSSE_DO_READ;
   buf[(*icmd)++] = 0x00; // length low byte, 0x0000 ==> 1 bytes
   buf[(*icmd)++] = 0x00; // length high byte

   buf[(*icmd)++] = MPSSE_DO_READ | MPSSE_BITMODE;
   buf[(*icmd)++] = 0x03; // length, 0x0003 ==> 3 bits
   (*iread) += 2;
}

int main(void)
{
   // initialize
   struct ftdi_context ftdi;
   int ftdi_status = 0;
   ftdi_status = ftdi_init(&ftdi);
   if ( ftdi_status != 0 ) {
      std::cout << "Failed to initialize device\n";
      return 1;
   }
   ftdi_status = ftdi_usb_open(&ftdi, VENDOR, PRODUCT);
   if ( ftdi_status != 0 ) {
      std::cout << "Can't open device. Got error\n"
		<< ftdi_get_error_string(&ftdi) << '\n';
      return 1;
   }
   ftdi_usb_reset(&ftdi);
   ftdi_set_interface(&ftdi, INTERFACE_ANY);
   ftdi_set_bitmode(&ftdi, 0, 0); // reset
   ftdi_set_bitmode(&ftdi, 0, BITMODE_MPSSE); // enable mpsse on all bits
   ftdi_usb_purge_buffers(&ftdi);

   ftdi_write_data_set_chunksize(&ftdi, 0x5FFFFFFE);
   ftdi_read_data_set_chunksize(&ftdi, 0x5FFFFFFE);

   struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000  };
   nanosleep(&ts, NULL); // sleep 50 ms for setup to complete
   
   // Setup MPSSE; Operation code followed by 0 or more arguments.
   unsigned int icmd = 0;
   unsigned char* buf = (unsigned char*) calloc(10000000, sizeof(unsigned char));
   buf[icmd++] = 0x8A;            // opcode: disable div by 5
   buf[icmd++] = TCK_DIVISOR;     // opcode: set clk divisor
   buf[icmd++] = 0x00;            // argument: low bit. 60 MHz / ((1+0)*2) = 30 MHz
   buf[icmd++] = 0x00;            // argument: high bit.
   buf[icmd++] = DIS_ADAPTIVE;    // opcode: disable adaptive clocking
   buf[icmd++] = DIS_3_PHASE;     // opcode: disable 3-phase clocking
   buf[icmd++] = SET_BITS_LOW;    // opcode: set low bits (ADBUS[0-7])
   buf[icmd++] = pinInitialState; // argument: inital pin states
   buf[icmd++] = pinDirection;    // argument: pin direction
   // Write the setup to the chip.
   if ( ftdi_write_data(&ftdi, buf, icmd) != icmd ) {
      std::cout << "Write failed\n";
   }else{
      std::cout << "Config successful\n";
   }


   // zero the buffer for good measure
   memset(buf, 0, 10000000);
   icmd = 0;

   // buf[icmd++] = SET_BITS_LOW;
   // buf[icmd++] = pinInitialState & ~Pin::CS;
   // buf[icmd++] = pinDirection;
   // buf[icmd++] = MPSSE_DO_WRITE;
   // buf[icmd++] = 0x02; // length: low byte, 0x0002 ==> 3 bytes
   // buf[icmd++] = 0x00; // length: high byte
   // buf[icmd++] = 0x04; 
   // buf[icmd++] = (uint8_t) 0x01;
   // buf[icmd++] = (uint8_t) 0x01;
   // buf[icmd++] = SET_BITS_LOW;
   // buf[icmd++] = pinInitialState;
   // buf[icmd++] = pinDirection;

   
   // for(int i=0; i<0xFFFF; i+=0x0001){
   //    write_DAC80501(buf, &icmd, i);
   // }

   sine_dac(buf, &icmd);
   std::cout << icmd;

   // buf[icmd++] = SET_BITS_LOW;
   // buf[icmd++] = pinInitialState & ~Pin::CS;
   // buf[icmd++] = pinDirection;
   
   // buf[icmd++] = MPSSE_DO_WRITE;
   // buf[icmd++] = 0x02; // length: low byte, 0x0002 ==> 3 bytes
   // buf[icmd++] = 0x00; // length: high byte
   // buf[icmd++] = 0x03; 
   // buf[icmd++] = (uint8_t) 0x00;
   // buf[icmd++] = (uint8_t) 0x01;

   // buf[icmd++] = SET_BITS_LOW;
   // buf[icmd++] = pinInitialState;
   // buf[icmd++] = pinDirection;




   // for(int i=0; i<250000; i++){
   //    // Now we will write and read 1 byte.
   //    // The DO and DI pins should be physically connected on the breadboard.
   //    // Next three commands sets the GPIOL0 pin low. Pulling CS low.
   //    buf[icmd++] = SET_BITS_LOW;
   //    buf[icmd++] = pinInitialState & ~Pin::CS;
   //    buf[icmd++] = pinDirection;
   //    // commands to write and read one byte in SPI0 (polarity = phase = 0) mode
   //    buf[icmd++] = MPSSE_DO_WRITE | MPSSE_WRITE_NEG;
   //    buf[icmd++] = 0x02; // length low byte, 0x0002 ==> 3 bytes
   //    buf[icmd++] = 0x00; // length high byte
   //    buf[icmd++] = (unsigned char) ((i & 0xFF00) >> 2); // byte to send
   //    buf[icmd++] = (unsigned char) (i & 0xFF); // byte to send

   //    // Next three commands sets the GPIOL0 pin high. Pulling CS high.
   //    buf[icmd++] = SET_BITS_LOW;
   //    buf[icmd++] = pinInitialState & ~Pin::L0;
   //    buf[icmd++] = pinDirection;
   //    // commands to write and read one byte in SPI0 (polarity = phase = 0) mode
   //    buf[icmd++] = MPSSE_DO_READ;
   //    buf[icmd++] = 0x01; // length low byte, 0x0001 ==> 2 bytes
   //    buf[icmd++] = 0x00; // length high byte

   //    // Next three commands sets the GPIOL0 pin high. Pulling CS high.
   //    buf[icmd++] = SET_BITS_LOW;
   //    buf[icmd++] = pinInitialState & ~Pin::L1;
   //    buf[icmd++] = pinDirection;
   //    // commands to write and read one byte in SPI0 (polarity = phase = 0) mode
   //    buf[icmd++] = MPSSE_DO_READ;
   //    buf[icmd++] = 0x01; // length low byte, 0x0001 ==> 2 bytes
   //    buf[icmd++] = 0x00; // length high byte

   //          // Next three commands sets the GPIOL0 pin high. Pulling CS high.
   //    buf[icmd++] = SET_BITS_LOW;
   //    buf[icmd++] = pinInitialState & ~Pin::L2;
   //    buf[icmd++] = pinDirection;
   //    // commands to write and read one byte in SPI0 (polarity = phase = 0) mode
   //    buf[icmd++] = MPSSE_DO_READ;
   //    buf[icmd++] = 0x01; // length low byte, 0x0001 ==> 2 bytes
   //    buf[icmd++] = 0x00; // length high byte


   //    // if((icmd % 1000) == 0){
   //    //    std::cout << icmd;
   //    // }
   // }

   // buf[icmd++] = SET_BITS_LOW;
   // buf[icmd++] = pinInitialState;
   // buf[icmd++] = pinDirection;

   std::cout << "Writing: ";
   // for ( int i = 0; i < icmd; ++i ) {
   //    std::cout << std::hex << (unsigned int)buf[i] << ' ';
   // }
   // std::cout << '\n';
   
   // need to purge tx when reading for some etherial reason
   ftdi_usb_purge_tx_buffer(&ftdi);
   int ret = ftdi_write_data(&ftdi, buf, icmd);
   if ( ret != icmd ) {
      std::cout << "Write failed " <<"icmd:" << icmd <<"ret:" << ret << '\n';
   }
   // ftdi_write_data_submit(&ftdi, buf, icmd);
   //ftdi_write_data_submit(&ftdi, buf, icmd);

   // now get the data we read just read from the chip
   // unsigned char* readBuf = (unsigned char*) calloc(30000000, sizeof(unsigned char));
   // if ( ftdi_read_data(&ftdi, readBuf, 3*150000) != 3*150000) std::cout << "Read failed\n";
   // else std::cout << "Answer: " << std::hex << (unsigned int)readBuf[0] << '\n';

   // close ftdi
   free(buf);
   ftdi_usb_reset(&ftdi);
   ftdi_usb_close(&ftdi);
   return 0;
}
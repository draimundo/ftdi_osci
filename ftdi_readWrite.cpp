// Windows:
//g++ ftdi_readWrite.cpp -I include/ -L include/libftdi -lftdi1 -lftdipp1 -o build/ftdi_readWrite -Wall

// Linux:
//g++ ftdi_readWrite.cpp -I include/ -L include/libftdi -lftdi1 -lftdipp1 -lusb-1.0 -o build/ftdi_readWrite -Wall


#include <libftdi/ftdi.hpp>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <fstream>


namespace Osci{
   const uint32_t bufSize = 100000000;
   const unsigned int chunkSize = 0x5FFFFFFE;
}

// Config for FT232
namespace Ft232 {
   // Enumerate the AD bus for convenience.
   enum pins {
      SK = 0x01, // ADBUS0, SPI data clock
      DO = 0x02, // ADBUS1, SPI data out
      DI = 0x04, // ADBUS2, SPI data in
      CS0 = 0x08, // ADBUS3, SPI chip select
      CS1 = 0x10, // ADBUS4, general-ourpose i/o, GPIOL0
      CS2 = 0x20, // ADBUS5, general-ourpose i/o, GPIOL1
      CS3 = 0x40, // ADBUS6, general-ourpose i/o, GPIOL2
      l3 = 0x80  // ADBUS7, general-ourpose i/o, GPIOL3
   };
   
   // Chip-specific info
   const uint16_t vendor = 0x0403;
   const uint16_t product = 0x6014;

   const uint8_t pinInitialState = pins::CS0|pins::CS1|pins::CS2|pins::CS3; // Set these pins high
   const uint8_t pinDirection    = pins::SK|pins::DO|pins::CS0|pins::CS1|pins::CS2|pins::CS3; // Use these pins as outputs

   struct ftdi_context context;
}

// DAC register offsets
namespace Dacx0501{
   const uint8_t DAC_DATA = 0x08;
   const uint8_t CONFIG = 0x03;
}

// ADC configuration bits
namespace Lt230x{
   enum config{
      SINGLE_ENDED = 0x80,
      ODD = 0x40,
      UNIPOLAR = 0x08
   };
}

// Convert Dacx0501 12bit 2-complement output to volts
float outToVolt(uint16_t out){
   uint16_t sign = out&0x800;
   float ret = -1.0*sign + 1.0*(out&0x7FF);
   ret = (((ret+2048.0)/4095.0)*5.0)-2.5;
   return ret;
}


int main(void){
   // Prepare buffers and indices
   uint8_t* writeBuf = (uint8_t*) calloc(Osci::bufSize, sizeof(uint8_t));
   if(writeBuf == NULL){
      std::cout << "Failed to allocate writeBuf\n";
      exit(1);
   }
   int32_t iWrite = 0;
   
   uint8_t* readBuf = (uint8_t*) calloc(Osci::bufSize, sizeof(uint8_t));
   if(readBuf == NULL){
      std::cout << "Failed to allocate readBuf\n";
      exit(1);
   }
   int32_t iRead = 0;

   // Initialize FTDI chip
   int ftdi_status = ftdi_init(&Ft232::context);
   if ( ftdi_status != 0 ) {
      std::cout << "Failed to initialize device\n";
      exit(1);
   }
   ftdi_status = ftdi_usb_open(&Ft232::context, Ft232::vendor, Ft232::product);
   if ( ftdi_status != 0 ) {
      std::cout << "Can't open device. Got error\n"
		<< ftdi_get_error_string(&Ft232::context) << '\n';
      exit(1);
   }
   ftdi_usb_reset(&Ft232::context);
   ftdi_set_interface(&Ft232::context, INTERFACE_ANY);
   ftdi_set_bitmode(&Ft232::context, 0, 0); // reset
   ftdi_set_bitmode(&Ft232::context, 0, BITMODE_MPSSE); // enable mpsse on all bits
   ftdi_tcioflush(&Ft232::context);
   
   // Max out chunksize
   ftdi_write_data_set_chunksize(&Ft232::context, Osci::chunkSize);
   ftdi_read_data_set_chunksize(&Ft232::context, Osci::chunkSize);

   // Sleep 50 ms for setup to complete
   // struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000};
   // nanosleep(&ts, NULL); 
   
   // Setup MPSSE; Operation code followed by 0 or more arguments.
   writeBuf[iWrite++] = 0x8A;            // opcode: disable div by 5
   
   writeBuf[iWrite++] = TCK_DIVISOR;     // opcode: set clk divisor
   writeBuf[iWrite++] = 0x00;            // argument: low bit. 60 MHz / ((1+0)*2) = 30 MHz
   writeBuf[iWrite++] = 0x00;            // argument: high bit.
   
   writeBuf[iWrite++] = DIS_ADAPTIVE;    // opcode: disable adaptive clocking
   
   writeBuf[iWrite++] = DIS_3_PHASE;     // opcode: disable 3-phase clocking
   
   writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
   writeBuf[(iWrite)++] = Ft232::pinInitialState & ~Ft232::CS3; // argument: inital pin states, select DAC
   writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction

   writeBuf[(iWrite)++] = MPSSE_DO_WRITE; // opcode: write on rising clock edge
   writeBuf[(iWrite)++] = 0x02; // argument: length low byte, 0x0002 ==> 3 bytes
   writeBuf[(iWrite)++] = 0x00; // argument: length high byte
   writeBuf[(iWrite)++] = Dacx0501::CONFIG; // argument: first byte content -> configure DAC
   writeBuf[(iWrite)++] = (uint8_t) 0x01; // argument: second byte content -> disable internal ref
   writeBuf[(iWrite)++] = (uint8_t) 0x00; // argument: third byte content 
   
   writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
   writeBuf[(iWrite)++] = Ft232::pinInitialState; // argument: inital pin states, default
   writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction (keep default)

   // Write the setup to the chip.
   if ( ftdi_write_data(&Ft232::context, writeBuf, iWrite) != iWrite ) {
      std::cout << "Write failed\n";
   }else{
      std::cout << "Config successful\n";
   }

   // Zero the buffer after using it for initialisation
   memset(writeBuf, 0, Osci::bufSize);
   iWrite = 0;

   // Prepare input csv reading
   std::ifstream inFile;
   inFile.open("in.csv");
   std::string line;

   // Main loop: Read the input buffer, send it line-by-line to DAC
   // Fill the read buffer line-by-line with the measurements
   while(getline(inFile, line)){
      uint16_t dacVal = (uint16_t) std::stoi(line); // Format read line

      //Write DAC
      writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
      writeBuf[(iWrite)++] = Ft232::pinInitialState & ~Ft232::CS3; // argument: inital pin states, select DAC
      writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction

      writeBuf[(iWrite)++] = MPSSE_DO_WRITE; // opcode: write on rising clock edge
      writeBuf[(iWrite)++] = 0x02; // argument: length low byte, 0x0002 ==> 3 bytes
      writeBuf[(iWrite)++] = 0x00; // argument: length high byte
      writeBuf[(iWrite)++] = Dacx0501::DAC_DATA; // argument: first byte content -> send DAC value
      writeBuf[(iWrite)++] = (uint8_t) (((dacVal & 0x0FF0) >> 4) & 0x00FF); // argument: second byte content -> send MSB
      writeBuf[(iWrite)++] = (uint8_t) ((dacVal & 0x000F) << 4); // argument: second byte content -> send first LSB
      // 60501 has 12bits, and needs the 4 last ones to be 0

      // Read ADC0
      writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
      writeBuf[(iWrite)++] = Ft232::pinInitialState & ~Ft232::CS0; // argument: inital pin states, select ADC0
      writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_DO_WRITE; // opcode: write on rising clock edge, read on rising clock edge
      writeBuf[(iWrite)++] = 0x00; // length low byte, 0x0000 ==> 1 bytes
      writeBuf[(iWrite)++] = 0x00; // length high byte
      writeBuf[(iWrite)++] = 0x00;//Lt230x::UNIPOLAR;

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_BITMODE; // opcode: write on rising clock edge, read on rising clock edge
      writeBuf[(iWrite)++] = 0x03; // length, 0x0003 ==> 4 bits
      (iRead) += 2; // read 12 bits on 2 bytes (the MSB of the second byte is irrelevant)

      // Read ADC1
      writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
      writeBuf[(iWrite)++] = Ft232::pinInitialState & ~Ft232::CS1; // argument: inital pin states, select ADC1
      writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_DO_WRITE ; // opcode: write on rising clock edge, read on rising clock edge
      writeBuf[(iWrite)++] = 0x00; // length low byte, 0x0000 ==> 1 bytes
      writeBuf[(iWrite)++] = 0x00; // length high byte
      writeBuf[(iWrite)++] = 0x00;//Lt230x::UNIPOLAR;

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_BITMODE ; // opcode: write on rising clock edge, read on rising clock edge
      writeBuf[(iWrite)++] = 0x03; // length, 0x0003 ==> 4 bits
      (iRead) += 2; // read 12 bits on 2 bytes (the MSB of the second byte is irrelevant)

      // Read ADC2
      writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
      writeBuf[(iWrite)++] = Ft232::pinInitialState & ~Ft232::CS2; // argument: inital pin states, select ADC2
      writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_DO_WRITE ; // opcode: write on rising clock edge, read on rising clock edge
      writeBuf[(iWrite)++] = 0x00; // length low byte, 0x0000 ==> 1 bytes
      writeBuf[(iWrite)++] = 0x00; // length high byte
      writeBuf[(iWrite)++] = 0x00; //Lt230x::UNIPOLAR;

      writeBuf[(iWrite)++] = MPSSE_DO_READ | MPSSE_BITMODE ; // opcode: write on rising clock edge, read on falling clock edge
      writeBuf[(iWrite)++] = 0x03; // length, 0x0003 ==> 4 bits
      (iRead) += 2; // read 12 bits on 2 bytes (the MSB of the second byte is irrelevant)
   }
   inFile.close(); // close the input file

   // Reset CS pins
   writeBuf[(iWrite)++] = SET_BITS_LOW; // opcode: set low bits (ADBUS[0-7])
   writeBuf[(iWrite)++] = Ft232::pinInitialState; // argument: inital pin states, default
   writeBuf[(iWrite)++] = Ft232::pinDirection; // argument: pin direction (keep default)

   // Write and read data from Ft232
   ftdi_usb_purge_tx_buffer(&Ft232::context);
   ftdi_write_data_submit(&Ft232::context, writeBuf, iWrite);
   
   // Get the data that was read
   // Open output file
   std::ofstream outFile;
   outFile.open("out.csv");
   
   float res = 0; // value to store the average
   if (ftdi_read_data(&Ft232::context, readBuf, iRead) != iRead) std::cout << "Read failed\n"; // fill the readBuf with the read data, test for length
   else {
      for(int i = 0; i < iRead; i+=6){ // decode the ADC values, which 12 bits on 2 bytes (the MSB of the second byte is irrelevant)
         uint16_t adc0 = (((uint16_t) readBuf[i]) << 4) + (readBuf[i+1] & 0x0F);
         uint16_t adc1 = (readBuf[i+2] << 4) + (readBuf[i+3] & 0x0F);
         uint16_t adc2 = (readBuf[i+4] << 4) + (readBuf[i+5] & 0x0F);
         outFile << std::dec << adc0 << "; " << std::dec << adc1 << "; " << std::dec << adc2 << "\n"; // write the results to the output file
         if (i >=6){ // the first value read not always reliable
            res += outToVolt(adc0); 
         }
         
      }
   }
   float avg = res/((iRead)/6.0-1.0);
   // std::cout << std::dec << ((iRead)/6-1) << " avg\n"; //average removing the first value
   // std::cout << std::hex << avg << " res\n"; //average removing the first value
   std::cout << avg;
   outFile.close();
   std::cout << "\nDone\n";

   // Clear system
   free(writeBuf);
   free(readBuf);
   ftdi_tcioflush(&Ft232::context);
   ftdi_usb_reset(&Ft232::context);
   ftdi_usb_close(&Ft232::context);
   return 0;
}

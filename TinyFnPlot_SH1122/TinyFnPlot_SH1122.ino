// TinyFnPlot_SH1122.ino

/* Tiny Function Plotter (SH1106 version)
 *   see http://www.technoblogy.com/show?2CFT
 * 
 * David Johnson-Davies
 * www.technoblogy.com
 * 14th October 2018
 * ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
 * 
 * Sino Wealth SH1122 256x64 I2C OLED
 * Donald Johnson
 * www.dlakwi.net
 * 2020-01-26
 * Seeedstudio V4.2 (ATMega328P) - 3.3V
 * https://www.ebay.ca/itm/I2C-2-08-256x64-Monochrome-Graphic-OLED-Display-Module/133135253955
 * 
 * CC BY 4.0
 * Licensed under a Creative Commons Attribution 4.0 International license: 
 * http://creativecommons.org/licenses/by/4.0/
 */

#include <Wire.h>

// Constants
int const address    = 0x3C;

int const commands   = 0x00;
int const onecommand = 0x80;
int const data       = 0x40;
int const onedata    = 0xC0;

uint8_t const plotColour = 0xF;
int const point_mode = 0;
int const histo_mode = 1;

int const width       = 256;
int const height      =  64;
int const max_y       = height - 1;
int const pixels_byte =   2;

// OLED display **********************************************

// Initialisation sequence for SH1122 OLED module

const uint8_t Init[] PROGMEM =
{
  0xAE,        // display off
  0xB0, 0x00,  // set row address mode
  0x10,        // set column high address         = 00
  0x00,        // set column low  address         = 00
  0xD5, 0x50,  // set frame frequency             = 50 [default]
  0xD9, 0x08,  // set dis-/pre-_charge period     = 22 [default = 08]
  0x40,        // set display start line (40-7F)  = 40  = 0
  0x81, 0xC0,  // contrast control(00-FF)         = C0
  0xA0,        // set segment re-map (A0|A1)      = A0 (rotation)
  0xC0,        // com scan com1-com64 (C0 or C8)  = C0 [default] (C0: 0..N, C8: N..0)
  0xA4,        // entire display off (A4|A5)      = A4 (A4=black background, A5=white background)
  0xA6,        // set normal display (A6|A7)      = A6 (A6=normal, A7=reverse)
  0xA8, 0x3F,  // set multiplex ratio             = 3F [default]
  0xAD, 0x80,  // set dc/dc booster               = 80
  0xD3, 0x00,  // set display offset (00-3F)      = 00 [default]
  0xDB, 0x35,  // set vcom deselect level (35)    = 35 [default]
  0xDC, 0x35,  // set vsegm level                 = 35 [default]
  0x30,        // Set Discharge VSL Level         = 30
  0xAF         // display on
};

#define InitLen sizeof(Init)/sizeof(uint8_t)

// Write a single command

void SingleComm( uint8_t x )
{
  Wire.write( onecommand );
  Wire.write( x );
}

// ?? before turning the display on (0xAF) - the last command in the Init[] table,
// clear the display

void InitDisplay()
{
  Wire.beginTransmission( address );
  Wire.write( commands );
  for ( uint8_t i = 0; i < InitLen; i++ )
    Wire.write( pgm_read_byte( &Init[i] ) );
  Wire.endTransmission();
}

// Plot one point
// 
// 2020-01-26 working correctly

void PlotPoint( int x, int y )
{
  uint8_t o, n;                         // old and new pixels
  uint8_t col = x >> 1;                 // two pixels per byte

  Wire.beginTransmission( address );
  SingleComm( 0xB0 ); SingleComm( y );  // Row start
  SingleComm( 0x00 + (col & 0x0F) );    // Column start lo
  SingleComm( 0x10 + (col >> 4) );      // Column start hi
  SingleComm( 0xE0 );                   // start Read-Modify-Write     // start R-M-W
  Wire.write( onedata );                // needed? (end commands?)
  Wire.endTransmission();

  Wire.requestFrom( address, 2 );
  o = Wire.read();                      // dummy read                  // Read
  o = Wire.read();                      // read current pixels (2)     // 

  if ( ( x & 1 ) == 0 )                 // even                        // Modify
  {                                                                    // 
    n  = plotColour << 4;               // new point is hi nibble      // 
    o &= 0x0F;                          // keep old lo nibble          // 
  }                                                                    // 
  else                                  // odd                         // 
  {                                                                    // 
    n  = plotColour;                    // new point is lo nibble      // 
    o &= 0xF0;                          // keep old hi nibble          // 
  }                                                                    // 

  Wire.beginTransmission( address );
  Wire.write( onedata );                //                             // Write
  Wire.write( o | n );                  //                             // old | new

  SingleComm( 0xEE );                   // end Read-Modify-Write       // end R-M-W
  Wire.endTransmission();
}

// Plot data (SH1122): x (0 to 255), y (0 to 63), mode (0 = point, 1 = histogram)
// data origin is at lower-left (0,63), hence inverting y ==> 63-y

void PlotData( int x, int y, int mode )
{
  if ( mode == point_mode || y == 0 )  // point
  {
    PlotPoint( x, max_y-y );
  }
  else  // histogram
  {
    // plot a column of points
    for ( int j = max_y; j > max_y-y; j-- )
      PlotPoint( x, j );
  }
}

int const i2cbytes = 16;
int const i2cblocks = ( width / i2cbytes ) * pixels_byte;

void ClearDisplay()
{
  for ( int y = 0 ; y < height; y++ )
  {
    Wire.beginTransmission( address );
    Wire.write( commands );
    Wire.write( 0xB0 );  Wire.write( y );     // row
    Wire.write( 0x00 );  Wire.write( 0x10 );  // starting at column 0
    Wire.endTransmission();

    // I2C transmission must be less than 32 bytes in length,
    //  so, break the 256 nibbles/128 bytes into 8 blocks of 32 nibbles/16 bytes
    for ( int c = 0 ; c < width; c += 32 )
    {
      Wire.beginTransmission( address );
      Wire.write( data );
      for ( int b = 0 ; b < i2cbytes; b++ )  // 16 bytes = 32 nibbles
        Wire.write( 0 );
      Wire.endTransmission();
    }
  }
}

// clear the display one point at a time

void ClearDisplayPoints()
{
  for ( int x = 0; x < width; x++ )
    PlotData( x, max_y, histo_mode );
}

// Setup **********************************************

void setup()
{
  Wire.begin();
  InitDisplay();
//ClearDisplay();
}

// Gaussian approximation

int e( int x, int f, int m )
{
  return (f * 248) / (256 + ((x - m) * (x - m)));
}

// Demo plot

void loop()
{
  ClearDisplay();
  for ( int x = 0; x < width; x++ )
  {
    int y = e( x, 40, 48 ) + e( x, 68, 128 ) + e( x, 30, 208 ) - 4;
    PlotData( x, y, point_mode );  // point
  }
  delay( 2500 );

  ClearDisplay();
  for ( int x = 0; x < width; x++ )
  {
    int y = e( x, 30, 48 ) + e( x, 68, 128 ) + e( x, 40, 208 ) - 4;
    PlotData( x, y, histo_mode );  // bar ~ histogram
  }
  delay( 2500 );
}

// --fin--

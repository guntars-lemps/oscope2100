#ifndef _OSCOPE2100_USB_H_
#define _OSCOPE2100_USB_H_

const int SFREQ=50000000; // 50 Mhz
const int BUFSIZE=30000;
enum tscopestate {NONE,OK,DEMO};

extern tscopestate scopestate;

volatile extern gboolean buf_ready;

extern uchar usb_ch1buf[];
extern uchar usb_ch2buf[];
extern int usb_buf_time;
extern int usb_buf_ch1;
extern int usb_buf_ch2;

void usb_start(); // start USB thread
void usb_stop(); // stop USB thread


#endif

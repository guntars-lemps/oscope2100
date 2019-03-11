#include "oscope2100_common.h"
#include "oscope2100_usb.h"
#include "oscope2100_interface.h"

#include <pthread.h>
#include <math.h>
#include <gtk/gtk.h>
#include <stdint.h>
#include <usb.h>
#include <errno.h>
#include <iomanip>

const guint16 VENDOR_ID=0x0547;
const guint16 PRODUCT_ID=0x1002;

const int ATTEMPTS=3;
const int USB_TIMEOUT=30000;// 30 sec

tscopestate scopestate;

volatile gboolean buf_ready;

// data buffer and params for USB
uchar usb_ch1buf[BUFSIZE];
uchar usb_ch2buf[BUFSIZE];
int usb_buf_time;
int usb_buf_ch1;
int usb_buf_ch2;


pthread_t usb_thread;

double demo_d1=0;
double demo_d2=0.0000005;



struct usb_device *scope;
usb_dev_handle *scope_handle;
usb_config_descriptor *scope_config;
usb_interface *scope_interface;

uchar pdata[0x100];
uchar data[0x10000];

std::string buf2str(uchar *buf,int len)
{
  std::ostringstream s;
  for (int n=0;n<len;n++)
  {
    if (n>0) s<<" ";
    s<<std::hex<<std::setfill('0')<<std::setw(2)<<int(buf[n]);
  }
  return s.str();
}

static struct usb_device *find_usb_device(uint16_t vendor, uint16_t product)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  struct usb_bus *busses;
  //
  usb_init();
  usb_find_busses();
  usb_find_devices();
  busses=usb_get_busses();
  //
  for (bus=busses;bus;bus=bus->next)
  {
    for (dev=bus->devices;dev;dev=dev->next)
    {
      if ((dev->descriptor.idVendor==vendor) && (dev->descriptor.idProduct==product)) return dev;
    }
  }
  return NULL;
}

int write_buf(int ep,uchar *buf,int len)
{
  if (scope_handle==NULL) return -1;
  //
  int i;
  int rv=-ETIMEDOUT;
  for (i=0;(rv==-ETIMEDOUT) && (i<ATTEMPTS); i++)
  {
    rv=usb_bulk_write(scope_handle, ep ,(char*)buf,len,USB_TIMEOUT);
  }
  if (rv < 0)
  {
    //std::cout<<"Usb write bulk returns error: "<<rv<<std::endl;
    //std::cout<<"Error: "<<usb_strerror()<<std::endl;
    scopestate=NONE;// reinit
    usb_close(scope_handle);
    scope_handle=NULL;
    return rv;
  }
  return 0;
}


int read_buf(int ep,uchar *buf, int len)
{
  if (scope_handle==NULL) return -1;
  //
  int i;
  int rv=-ETIMEDOUT;
  for (i=0;(rv==-ETIMEDOUT) && (i<ATTEMPTS); i++)
  {
    rv=usb_bulk_read(scope_handle, ep | 0x80, (char*)buf,len,USB_TIMEOUT);
    //std::cout<<"read "<<rv<<" bytes"<<std::endl;
  }
  if (rv < 0)
  {
    //std::cout<<"Usb read bulk returns error: "<<rv<<std::endl;
    //std::cout<<"Error: "<<usb_strerror()<<std::endl;
    scopestate=NONE;// reinit
    usb_close(scope_handle);
    scope_handle=NULL;
    return rv;
  }
  return 0;
}


void *usb_process(void* dummy)
{
  int reinit_timer;
  //
  while (true)
  {
    if (scopestate==NONE) // find & init
    {
      scope=find_usb_device(VENDOR_ID,PRODUCT_ID);
      if (scope!=NULL)
      {
        std::cout<<"Scope found"<<std::endl;
        scope_handle=usb_open(scope);
        if (scope_handle==NULL)
        {
          std::cout<<"Can't open USB device !"<<std::endl;
        }
        else
        {
          // check interface and endpoints
          scope_config=scope->config;
          for (int i=0;i<scope_config->bNumInterfaces; i++)
          {
            scope_interface=&scope_config->interface[i];
            if (scope_interface->num_altsetting < 1) continue;
            struct usb_interface_descriptor *usbInterfaceDescr = &scope_interface->altsetting[0];
            if (usbInterfaceDescr->bInterfaceClass==USB_CLASS_VENDOR_SPEC
               && usbInterfaceDescr->bInterfaceSubClass==0
               && usbInterfaceDescr->bInterfaceProtocol==0
               && usbInterfaceDescr->bNumEndpoints==14)
            {
              int err;
              if ((err=usb_claim_interface(scope_handle, usbInterfaceDescr->bInterfaceNumber))!=0)
              {
                std::cout<<"Not able to claim USB interface, error: "<<err<<std::endl;
              }
              else
              {
                std::cout<<"USB interface selected"<<std::endl;
                // check endpoints
                // send some init commands ...
                pdata[0]=0x03;
                pdata[1]=0xdd;
                pdata[2]=0x05;
                pdata[3]=0x00;
                pdata[4]=0x0f;
                pdata[5]=0x00;
                pdata[6]=0x00;
                pdata[7]=0xff;
                write_buf(0x04,pdata,8);
                //
                data[0]=0x06;
                write_buf(0x05,data,1);
                //
                scopestate=OK;
              }
            }
          }
        }
      }
      else
      {
       // std::cout<<"Scope not found !"<<std::endl;
        if (scope_handle!=NULL)
        {
          usb_close(scope_handle);
          scope_handle=NULL;
        }
      }
      if (scopestate!=OK)
      {
        reinit_timer=100;
        scopestate=DEMO;
      }
    }
    if (scopestate==OK) //////////////////////////////////////////////////////////////////////
    {
      while ((buf_ready) || (capture_mode==CM_STOPPED)) usleep(1000);
      restart_capture=false;
      //
      //data[0]=0x05;
      //write_buf(0x06,data,1);
      //read_buf(0x06,data,1);
      //std::cout<<"Scope init="<<int(data[0])<<std::endl;
      //
      data[0]=0x02;
      write_buf(0x03,data,1);
      // set params
      usb_buf_time=ui_time;
      usb_buf_ch1=ui_ch1_voltage;
      usb_buf_ch2=ui_ch2_voltage;
      //
      pdata[0]=03;
      //
      pdata[1]=(usb_buf_ch2)+((usb_buf_ch1)<<4);
      if (usb_buf_ch2>3) pdata[1]+=0x01;
      if (usb_buf_ch1>3) pdata[1]+=0x10;
      if (ui_ch2_mode==CHMODE_AC) pdata[1]|=0x08;
      if (ui_ch1_mode==CHMODE_AC) pdata[1]|=0x80;
      //
      pdata[2]=(SCOPE_TIME[usb_buf_time]-1) & 0xff;//l
      pdata[3]=((SCOPE_TIME[usb_buf_time]-1) >> 8) & 0xff;//h
      pdata[4]=0xdf; //df 0f d7
      pdata[5]=0x00;
      pdata[6]=0x00;
      if (ui_trigger==TR_CH1) pdata[6]=0x01;
      if (ui_trigger==TR_CH2) pdata[6]=0x02;
      if (ui_trigger==TR_EXT) pdata[6]=0x04;
      pdata[7]=0xff;
      //
      write_buf(0x04,pdata,8);
      //
      if (ui_edge==0) pdata[4]=0x00; // 0
      if (ui_edge==1) pdata[4]=0x02; // 2
      if (ui_trigger==TR_OFF) pdata[4]=0x01;
      if ((pdata[2]!=0) || (pdata[3]!=0)) pdata[4]|=0x08;
      //
      write_buf(0x04,pdata,8);
      //
      data[0]=0x02;
      write_buf(0x03,data,1);
      //
      // std::cout<<"Waiting buffer"<<std::endl;
      do
      {
        data[0]=26;
        write_buf(0x07,data,1);
        read_buf(0x07,data,1);
      }
      while ((data[0]!=1) && (!restart_capture) && (scopestate==OK));
      if (restart_capture || (scopestate!=OK)) continue;
      //
      pdata[4]=0xdf;
      write_buf(0x04,pdata,8);
      // read buffer
      data[0]=0x13;
      write_buf(0x02,data,1);
      //
      read_buf(0x02,data,0xff80);
      //std::cout<<"Buffer received"<<std::endl;
      //
      for (int n=0;n<BUFSIZE;n++)
      {
        if (ui_ch1_mode==CHMODE_GND) usb_ch1buf[n]=0x7f;else usb_ch1buf[n]=data[n+0x0002];
        if (ui_ch2_mode==CHMODE_GND) usb_ch2buf[n]=0x7f;else usb_ch2buf[n]=data[n+0x8002];
      }
      buf_ready=true;
      if (capture_mode==CM_SINGLE) capture_mode=CM_STOPPED;
    }
    if (scopestate==DEMO) // no scope found
    {
      while (buf_ready) usleep(1000);
      usb_buf_time=ui_time;
      usb_buf_ch1=ui_ch1_voltage;
      usb_buf_ch2=ui_ch2_voltage;
      //
      double ch1a=0;
      double ch2a=0;
      demo_d1+=demo_d2;
      if (demo_d1<0) {demo_d1=0;demo_d2=-demo_d2;}
      if (demo_d1>0.0008) {demo_d1=0.0008;demo_d2=-demo_d2;}
      //
      double d1=0.001+demo_d1;
      double d2=0.0014-demo_d1;
      //
      for (int n=0;n<BUFSIZE;n++)
      {
        usb_ch1buf[n]=128+127*sin(ch1a);
        usb_ch2buf[n]=128+127*cos(ch2a);
        ch1a+=d1;
        ch2a+=d2;
      }
      buf_ready=true;
      if (!reinit_timer--) scopestate=NONE;// try to find scope
    }
    usleep(1000);
  }
  return NULL;
}

void usb_start()
{
  pthread_create(&usb_thread, NULL, usb_process, NULL);
}

void usb_stop()
{
  if (scopestate==OK) //////////////////////////////////////////////////////////////////////
  {
    data[0]=0x06;
    write_buf(0x05,data,1);
    write_buf(0x05,data,1);
    write_buf(0x05,data,1);
    usb_close(scope_handle);
    scope_handle=NULL;
  }
}



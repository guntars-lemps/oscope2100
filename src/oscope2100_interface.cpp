#include "oscope2100_common.h"
#include "oscope2100_interface.h"
#include "oscope2100_usb.h"

#include <iostream>
#include <fstream>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>


tcapture_mode capture_mode;
gboolean restart_capture;

// UI state
tmode ui_mode;
int ui_time;

gboolean ui_ch1_on;
gboolean ui_ch1_flip;
int ui_ch1_voltage;
tchmode ui_ch1_mode;
tchmul ui_ch1_mul;

gboolean ui_ch2_on;
gboolean ui_ch2_flip;
int ui_ch2_voltage;
tchmode ui_ch2_mode;
tchmul ui_ch2_mul;

ttrigger ui_trigger;
int ui_edge;

int ui_math;
double ui_rshunt;
double ui_rload;

gboolean ui_scientific_format;
uchar ui_separator;
std::ostringstream expstr;

// TIME normal mode div = 2500px, zoom mode div = 100px (20x less)

GtkWidget *window;
GtkWidget *hscale; // horizontal scroll for "full zoom" mode
GdkPixmap *scopebg; // black scope background + divs
GtkWidget *draw_area;
//
GtkWidget *ch1_label;
GtkWidget *ch2_label;
GtkWidget *time_label;
GtkWidget *x_info_label;
GtkWidget *y_info_label;
GtkWidget *demo_label;
GtkWidget *math_label;
//
GtkWidget *ch1_on;
GtkWidget *ch1_flip;
GtkWidget *ch1_voltage;
GtkWidget *ch1_mode;
GtkWidget *ch1_mul;
//
GtkWidget *ch2_on;
GtkWidget *ch2_flip;
GtkWidget *ch2_voltage;
GtkWidget *ch2_mode;
GtkWidget *ch2_mul;
//
GtkWidget *cb_time;
//
GtkWidget *ch1_offset_label;
GtkWidget *ch2_offset_label;
//
GtkWidget *cb_mode;
//
GtkWidget *cb_trigger;
GtkWidget *cb_edge;

GtkWidget *scientific_format;
GtkWidget *cb_csv_separator;
GtkWidget *csv_button;
GtkWidget *clip_button;

GtkWidget *cb_math;

GtkWidget *entry_rshunt;
GtkWidget *entry_rload;

GtkWidget *button_rshunt;
GtkWidget *button_rload;

GdkDisplay* display;
GtkClipboard* clipboard;

int cursor_x0;
int cursor_y0;
int cursor_x1;
int cursor_y1;

// data buffer and params for GUI
uchar ch1buf[BUFSIZE];
uchar ch2buf[BUFSIZE];
int buf_time;
int buf_ch1;
int buf_ch2;

double ch1_k; // 1, 10, 100
double ch2_k; // 1, 10, 100

double ch1_offset;
double ch2_offset;


bool destroying=false;


void on_window_destroy(GtkObject *object, gpointer user_data)
{
  std::cout<<"Window destroy"<<std::endl;
  destroying=true;
  gtk_main_quit ();
}


gboolean on_draw_area_resize(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  //remove cursor
  cursor_x0=-1;
  cursor_y0=-1;
  cursor_x1=-1;
  cursor_y1=-1;
  gtk_widget_hide(x_info_label);
  gtk_widget_hide(y_info_label);
  // redraw scopebg
  int w=draw_area->allocation.width;
  int h=draw_area->allocation.height;
  //
  if (scopebg!=NULL) g_object_unref(scopebg);
  scopebg = gdk_pixmap_new(NULL, w, h, gdk_colormap_get_visual(gdk_colormap_get_system())->depth);
  // std::cout<<"New draw area size is "<<w<<" x "<<h<<std::endl;
  // draw background
  GdkGC *gc = gdk_gc_new (widget->window);
  //
  GdkColor color = {0xffff,0x0000,0x0000,0x0000};
  gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
  gdk_gc_set_foreground(gc,&color);
	//
  gdk_draw_rectangle (scopebg,gc,TRUE,0,0,w,h);
  // draw divs
  color.red=0xa000;
  color.green=0xa000;
  color.blue=0xa000;
  gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
  gdk_gc_set_foreground(gc,&color);
  //

  if (ui_mode==MODE_NORMAL)
  {
    gtk_widget_hide(hscale);
    gdk_draw_rectangle (scopebg,gc,FALSE,0,0,w-1,h-1);
    gdk_draw_line(scopebg,gc,0,(h/2),w-1,(h/2));
    for (int n=1;n<=11;n++)
    {
      int x=(n*w)/12;
      for (int y=1;y<=h;y+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int n=1;n<=7;n++)
    {
      int y=(n*h)/8;
      for (int x=1;x<=w;x+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int n=1;n<=59;n++)
    {
      int x=(n*w)/60;
      gdk_draw_point(scopebg,gc,x,1);
      gdk_draw_point(scopebg,gc,x,2);
      //
      gdk_draw_point(scopebg,gc,x,(h/2)-2);
      gdk_draw_point(scopebg,gc,x,(h/2)-1);
      gdk_draw_point(scopebg,gc,x,(h/2)+0);
      gdk_draw_point(scopebg,gc,x,(h/2)+1);
      gdk_draw_point(scopebg,gc,x,(h/2)+2);
      //
      gdk_draw_point(scopebg,gc,x,h-3);
      gdk_draw_point(scopebg,gc,x,h-2);
    }
    for (int n=1;n<=39;n++)
    {
      int y=((n*h)/40);
      gdk_draw_point(scopebg,gc,1,y);
      gdk_draw_point(scopebg,gc,2,y);
      gdk_draw_point(scopebg,gc,w-3,y);
      gdk_draw_point(scopebg,gc,w-2,y);
    }
  }
  if (ui_mode==MODE_FZOOM) // atkariigs no zoom pozicijas, 1 div=100px
  {
    int value=gtk_range_get_value(GTK_RANGE(hscale));
    gtk_range_set_range(GTK_RANGE(hscale),0,BUFSIZE-w);
    gtk_range_set_increments(GTK_RANGE(hscale),10,w);
    if (value>(BUFSIZE-w))
    {
      value=(BUFSIZE-w);
      gtk_range_set_value(GTK_RANGE(hscale),value);
    }
    gtk_widget_show(hscale);
    //
    gdk_draw_rectangle (scopebg,gc,FALSE,0,0,w-1,h-1);
    gdk_draw_line(scopebg,gc,0,(h/2),w-1,(h/2));
    for (int x=100-(value%100);x<w;x+=100)
    {
      for (int y=1;y<=h;y+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int n=1;n<=7;n++)
    {
      int y=(n*h)/8;
      for (int x=1;x<=w;x+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int x=20-(value%20);x<w;x+=20)
    {
      gdk_draw_point(scopebg,gc,x,1);
      gdk_draw_point(scopebg,gc,x,2);
      //
      gdk_draw_point(scopebg,gc,x,(h/2)-2);
      gdk_draw_point(scopebg,gc,x,(h/2)-1);
      gdk_draw_point(scopebg,gc,x,(h/2)+0);
      gdk_draw_point(scopebg,gc,x,(h/2)+1);
      gdk_draw_point(scopebg,gc,x,(h/2)+2);
      //
      gdk_draw_point(scopebg,gc,x,h-3);
      gdk_draw_point(scopebg,gc,x,h-2);
    }
    for (int n=1;n<=39;n++)
    {
      int y=((n*h)/40);
      gdk_draw_point(scopebg,gc,1,y);
      gdk_draw_point(scopebg,gc,2,y);
      gdk_draw_point(scopebg,gc,w-3,y);
      gdk_draw_point(scopebg,gc,w-2,y);
    }
  }
  if (ui_mode==MODE_XY)
  {
    gtk_widget_hide(hscale);
    gdk_draw_rectangle (scopebg,gc,FALSE,0,0,w-1,h-1);
    //
    gdk_draw_line(scopebg,gc,w/2,0,w/2,h-1);
    gdk_draw_line(scopebg,gc,0,(h/2),w-1,(h/2));
    for (int n=1;n<=7;n++)
    {
      int x=(n*w)/8;
      for (int y=1;y<=h;y+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int n=1;n<=7;n++)
    {
      int y=(n*h)/8;
      for (int x=1;x<=w;x+=4) gdk_draw_point(scopebg,gc,x,y);
    }
    for (int n=1;n<=39;n++)
    {
      int x=(n*w)/40;
      gdk_draw_point(scopebg,gc,x,1);
      gdk_draw_point(scopebg,gc,x,2);
      //
      gdk_draw_point(scopebg,gc,x,(h/2)-2);
      gdk_draw_point(scopebg,gc,x,(h/2)-1);
      gdk_draw_point(scopebg,gc,x,(h/2)+0);
      gdk_draw_point(scopebg,gc,x,(h/2)+1);
      gdk_draw_point(scopebg,gc,x,(h/2)+2);
      //
      gdk_draw_point(scopebg,gc,x,h-3);
      gdk_draw_point(scopebg,gc,x,h-2);
    }
    for (int n=1;n<=39;n++)
    {
      int y=((n*h)/40);
      gdk_draw_point(scopebg,gc,1,y);
      gdk_draw_point(scopebg,gc,2,y);
      //
      gdk_draw_point(scopebg,gc,(w/2)-2,y);
      gdk_draw_point(scopebg,gc,(w/2)-1,y);
      //
      gdk_draw_point(scopebg,gc,(w/2)+1,y);
      gdk_draw_point(scopebg,gc,(w/2)+2,y);
      //
      gdk_draw_point(scopebg,gc,w-3,y);
      gdk_draw_point(scopebg,gc,w-2,y);
    }
  }
  return TRUE;
}


gboolean on_draw_area_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkGC *gc = gdk_gc_new (widget->window);
  GdkColor color;
  //
  gdk_draw_drawable (widget->window,gc,scopebg,0,0,0,0,-1,-1);
  //
  int w=widget->allocation.width;
  int h=widget->allocation.height;
  int miny0;
  int maxy0;
  int miny1;
  int maxy1;
  int x0;
  int p0,p1;
  uchar b;
  int p_start=gtk_range_get_value(GTK_RANGE(hscale));
  //
  if ((ui_mode==MODE_NORMAL) || (ui_mode==MODE_FZOOM))
  {
    if (ui_ch1_on)
    {
      // Draw CH1
      color.red   = 0x0000;
      color.green = 0xffff;
      color.blue  = 0x0000;
      //
      gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
      gdk_gc_set_foreground (gc, &color);
      //
      miny0=-1;
      maxy0=-1;
      miny1=-1;
      maxy1=-1;
      x0=0;
      //
      for (int x=0;x<w;x++)
      {
        if (ui_mode==MODE_FZOOM)
        {
          p0=p_start+x;
          p1=p_start+x+1;
        }
        else
        {
          p0=(x*BUFSIZE)/w;
          p1=((x+1)*BUFSIZE)/w;
        }
        miny1=-1;
        maxy1=-1;
        if (ui_ch1_flip)
        {
          for (int p=p0;p<p1;p++)
          {
            if (ch1buf[p]==0xff) b=0; else b=(0xfe)-ch1buf[p];
            if ((miny1==-1) || (b<miny1)) miny1=b;
            if ((maxy1==-1) || (b>maxy1)) maxy1=b;
          }
        }
        else
        {
          for (int p=p0;p<p1;p++)
          {
            if ((miny1==-1) || (ch1buf[p]<miny1)) miny1=ch1buf[p];
            if ((maxy1==-1) || (ch1buf[p]>maxy1)) maxy1=ch1buf[p];
          }
        }
        if ((miny0!=-1) && (maxy0!=-1) && (miny1!=-1) && (maxy1!=-1))
        {
          int min_y0=(h*(0xff-miny0))/0x100;
          int min_y1=(h*(0xff-miny1))/0x100;
          int max_y0=(h*(0xff-maxy0))/0x100;
          int max_y1=(h*(0xff-maxy1))/0x100;
          gdk_draw_line(draw_area->window,gc,x0,min_y0,x,min_y1);
          gdk_draw_line(draw_area->window,gc,x0,max_y0,x,max_y1);
          if (min_y1!=max_y1) for (int xx=x0;xx<=x;xx++) gdk_draw_line(draw_area->window,gc,xx,min_y1,xx,max_y1);
          x0=x;
        }
        if (miny1!=-1) miny0=miny1;
        if (maxy1!=-1) maxy0=maxy1;
      }
    }
    if (ui_ch2_on)
    {
      // Draw CH2 ...
      color.red   = 0xffff;
      color.green = 0xffff;
      color.blue  = 0x0000;
      //
      gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
      gdk_gc_set_foreground (gc, &color);
      //
      miny0=-1;
      maxy0=-1;
      miny1=-1;
      maxy1=-1;
      x0=0;
      for (int x=0;x<w;x++)
      {
        if (ui_mode==MODE_FZOOM)
        {
          p0=p_start+x;
          p1=p_start+x+1;
        }
        else
        {
          p0=(x*BUFSIZE)/w;
          p1=((x+1)*BUFSIZE)/w;
        }
        miny1=-1;
        maxy1=-1;
        if (ui_ch2_flip)
        {
          for (int p=p0;p<p1;p++)
          {
            if (ch2buf[p]==0xff) b=0; else b=(0xfe)-ch2buf[p];
            if ((miny1==-1) || (b<miny1)) miny1=b;
            if ((maxy1==-1) || (b>maxy1)) maxy1=b;
          }
        }
        else
        {
          for (int p=p0;p<p1;p++)
          {
            if ((miny1==-1) || (ch2buf[p]<miny1)) miny1=ch2buf[p];
            if ((maxy1==-1) || (ch2buf[p]>maxy1)) maxy1=ch2buf[p];
          }
        }
        // draw lines
        if ((miny0!=-1) && (maxy0!=-1) && (miny1!=-1) && (maxy1!=-1))
        {
          int min_y0=(h*(0xff-miny0))/0x100;
          int min_y1=(h*(0xff-miny1))/0x100;
          int max_y0=(h*(0xff-maxy0))/0x100;
          int max_y1=(h*(0xff-maxy1))/0x100;
          gdk_draw_line(draw_area->window,gc,x0,min_y0,x,min_y1);
          gdk_draw_line(draw_area->window,gc,x0,max_y0,x,max_y1);
          if (min_y1!=max_y1) for (int xx=x0;xx<=x;xx++) gdk_draw_line(draw_area->window,gc,xx,min_y1,xx,max_y1);
          x0=x;
        }
        if (miny1!=-1) miny0=miny1;
        if (maxy1!=-1) maxy0=maxy1;
      }
    }
  }
  if (ui_mode==MODE_XY)
  {
    color.red   = 0x0000;
    color.green = 0xffff;
    color.blue  = 0xffff;
    //
    gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
    gdk_gc_set_foreground (gc, &color);
    //
    int x0=-1;
    int y0=-1;
    int x1=-1;
    int y1=-1;
    for (int n=0;n<BUFSIZE;n++)
    {
      b=ch1buf[n];
      if (ui_ch1_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
      x1=(w*b)/0x100;
      //
      b=ch2buf[n];
      if (ui_ch2_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
      y1=(h*(0xff-b))/0x100;
      //
      if (n>0) gdk_draw_line(draw_area->window,gc,x0,y0,x1,y1);
      x0=x1;
      y0=y1;
    }
  }
  // cursor
  if ((cursor_x0>=0) && (cursor_x1>=0) && (cursor_y0>=0) && (cursor_y1>=0))
  {
    color.red   = 0xffff;
    color.green = 0x4000;
    color.blue  = 0xffff;
    //
    gdk_colormap_alloc_color (gdk_colormap_get_system (),&color, FALSE, TRUE);
    gdk_gc_set_foreground (gc, &color);
    //
    if (cursor_x0<w)
    {
      for (int y=0;y<h;y++) if (y&0x02) gdk_draw_point(draw_area->window,gc,cursor_x0,y);
    }
    if (cursor_y0<h)
    {
      for (int x=0;x<w;x++) if (x&0x02) gdk_draw_point(draw_area->window,gc,x,cursor_y0);
    }
    if (cursor_x1<w)
    {
      gdk_draw_line(draw_area->window,gc,cursor_x1,0,cursor_x1,h-1);
    }
    if (cursor_y0<h)
    {
      gdk_draw_line(draw_area->window,gc,0,cursor_y1,w-1,cursor_y1);
    }
    // Info labels
    if ((h>0) && (w>0))
    {
      if (ui_mode==MODE_NORMAL)
      {
        std::string xtext;
        xtext=" X="+pad(val2str((1.0*(fabs(cursor_x1-cursor_x0))*SCOPE_TIME[buf_time]*BUFSIZE)/(1.0*SFREQ*w),"s"),7)+" ";
        if (fabs(cursor_x1-cursor_x0)>0)
        {
          xtext+=" f="+pad(val2str((1.0*SFREQ*w)/(1.0*(fabs(cursor_x1-cursor_x0))*SCOPE_TIME[buf_time]*BUFSIZE),"Hz"),8)+" ";
        }
        gtk_label_set_markup(GTK_LABEL(x_info_label),("<tt>"+xtext+"</tt>").c_str());
        std::string ytext;
        if (ui_ch1_on) ytext+=" Y(CH1)="+pad(val2str((ch1_k*(cursor_y0-cursor_y1)*8*VOLTAGE_RESOLUTION[buf_ch1])/(h),"V"),7)+" ";
        if (ui_ch2_on) ytext+=" Y(CH2)="+pad(val2str((ch2_k*(cursor_y0-cursor_y1)*8*VOLTAGE_RESOLUTION[buf_ch2])/(h),"V"),7)+" ";
        gtk_label_set_markup(GTK_LABEL(y_info_label),("<tt>"+ytext+"</tt>").c_str());
      }
      if (ui_mode==MODE_FZOOM)
      {
        std::string xtext;
        xtext=" X="+pad(val2str((1.0*(fabs(cursor_x1-cursor_x0))*SCOPE_TIME[buf_time])/(1.0*SFREQ),"s"),7)+" ";
        if (fabs(cursor_x1-cursor_x0)>0)
        {
          xtext+=" f="+pad(val2str((1.0*SFREQ)/(1.0*(fabs(cursor_x1-cursor_x0))*SCOPE_TIME[buf_time]),"Hz"),8)+" ";
        }
        gtk_label_set_markup(GTK_LABEL(x_info_label),("<tt>"+xtext+"</tt>").c_str());
        std::string ytext;
        if (ui_ch1_on) ytext+=" Y(CH1)="+pad(val2str((ch1_k*(cursor_y0-cursor_y1)*8*VOLTAGE_RESOLUTION[buf_ch1])/(h),"V"),8)+" ";
        if (ui_ch2_on) ytext+=" Y(CH2)="+pad(val2str((ch2_k*(cursor_y0-cursor_y1)*8*VOLTAGE_RESOLUTION[buf_ch2])/(h),"V"),8)+" ";
        gtk_label_set_markup(GTK_LABEL(y_info_label),("<tt>"+ytext+"</tt>").c_str());
      }
      if (ui_mode==MODE_XY)
      {
        std::string xtext;
        xtext=" X(CH1)="+pad(val2str((ch1_k*(cursor_x1-cursor_x0)*8*VOLTAGE_RESOLUTION[buf_ch1])/(w),"V"),7)+" ";
        gtk_label_set_markup(GTK_LABEL(x_info_label),("<tt>"+xtext+"</tt>").c_str());
        std::string ytext;
        ytext=" Y(CH2)="+pad(val2str((ch2_k*(cursor_y0-cursor_y1)*8*VOLTAGE_RESOLUTION[buf_ch2])/(h),"V"),7)+" ";
        gtk_label_set_markup(GTK_LABEL(y_info_label),("<tt>"+ytext+"</tt>").c_str());
      }
      gtk_widget_show(x_info_label);
      gtk_widget_show(y_info_label);
      gtk_widget_hide(math_label);
    }
  }
  g_object_unref(gc);
  return TRUE;
}

gboolean on_hscale_changed (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  //std::cout<<"on_hscale_changed"<<std::endl;
  on_draw_area_resize(widget,event,data);
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  return TRUE;
}

gboolean refresh_timer(gpointer data)
{
  if (buf_ready)
  {
    // copy buffer and params
    for (int n=0;n<BUFSIZE;n++)
    {
      ch1buf[n]=usb_ch1buf[n];
      ch2buf[n]=usb_ch2buf[n];
    }
    buf_time=usb_buf_time;
    buf_ch1=usb_buf_ch1;
    buf_ch2=usb_buf_ch2;
    buf_ready=false;
    // change label text
    std::string ch1_text;
    std::string ch2_text;
    std::string time_text;
    //
    ch1_text=" "+val2str(VOLTAGE_RESOLUTION[buf_ch1]*ch1_k,"V")+" ";
    ch2_text=" "+val2str(VOLTAGE_RESOLUTION[buf_ch2]*ch2_k,"V")+" ";
    //time
    if (ui_mode==MODE_NORMAL) time_text=" "+val2str((1.0*SCOPE_TIME[buf_time]*BUFSIZE)/(SFREQ*12),"s")+" ";
    if (ui_mode==MODE_FZOOM) time_text=" "+val2str((1.0*SCOPE_TIME[buf_time]*100)/SFREQ,"s")+" ";
    if (ui_mode==MODE_XY) time_text=" "+val2str(1.0*SFREQ/SCOPE_TIME[buf_time],"s/s",false)+" ";
    //
    gtk_label_set_markup(GTK_LABEL(ch1_label),("<tt>"+ch1_text+"</tt>").c_str());
    gtk_label_set_markup(GTK_LABEL(ch2_label),("<tt>"+ch2_text+"</tt>").c_str());
    gtk_label_set_markup(GTK_LABEL(time_label),("<tt>"+time_text+"</tt>").c_str());
    gtk_widget_show(ch1_label);
    gtk_widget_show(ch2_label);
    gtk_widget_show(time_label);
    if (scopestate!=OK) gtk_widget_show(demo_label); else gtk_widget_hide(demo_label);
    gtk_widget_set_sensitive(GTK_WIDGET(csv_button),TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(clip_button),TRUE);
    // Math
    show_math();
    //
    show_offsets();
    //
    if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  }
  return !destroying;
}


gboolean on_mode_auto(GtkWidget *widget, gpointer data)
{
  capture_mode=CM_AUTO;
  return TRUE;
}

gboolean on_mode_single(GtkWidget *widget, gpointer data)
{
  capture_mode=CM_SINGLE;
  return TRUE;
}

gboolean on_mode_stop(GtkWidget *widget, gpointer data)
{
  capture_mode=CM_STOPPED;
  restart_capture=true;
  return TRUE;
}

void setup_cb_time()
{
  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(cb_time));
  gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(cb_time))));
  //
  for (int n=0;n<TCOUNT;n++)
  {
    if (ui_mode==MODE_NORMAL) gtk_combo_box_append_text(GTK_COMBO_BOX(cb_time),val2str((1.0*SCOPE_TIME[n]*BUFSIZE)/(SFREQ*12),"s").c_str());
    if (ui_mode==MODE_FZOOM) gtk_combo_box_append_text(GTK_COMBO_BOX(cb_time),val2str((1.0*SCOPE_TIME[n]*100)/SFREQ,"s").c_str());
    if (ui_mode==MODE_XY) gtk_combo_box_append_text(GTK_COMBO_BOX(cb_time),val2str(1.0*SFREQ/SCOPE_TIME[n],"s/s",false).c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_time),i);
}


void setup_ch1_voltage()
{
  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_voltage));
  gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(ch1_voltage))));
  //
  for (int n=0;n<VCOUNT;n++)
  {
    gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_voltage),val2str(VOLTAGE_RESOLUTION[n]*ch1_k,"V").c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch1_voltage),i);
}


void setup_ch2_voltage()
{
  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_voltage));
  gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(ch2_voltage))));
  //
  for (int n=0;n<VCOUNT;n++)
  {
    gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_voltage),val2str(VOLTAGE_RESOLUTION[n]*ch2_k,"V").c_str());
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch2_voltage),i);
}


gboolean on_cb_mode_change(GtkWidget *widget, gpointer data)
{
  int i=gtk_combo_box_get_active(GTK_COMBO_BOX(cb_mode));
  if (i==MODE_NORMAL) ui_mode=MODE_NORMAL;
  if (i==MODE_FZOOM) ui_mode=MODE_FZOOM;
  if (i==MODE_XY) ui_mode=MODE_XY;
  setup_cb_time();
  on_draw_area_resize(widget,NULL,data);
  return TRUE;
}

gboolean on_cb_time_change(GtkWidget *widget, gpointer data)
{
  ui_time=gtk_combo_box_get_active(GTK_COMBO_BOX(cb_time));
  restart_capture=true;
  return TRUE;
}

gboolean on_ch1_on_change(GtkWidget *widget, gpointer data)
{
  ui_ch1_on=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ch1_on));
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  return TRUE;
}


gboolean on_ch1_flip_change(GtkWidget *widget, gpointer data)
{
  ui_ch1_flip=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ch1_flip));
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  show_offsets();
  show_math();
  return TRUE;
}


gboolean on_ch2_on_change(GtkWidget *widget, gpointer data)
{
  ui_ch2_on=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ch2_on));
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  return TRUE;
}


gboolean on_ch2_flip_change(GtkWidget *widget, gpointer data)
{
  ui_ch2_flip=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ch2_flip));
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  show_offsets();
  show_math();
  return TRUE;
}


gboolean on_ch1_voltage_change(GtkWidget *widget, gpointer data)
{
  ui_ch1_voltage=gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_voltage));
  restart_capture=true;
  return TRUE;
}

gboolean on_ch2_voltage_change(GtkWidget *widget, gpointer data)
{
  ui_ch2_voltage=gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_voltage));
  restart_capture=true;
  return TRUE;
}

gboolean on_ch1_mode_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mode))==CHMODE_DC) ui_ch1_mode=CHMODE_DC;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mode))==CHMODE_AC) ui_ch1_mode=CHMODE_AC;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mode))==CHMODE_GND) ui_ch1_mode=CHMODE_GND;
  restart_capture=true;
  return TRUE;
}

gboolean on_ch2_mode_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mode))==CHMODE_DC) ui_ch2_mode=CHMODE_DC;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mode))==CHMODE_AC) ui_ch2_mode=CHMODE_AC;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mode))==CHMODE_GND) ui_ch2_mode=CHMODE_GND;
  restart_capture=true;
  return TRUE;
}

void refresh_ch_k()
{
  if (ui_ch1_mul==X1) ch1_k=1;
  if (ui_ch1_mul==X10) ch1_k=10;
  if (ui_ch1_mul==X100) ch1_k=100;
  //
  if (ui_ch2_mul==X1) ch2_k=1;
  if (ui_ch2_mul==X10) ch2_k=10;
  if (ui_ch2_mul==X100) ch2_k=100;
}

gboolean on_ch1_mul_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mul))==X1) ui_ch1_mul=X1;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mul))==X10) ui_ch1_mul=X10;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch1_mul))==X100) ui_ch1_mul=X100;
  refresh_ch_k();
  setup_ch1_voltage();
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  show_offsets();
  show_math();
  return TRUE;
}

gboolean on_ch2_mul_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mul))==X1) ui_ch2_mul=X1;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mul))==X10) ui_ch2_mul=X10;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(ch2_mul))==X100) ui_ch2_mul=X100;
  refresh_ch_k();
  setup_ch2_voltage();
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  show_offsets();
  show_math();
  return TRUE;
}

gboolean on_cb_trigger_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_trigger))==0) ui_trigger=TR_OFF;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_trigger))==1) ui_trigger=TR_CH1;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_trigger))==2) ui_trigger=TR_CH2;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_trigger))==3) ui_trigger=TR_EXT;
  restart_capture=true;
  return TRUE;
}

gboolean on_cb_edge_change(GtkWidget *widget, gpointer data)
{
  ui_edge=gtk_combo_box_get_active(GTK_COMBO_BOX(cb_edge));
  restart_capture=true;
  return TRUE;
}


gboolean on_scientific_format_change(GtkWidget *widget, gpointer data)
{
  ui_scientific_format=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scientific_format));
  return TRUE;
}


gboolean on_cb_csv_separator_change(GtkWidget *widget, gpointer data)
{
  switch (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_csv_separator)))
  {
    case 1: ui_separator=';';break;
    case 2: ui_separator=0x09;break;
    default: ui_separator=',';
  }
  return TRUE;
}


void file_save_dialog ()
{
  GtkWidget *dialog;
  dialog=gtk_file_chooser_dialog_new ("Save File",
				                              GTK_WINDOW(window),
				                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                      NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  //
  std::string defdir=getenv("HOME");
  defdir+="/Desktop";
  //
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),defdir.c_str());
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "data.csv");
  //
  if (gtk_dialog_run (GTK_DIALOG (dialog))==GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename=gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    //save_to_file (filename);

    std::ofstream out(filename, std::ios::out | std::ios::binary);
    if (!out)
    {
      std::cout << "Cannot open output file."<<std::endl;
    }
    else
    {
      out<<expstr.str();
      out.close();
    }
    g_free (filename);
  }
  gtk_widget_destroy (dialog);
}


gboolean on_export_csv(GtkWidget *widget, gpointer data)
{
  // export csv file
  if (prepare_expstr())
  {
    file_save_dialog();
  }
  return TRUE;
}


gboolean on_export_clipboard(GtkWidget *widget, gpointer data)
{
  // export to clipboard
  if (prepare_expstr())
  {
    gtk_clipboard_set_text(clipboard,expstr.str().c_str(),expstr.str().size());
  }
  return TRUE;
}

gboolean on_draw_area_click(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  int x=event->x;
  int y=event->y;
  if (event->type==GDK_BUTTON_PRESS)
  {
    cursor_x0=x;
    cursor_y0=y;
    cursor_x1=-1;
    cursor_y1=-1;
    gtk_widget_hide(x_info_label);
    gtk_widget_hide(y_info_label);
    show_math();
  }
  else
  {
    cursor_x1=x;
    cursor_y1=y;
    if ((event->type==GDK_BUTTON_RELEASE) && (cursor_x1==cursor_x0) && (cursor_y1==cursor_y0))
    {
      cursor_x0=-1;
      cursor_y0=-1;
      cursor_x1=-1;
      cursor_y1=-1;
      gtk_widget_hide(x_info_label);
      gtk_widget_hide(y_info_label);
      show_math();
    }
  }
  if (draw_area!=NULL) gtk_widget_queue_draw (draw_area);
  return TRUE;
}

// Offsets
void show_offsets()
{
  //CH1 offeset
  double ch1o;
  double ch2o;
  ch1o=ch1_offset;
  ch2o=ch2_offset;
  //
  if (ui_ch1_flip) {if (ch1o>=0xfe) ch1o=0; else ch1o=(0xfe)-ch1o;}
  if (ui_ch2_flip) {if (ch2o>=0xfe) ch2o=0; else ch2o=(0xfe)-ch2o;}
  //
  std::string ch1o_str;
  std::string ch2o_str;
  //
  ch1o_str="CH1="+val2str((ch1_k*(ch1o-0x7f)*8*VOLTAGE_RESOLUTION[buf_ch1])/256,"V");;
  ch2o_str="CH2="+val2str((ch2_k*(ch2o-0x7f)*8*VOLTAGE_RESOLUTION[buf_ch2])/256,"V");
  //
  gtk_label_set_text(GTK_LABEL(ch1_offset_label),ch1o_str.c_str());
  gtk_label_set_text(GTK_LABEL(ch2_offset_label),ch2o_str.c_str());
}


gboolean on_offset_clear(GtkWidget *widget, gpointer data)
{
  ch1_offset=0x7f;
  ch2_offset=0x7f;
  show_offsets();
  show_math();
  return TRUE;
}


gboolean on_offset_set(GtkWidget *widget, gpointer data)
{
  double ch1_summ=0;
  double ch2_summ=0;
  //
  for (int n=0;n<BUFSIZE;n++)
  {
    ch1_summ+=ch1buf[n];
    ch2_summ+=ch2buf[n];
  }
  ch1_offset=ch1_summ/BUFSIZE;
  ch2_offset=ch2_summ/BUFSIZE;
  //
  show_offsets();
  show_math();
  return TRUE;
}


gboolean on_cb_math_change(GtkWidget *widget, gpointer data)
{
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_math))==M_OFF) ui_math=M_OFF;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_math))==M_CH1V_CH2I) ui_math=M_CH1V_CH2I;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_math))==M_CH1I_CH2V) ui_math=M_CH1I_CH2V;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_math))==M_CH1I) ui_math=M_CH1I;
  if (gtk_combo_box_get_active(GTK_COMBO_BOX(cb_math))==M_CH2I) ui_math=M_CH2I;
  show_math();
  gtk_widget_set_sensitive(button_rshunt,((ui_math==M_CH1I_CH2V) || (ui_math==M_CH1V_CH2I)));
  gtk_widget_set_sensitive(button_rload,((ui_math==M_CH1I_CH2V) || (ui_math==M_CH1V_CH2I)));
  return TRUE;
}

gboolean on_math_rchange(GtkWidget *widget,gpointer data)
{
  std::string rshunt_str=gtk_entry_get_text(GTK_ENTRY(entry_rshunt));
  std::string rload_str=gtk_entry_get_text(GTK_ENTRY(entry_rload));
  ui_rshunt=atof(rshunt_str.c_str());
  ui_rload=atof(rload_str.c_str());
  show_math();
  return TRUE;
}


gboolean on_math_rshunt_calc(GtkWidget *widget,gpointer data)
{
  if ((ui_math!=M_CH1I_CH2V) && (ui_math!=M_CH1V_CH2I)) return TRUE;
  if (ui_rload<0.001) return TRUE;
  //
  int b;
  double ch1;
  double ch2;
  double v_load=0;
  double v_shunt=0;
  double i;
  //
  double ch1o;
  double ch2o;
  ch1o=ch1_offset;
  ch2o=ch2_offset;
  //
  if (ui_ch1_flip) {if (ch1o>=0xfe) ch1o=0; else ch1o=(0xfe)-ch1o;}
  if (ui_ch2_flip) {if (ch2o>=0xfe) ch2o=0; else ch2o=(0xfe)-ch2o;}
  //
  for (int n=0;n<BUFSIZE;n++)
  {
    b=ch1buf[n];
    if (ui_ch1_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
    ch1=(ch1_k*(b-ch1o)*8*VOLTAGE_RESOLUTION[buf_ch1])/256;
    b=ch2buf[n];
    if (ui_ch2_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
    ch2=(ch2_k*(b-ch2o)*8*VOLTAGE_RESOLUTION[buf_ch2])/256;
    //
    if (ui_math==M_CH1I_CH2V) {v_shunt+=ch1;v_load+=ch2;}
    if (ui_math==M_CH1V_CH2I) {v_shunt+=ch2;v_load+=ch1;}
  }
  v_shunt/=BUFSIZE;
  v_load/=BUFSIZE;
  //
  i=(v_load/ui_rload);
  //
  gtk_entry_set_text(GTK_ENTRY(entry_rshunt),fround4(fabs(v_shunt/i)).c_str());
  //
  return TRUE;
}


gboolean on_math_rload_calc(GtkWidget *widget,gpointer data)
{
  if ((ui_math!=M_CH1I_CH2V) && (ui_math!=M_CH1V_CH2I)) return TRUE;
  if (ui_rshunt<0.001) return TRUE;
  //
  int b;
  double ch1;
  double ch2;
  double v_load=0;
  double v_shunt=0;
  double i;
  //
  double ch1o;
  double ch2o;
  ch1o=ch1_offset;
  ch2o=ch2_offset;
  //
  if (ui_ch1_flip) {if (ch1o>=0xfe) ch1o=0; else ch1o=(0xfe)-ch1o;}
  if (ui_ch2_flip) {if (ch2o>=0xfe) ch2o=0; else ch2o=(0xfe)-ch2o;}
  //
  for (int n=0;n<BUFSIZE;n++)
  {
    b=ch1buf[n];
    if (ui_ch1_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
    ch1=(ch1_k*(b-ch1o)*8*VOLTAGE_RESOLUTION[buf_ch1])/256;
    b=ch2buf[n];
    if (ui_ch2_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
    ch2=(ch2_k*(b-ch2o)*8*VOLTAGE_RESOLUTION[buf_ch2])/256;
    //
    if (ui_math==M_CH1I_CH2V) {v_shunt+=ch1;v_load+=ch2;}
    if (ui_math==M_CH1V_CH2I) {v_shunt+=ch2;v_load+=ch1;}
  }
  v_shunt/=BUFSIZE;
  v_load/=BUFSIZE;
  //
  i=(v_shunt/ui_rshunt);
  //
  gtk_entry_set_text(GTK_ENTRY(entry_rload),fround4(fabs(v_load/i)).c_str());
  //
  return TRUE;
}


void show_math()
{
  if ((ui_math==M_OFF) || GTK_WIDGET_VISIBLE(x_info_label) || (GTK_WIDGET_VISIBLE(y_info_label)))
  {
    gtk_widget_hide(math_label);
  }
  else
  {
    std::string math_text;
    //
    if ((ui_rshunt<0.001) || (ui_rload<0))
    {
      if (ui_rshunt<0.001) math_text=" Minimal R(shunt) 0.001 ! "; else math_text=" R(load) must be positive ! ";
    }
    else
    {
      int b;
      double ch1;
      double ch2;
      double v=0;
      double i=0;
      double p_in=0;
      double p_out=0;
      double p_heat=0;
      //
      double ch1o;
      double ch2o;
      ch1o=ch1_offset;
      ch2o=ch2_offset;
      //
      if (ui_ch1_flip) {if (ch1o>=0xfe) ch1o=0; else ch1o=(0xfe)-ch1o;}
      if (ui_ch2_flip) {if (ch2o>=0xfe) ch2o=0; else ch2o=(0xfe)-ch2o;}
      //
      for (int n=0;n<BUFSIZE;n++)
      {
        b=ch1buf[n];
        if (ui_ch1_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
        ch1=(ch1_k*(b-ch1o)*8*VOLTAGE_RESOLUTION[buf_ch1])/256;
        b=ch2buf[n];
        if (ui_ch2_flip) {if (b==0xff) b=0; else b=(0xfe)-b;}
        ch2=(ch2_k*(b-ch2o)*8*VOLTAGE_RESOLUTION[buf_ch2])/256;
        //
        if (ui_math==M_CH1I_CH2V) {i=ch1/ui_rshunt;v=ch2;}
        if (ui_math==M_CH1V_CH2I) {i=ch2/ui_rshunt;v=ch1;}
        if (ui_math==M_CH1I) {i=ch1/ui_rshunt;}
        if (ui_math==M_CH2I) {i=ch2/ui_rshunt;}
        //
        if ((i*v)>0) p_in+=(i*v);else p_out-=(i*v);
        p_heat+=i*i*ui_rload;
      }
      p_in/=BUFSIZE;
      p_out/=BUFSIZE;
      p_heat/=BUFSIZE;
      //
      math_text="";
      if ((ui_math==M_CH1I_CH2V) || (ui_math==M_CH1V_CH2I))
      {
        math_text+=" P(total)="+pad(val2str(p_in-p_out,"W"),7)+" "+
                   "P(in)="+pad(val2str(p_in,"W"),7)+" "+
                   "P(out)="+pad(val2str(p_out,"W"),7);
      }
      math_text+=" P(heat)="+pad(val2str(p_heat,"W"),7)+" ";
    }
    gtk_label_set_markup(GTK_LABEL(math_label),("<tt>"+math_text+"</tt>").c_str());
    gtk_widget_show(math_label);
  }
}


void create_interface()
{
  // Window
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window),WINDOW_TITLE.c_str());
 // gtk_window_set_default_size (GTK_WINDOW (window),800,550);
  gtk_window_set_default_size (GTK_WINDOW (window),800,550);
  g_signal_connect (G_OBJECT (window), "delete_event",G_CALLBACK (on_window_destroy), NULL);
  //
  GtkWidget *button;
  GtkWidget *label;
  // vbox1
  GtkWidget *vbox1 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox1);
  // DrawArea
  draw_area = gtk_drawing_area_new ();
  gtk_box_pack_start (GTK_BOX (vbox1),draw_area, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (draw_area), "expose_event", G_CALLBACK (on_draw_area_expose), NULL);
  g_signal_connect (G_OBJECT (draw_area), "configure_event", G_CALLBACK (on_draw_area_resize), NULL);
  gtk_widget_add_events(draw_area, GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(draw_area, GDK_BUTTON_MOTION_MASK);
  gtk_widget_add_events(draw_area, GDK_BUTTON_RELEASE_MASK);
  g_signal_connect(G_OBJECT(draw_area), "button-press-event", G_CALLBACK(on_draw_area_click), NULL);
  g_signal_connect(G_OBJECT(draw_area), "button-release-event", G_CALLBACK(on_draw_area_click), NULL);
  g_signal_connect(G_OBJECT(draw_area), "motion-notify-event", G_CALLBACK(on_draw_area_click), NULL);
  // Hscroll
  hscale=gtk_hscale_new_with_range(0,20,20);
  gtk_scale_set_draw_value  (GTK_SCALE(hscale),false);
  gtk_box_pack_start (GTK_BOX(vbox1),hscale, false, false, 0);
  g_signal_connect (G_OBJECT (hscale),"value-changed",G_CALLBACK (on_hscale_changed), NULL);
  // hbox1 for labels
  GtkWidget *hbox1 = gtk_hbox_new (false, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, false, false, 2);
  // Labels
  // CH1
  PangoAttrList *ch1_alist = pango_attr_list_new ();
  PangoAttribute *attr;
  //attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  //pango_attr_list_insert (ch1_alist, attr);
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (ch1_alist, attr);
  attr = pango_attr_foreground_new(0x0000,0xffff,0x0000);
  pango_attr_list_insert (ch1_alist, attr);
  //
  ch1_label = gtk_label_new (" CH1 ");
  gtk_misc_set_alignment (GTK_MISC (ch1_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(ch1_label),ch1_alist);
  gtk_box_pack_start (GTK_BOX (hbox1), ch1_label, false, false, 5);
  // CH2
  PangoAttrList *ch2_alist = pango_attr_list_new ();
  //attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  //pango_attr_list_insert (ch2_alist, attr);
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (ch2_alist, attr);
  attr = pango_attr_foreground_new(0xffff,0xffff,0x0000);
  pango_attr_list_insert (ch2_alist, attr);
  //
  ch2_label = gtk_label_new (" CH2 ");
  gtk_misc_set_alignment (GTK_MISC (ch2_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(ch2_label),ch2_alist);
  gtk_box_pack_start (GTK_BOX (hbox1), ch2_label, false, false, 5);
  // TIME
  PangoAttrList *time_alist = pango_attr_list_new ();
  //attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  //pango_attr_list_insert (time_alist, attr);
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (time_alist, attr);
  attr = pango_attr_foreground_new(0xc000,0xc000,0xff00);
  pango_attr_list_insert (time_alist, attr);
  //
  time_label = gtk_label_new (" TIME ");
  gtk_misc_set_alignment (GTK_MISC (time_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(time_label),time_alist);
  gtk_box_pack_start (GTK_BOX (hbox1),time_label, false, false, 5);
  // MATH
  PangoAttrList *math_alist = pango_attr_list_new ();
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (math_alist, attr);
  attr = pango_attr_foreground_new(0xff00,0x6000,0x6000);
  pango_attr_list_insert (math_alist, attr);
  //
  math_label = gtk_label_new (" MATH ");
  gtk_misc_set_alignment (GTK_MISC (math_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(math_label),math_alist);
  gtk_box_pack_start (GTK_BOX (hbox1),math_label, false, false, 5);
  // CURSOR X INFO
  PangoAttrList *x_info_alist = pango_attr_list_new ();
  //attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  //pango_attr_list_insert (x_info_alist, attr);
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (x_info_alist, attr);
  attr = pango_attr_foreground_new(0xffff,0x8000,0xffff);
  pango_attr_list_insert (x_info_alist, attr);
  //
  x_info_label = gtk_label_new (" X INFO ");
  gtk_misc_set_alignment (GTK_MISC (x_info_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(x_info_label),x_info_alist);
  gtk_box_pack_start (GTK_BOX (hbox1),x_info_label, false, false, 5);
  // CURSOR Y INFO
  PangoAttrList *y_info_alist = pango_attr_list_new ();
  //attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  //pango_attr_list_insert (y_info_alist, attr);
  attr = pango_attr_background_new(0x0000,0x0000,0x0000);
  pango_attr_list_insert (y_info_alist, attr);
  attr = pango_attr_foreground_new(0xffff,0x8000,0xffff);
  pango_attr_list_insert (y_info_alist, attr);
  //
  y_info_label = gtk_label_new (" Y INFO ");
  gtk_misc_set_alignment (GTK_MISC (y_info_label), 0.0, 0.5);
  gtk_label_set_attributes (GTK_LABEL(y_info_label),y_info_alist);
  gtk_box_pack_start (GTK_BOX (hbox1),y_info_label, false, false, 5);
  // Demo label
  demo_label = gtk_label_new ("DEMO");
  gtk_misc_set_alignment (GTK_MISC (demo_label), 1.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (demo_label), 5, 0);
  gtk_box_pack_start (GTK_BOX (hbox1),demo_label, true, true, 5);
  // hbox2 for scope control
  GtkWidget *hbox2 = gtk_hbox_new (false, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox2, false, false, 5);
  // CH1 control
  GtkWidget *frame3 = gtk_frame_new("CH1");
  gtk_frame_set_shadow_type(GTK_FRAME(frame3), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame3, false, false,3);
  //
  GtkWidget *a1=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a1),10,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame3), a1);
  //
  GtkWidget *vbox4 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a1), vbox4);
  //
  ch1_on=gtk_check_button_new_with_label("On");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ch1_on),ui_ch1_on);
  g_signal_connect (G_OBJECT (ch1_on),"toggled",G_CALLBACK (on_ch1_on_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox4),ch1_on, false, false,1);
  //
  ch1_flip=gtk_check_button_new_with_label("Flip");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ch1_flip),ui_ch1_flip);
  g_signal_connect (G_OBJECT (ch1_flip),"toggled",G_CALLBACK (on_ch1_flip_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox4),ch1_flip, false, false,1);
  //
  ch1_voltage=gtk_combo_box_new_text();
  setup_ch1_voltage();
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch1_voltage),ui_ch1_voltage);
  g_signal_connect (G_OBJECT (ch1_voltage),"changed",G_CALLBACK (on_ch1_voltage_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox4),ch1_voltage, false, false, 2);
  //on_cb_math_change
  ch1_mode=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mode),"DC");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mode),"AC");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mode),"GND");
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch1_mode),ui_ch1_mode);
  g_signal_connect (G_OBJECT (ch1_mode),"changed",G_CALLBACK (on_ch1_mode_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox4),ch1_mode, false, false, 2);
  //
  ch1_mul=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mul),"X1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mul),"X10");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch1_mul),"X100");
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch1_mul),ui_ch1_mul);
  g_signal_connect (G_OBJECT (ch1_mul),"changed",G_CALLBACK (on_ch1_mul_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox4),ch1_mul, false, false, 2);
  // CH2 control
  GtkWidget *frame4 = gtk_frame_new("CH2");
  gtk_frame_set_shadow_type(GTK_FRAME(frame4), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame4, false, false,3);
  //
  GtkWidget *a2=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a2),10,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame4), a2);
  //
  GtkWidget *vbox5 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a2), vbox5);
  //
  ch2_on=gtk_check_button_new_with_label("On");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ch2_on),ui_ch2_on);
  g_signal_connect (G_OBJECT (ch2_on),"toggled",G_CALLBACK (on_ch2_on_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox5),ch2_on, false, false,1);
  //
  ch2_flip=gtk_check_button_new_with_label("Flip");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ch2_flip),ui_ch2_flip);
  g_signal_connect (G_OBJECT (ch2_flip),"toggled",G_CALLBACK (on_ch2_flip_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox5),ch2_flip, false, false,1);
  //
  ch2_voltage=gtk_combo_box_new_text();
  setup_ch2_voltage();
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch2_voltage),ui_ch2_voltage);
  g_signal_connect (G_OBJECT (ch2_voltage),"changed",G_CALLBACK (on_ch2_voltage_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox5),ch2_voltage, false, false, 2);
  //
  ch2_mode=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mode),"DC");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mode),"AC");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mode),"GND");
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch2_mode),ui_ch2_mode);
  g_signal_connect (G_OBJECT (ch2_mode),"changed",G_CALLBACK (on_ch2_mode_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox5),ch2_mode, false, false, 2);
  //
  ch2_mul=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mul),"X1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mul),"X10");
  gtk_combo_box_append_text(GTK_COMBO_BOX(ch2_mul),"X100");
  gtk_combo_box_set_active(GTK_COMBO_BOX(ch2_mul),ui_ch2_mul);
  g_signal_connect (G_OBJECT (ch2_mul),"changed",G_CALLBACK (on_ch2_mul_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox5),ch2_mul, false, false, 2);
  //
  GtkWidget *vbox8 = gtk_vbox_new(false,5);
  gtk_box_pack_start (GTK_BOX(hbox2),vbox8,false,false,3);

  // Time control
  GtkWidget *frame2 = gtk_frame_new("Time");
  gtk_frame_set_shadow_type(GTK_FRAME(frame2), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (vbox8),frame2, false, true,0);
  //
  GtkWidget *a3=gtk_alignment_new(0,0,1,1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a3),10,5,5,5);
  gtk_container_add (GTK_CONTAINER (frame2), a3);
  //
  GtkWidget *vbox3 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a3), vbox3);
  //
  cb_time=gtk_combo_box_new_text();
  setup_cb_time();
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_time),ui_time);
  g_signal_connect (G_OBJECT (cb_time),"changed",G_CALLBACK (on_cb_time_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox3),cb_time, false, true, 2);

  // Offset control
  GtkWidget *frame7 = gtk_frame_new("Offset");
  gtk_frame_set_shadow_type(GTK_FRAME(frame7), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (vbox8),frame7, true, true,0);
  //
  GtkWidget *a7=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a7),6,5,2,2);
  gtk_container_add (GTK_CONTAINER (frame7), a7);
  //
  GtkWidget *vbox9 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a7), vbox9);
  //
  GtkWidget *hbox3 = gtk_hbox_new (false, 2);
  gtk_box_set_homogeneous (GTK_BOX (hbox3),true);
  gtk_box_pack_start (GTK_BOX (vbox9),hbox3, false, false,6);
  //
  button = gtk_button_new_with_label ("Clear");
  g_signal_connect (G_OBJECT (button),"clicked",G_CALLBACK (on_offset_clear), NULL);
  gtk_box_pack_start (GTK_BOX (hbox3), button, false, true, 2);
  //
  button = gtk_button_new_with_label ("Set");
  g_signal_connect (G_OBJECT (button),"clicked",G_CALLBACK (on_offset_set), NULL);
  gtk_box_pack_start (GTK_BOX (hbox3), button, false, true, 2);
  //
  ch1_offset_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (ch1_offset_label), 0.0, 1);
  gtk_misc_set_padding (GTK_MISC (ch1_offset_label), 3, 0);
  gtk_box_pack_start (GTK_BOX (vbox9),ch1_offset_label, true, true, 2);
  //
  ch2_offset_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (ch2_offset_label), 0.0, 1);
  gtk_misc_set_padding (GTK_MISC (ch2_offset_label),3, 0);
  gtk_box_pack_start (GTK_BOX (vbox9),ch2_offset_label, true, true, 2);
  // Mode control
  GtkWidget *frame1 = gtk_frame_new("Mode");
  gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame1, false, false,3);
  //
  GtkWidget *a4=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a4),10,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame1), a4);
  //
  GtkWidget *vbox2 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a4), vbox2);
  //
  GtkWidget *a4_1=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a4_1),0,10,0,0);
  gtk_box_pack_start (GTK_BOX (vbox2),a4_1, false, false, 2);
  //
  cb_mode=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_mode),"Normal");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_mode),"Full zoom");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_mode),"XY");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_mode),ui_mode);
  g_signal_connect (G_OBJECT (cb_mode),"changed",G_CALLBACK (on_cb_mode_change), NULL);
  gtk_container_add (GTK_CONTAINER (a4_1), cb_mode);
  //
  button = gtk_button_new_with_label ("Auto");
  g_signal_connect (G_OBJECT (button),"clicked",G_CALLBACK (on_mode_auto), NULL);
  gtk_box_pack_start (GTK_BOX (vbox2), button, false, false, 2);
  //
  button = gtk_button_new_with_label ("Single");
  g_signal_connect (G_OBJECT (button),"clicked",G_CALLBACK (on_mode_single), NULL);
  gtk_box_pack_start (GTK_BOX (vbox2), button, false, false, 2);
  //
  button = gtk_button_new_with_label ("Stop");
  g_signal_connect (G_OBJECT (button),"clicked",G_CALLBACK (on_mode_stop), NULL);
  gtk_box_pack_start (GTK_BOX (vbox2), button, false, false, 2);
  // Trigger control
  GtkWidget *frame5 = gtk_frame_new("Trigger");
  gtk_frame_set_shadow_type(GTK_FRAME(frame5), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame5, false, false,3);
  //
  GtkWidget *a5=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a5),10,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame5), a5);
  //
  GtkWidget *vbox6 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a5), vbox6);
  //
  cb_trigger=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_trigger),"OFF");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_trigger),"CH1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_trigger),"CH2");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_trigger),"EXT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_trigger),ui_trigger);
  g_signal_connect (G_OBJECT (cb_trigger),"changed",G_CALLBACK (on_cb_trigger_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox6),cb_trigger, false, false, 2);
  //
  cb_edge=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_edge),"+");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_edge),"-");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_edge),ui_edge);
  g_signal_connect (G_OBJECT (cb_edge),"changed",G_CALLBACK (on_cb_edge_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox6),cb_edge, false, false, 2);
  // Export control
  GtkWidget *frame6 = gtk_frame_new("Export");
  gtk_frame_set_shadow_type(GTK_FRAME(frame6), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame6, false, false,3);
  //
  GtkWidget *a6=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a6),10,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame6), a6);
  //
  GtkWidget *vbox7 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a6), vbox7);
  //
  scientific_format=gtk_check_button_new_with_label("Scientific");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scientific_format),ui_scientific_format);
  g_signal_connect (G_OBJECT (scientific_format),"toggled",G_CALLBACK (on_scientific_format_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox7),scientific_format, false, false,2);
  //
  csv_button=gtk_button_new_with_label ("CSV File");
  g_signal_connect (G_OBJECT (csv_button),"clicked",G_CALLBACK (on_export_csv), NULL);
  gtk_widget_set_sensitive(GTK_WIDGET(csv_button),FALSE);
  gtk_box_pack_start (GTK_BOX (vbox7),csv_button, false, false, 2);
  //
  clip_button=gtk_button_new_with_label ("Clipboard");
  g_signal_connect (G_OBJECT (clip_button),"clicked",G_CALLBACK (on_export_clipboard), NULL);
  gtk_widget_set_sensitive(GTK_WIDGET(clip_button),FALSE);
  gtk_box_pack_start (GTK_BOX (vbox7),clip_button, false, false, 2);
  //
  GtkWidget *label1 = gtk_label_new ("Separator");
  gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label1), 2, 0);
  gtk_box_pack_start (GTK_BOX (vbox7), label1, false, false, 5);
  //
  cb_csv_separator=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_csv_separator),",");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_csv_separator),";");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_csv_separator),"TAB");
  if (ui_separator==',') gtk_combo_box_set_active(GTK_COMBO_BOX(cb_csv_separator),0);
  if (ui_separator==';') gtk_combo_box_set_active(GTK_COMBO_BOX(cb_csv_separator),1);
  if (ui_separator==0x09) gtk_combo_box_set_active(GTK_COMBO_BOX(cb_csv_separator),2);
  g_signal_connect (G_OBJECT (cb_csv_separator),"changed",G_CALLBACK (on_cb_csv_separator_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox7),cb_csv_separator, false, false, 2);
  // Math control
  GtkWidget *frame8 = gtk_frame_new("Math");
  gtk_frame_set_shadow_type(GTK_FRAME(frame8), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (hbox2),frame8, false, false,3);
  //
  GtkWidget *a8=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a8),0,2,5,5);
  gtk_container_add (GTK_CONTAINER (frame8), a8);
  //
  GtkWidget *vbox10 = gtk_vbox_new (false, 0);
  gtk_container_add (GTK_CONTAINER (a8), vbox10);
  //
  cb_math=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_math),"OFF");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_math),"CH1=V;CH2=I");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_math),"CH1=I;CH2=V");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_math),"CH1=I");
  gtk_combo_box_append_text(GTK_COMBO_BOX(cb_math),"CH2=I");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cb_math),ui_math);
  g_signal_connect (G_OBJECT (cb_math),"changed",G_CALLBACK (on_cb_math_change), NULL);
  gtk_box_pack_start (GTK_BOX (vbox10),cb_math, false, false, 10);
  //label
  label = gtk_label_new ("R(shunt)");
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox10),label, false, false, 2);
  //
  GtkWidget *hbox4 = gtk_hbox_new (false, 0);
  gtk_box_set_spacing(GTK_BOX(hbox4),5);
  gtk_container_add (GTK_CONTAINER (vbox10), hbox4);
  //text input
  entry_rshunt = gtk_entry_new ();
  gtk_entry_set_width_chars(GTK_ENTRY(entry_rshunt),8);
  gtk_entry_set_text(GTK_ENTRY(entry_rshunt),f2str(ui_rshunt).c_str());
  g_signal_connect (G_OBJECT (entry_rshunt),"changed",G_CALLBACK (on_math_rchange), NULL);
  gtk_box_pack_start (GTK_BOX (hbox4),entry_rshunt, false, false, 0);
  //
  button_rshunt = gtk_button_new_with_label ("Calculate");
  g_signal_connect (G_OBJECT (button_rshunt),"clicked",G_CALLBACK (on_math_rshunt_calc), NULL);
  gtk_box_pack_start (GTK_BOX (hbox4), button_rshunt, true, true, 0);
  //
  GtkWidget *a9=gtk_alignment_new(0,0,0,0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(a9),0,0,0,0);
  gtk_box_pack_start (GTK_BOX (vbox10),a9, true, true, 3);
  //label
  label = gtk_label_new ("R(load)");
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1);
  gtk_box_pack_start (GTK_BOX (vbox10),label, false, false, 2);
  //
  GtkWidget *hbox5 = gtk_hbox_new (false, 0);
  gtk_box_set_spacing(GTK_BOX(hbox5),5);
  gtk_container_add (GTK_CONTAINER (vbox10), hbox5);
  //text input
  entry_rload = gtk_entry_new ();
  gtk_entry_set_width_chars(GTK_ENTRY(entry_rload),8);
  gtk_entry_set_text(GTK_ENTRY(entry_rload),f2str(ui_rload).c_str());
  g_signal_connect (G_OBJECT (entry_rload),"changed",G_CALLBACK (on_math_rchange), NULL);
  gtk_box_pack_start (GTK_BOX (hbox5),entry_rload, false, false, 0);
  //
  button_rload = gtk_button_new_with_label ("Calculate");
  g_signal_connect (G_OBJECT (button_rload),"clicked",G_CALLBACK (on_math_rload_calc), NULL);
  gtk_box_pack_start (GTK_BOX (hbox5), button_rload, false, true, 0);
  //
  display = gtk_widget_get_display (window);
  clipboard = gtk_clipboard_get_for_display(display,GDK_SELECTION_CLIPBOARD);
  //
  on_cb_math_change(GTK_WIDGET(cb_math),NULL);
  //
  g_timeout_add(20,refresh_timer,NULL);
  show_offsets();
}



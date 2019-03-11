#ifndef _OSCOPE2100_INTERFACE_H_
#define _OSCOPE2100_INTERFACE_H_

#include "oscope2100_usb.h"

enum tcapture_mode {CM_AUTO,CM_SINGLE,CM_STOPPED};
enum tmode {MODE_NORMAL=0,MODE_FZOOM=1,MODE_XY=2};
enum tchmode {CHMODE_DC=0,CHMODE_AC=1,CHMODE_GND=2};
enum tchmul {X1=0,X10=1,X100=2};
enum ttrigger {TR_OFF=0,TR_CH1=1,TR_CH2=2,TR_EXT=4};
enum tmath {M_OFF=0,M_CH1V_CH2I=1,M_CH1I_CH2V=2,M_CH1I=3,M_CH2I=4};

const int TCOUNT=14;
const guint16 SCOPE_TIME[]={1,2,5,10,20,50,100,200,500,1000,2000,5000,10000,20000}; // !!! -1 for scope

const int VCOUNT=7;
const double VOLTAGE_RESOLUTION[]={0.05,0.1,0.2,0.5,1,2,5};

extern tcapture_mode capture_mode;
extern gboolean restart_capture;

// UI state
extern tmode ui_mode;
extern int ui_time;

extern gboolean ui_ch1_on;
extern gboolean ui_ch1_flip;
extern int ui_ch1_voltage;
extern tchmode ui_ch1_mode;
extern tchmul ui_ch1_mul;

extern gboolean ui_ch2_on;
extern gboolean ui_ch2_flip;
extern int ui_ch2_voltage;
extern tchmode ui_ch2_mode;
extern tchmul ui_ch2_mul;

extern ttrigger ui_trigger;
extern int ui_edge;

extern gboolean ui_scientific_format;
extern uchar ui_separator;

extern int ui_math;
extern double ui_rshunt;
extern double ui_rload;


extern std::ostringstream expstr;
//

extern GtkWidget *window;

extern GtkWidget *ch1_label;
extern GtkWidget *ch2_label;
extern GtkWidget *time_label;
extern GtkWidget *demo_label;
extern GtkWidget *x_info_label;
extern GtkWidget *y_info_label;
extern GtkWidget *math_label;


extern int cursor_x0;
extern int cursor_y0;
extern int cursor_x1;
extern int cursor_y1;


extern uchar ch1buf[BUFSIZE];
extern uchar ch2buf[BUFSIZE];
extern int buf_time;
extern int buf_ch1;
extern int buf_ch2;

extern double ch1_k;
extern double ch2_k;

extern double ch1_offset;
extern double ch2_offset;


void create_interface();
void refresh_ch_k();
void show_offsets();
void show_math();

#endif

#ifndef _OSCOPE2100_COMMON_H_
#define _OSCOPE2100_COMMON_H_

#include <iostream>
#include <sstream>
#include <gtk/gtk.h>


typedef unsigned char uchar;

const std::string WINDOW_TITLE="Oscope 2100";


void load_settings();
void save_settings();

std::string fround4 (double x);
std::string val2str (double,std::string,bool=true);
std::string f2str(double,bool=false);
std::string pad(std::string s,unsigned int n);
bool prepare_expstr();

#endif

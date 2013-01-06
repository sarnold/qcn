/**
  @file    qcn_config_win.h
  @author  Carl Christensen, carlgt1@yahoo.com
  
this is checked in for windows builds, Mac & Linux will be autogenerated from config.h.in by autoconf

*/

// CMC -- 
/* double-inclusion protection for config.h */
#ifndef QCN_CONFIG_H
#define QCN_CONFIG_H

#include <windows.h>

/* Version defines are now in version.h */
#include "version.h"  
#include "str_util.h"


// all the usual autoconf/automake/configure style defines are in boinc/win_build/win-config.h
#include "../boinc/win_build/win-config.h"

/* end double-inclusion protection for config.h */
#endif /* #ifndef QCN_CONFIG_H */


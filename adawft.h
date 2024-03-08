// dawft.h


//----------------------------------------------------------------------------
//  PLATFORM SPECIFIC
//----------------------------------------------------------------------------

#ifndef WINDOWS
#define d_mkdir(s,u) mkdir(s,u)
#define DIR_SEPERATOR "/"
#endif

#ifdef WINDOWS
#define d_mkdir(s,u) mkdir(s)
#define DIR_SEPERATOR "\\"
#endif

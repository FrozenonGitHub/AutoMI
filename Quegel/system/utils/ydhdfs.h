#ifndef YDHDFS_H
#define YDHDFS_H

// If you use Hadoop 2.x, please edit "utils/ydhdfs.h" and uncomment the line "#define YARN"
//   to use "ydhdfs2.h" instead of "ydhdfs1.h".
// Also, please make sure that the hostname and port are correct
//   in the function "getHdfsFS()" of "ydhdfs1.h" or "ydhdfs2.h".

#define YARN


// #ifdef YARN
// #include "ydhdfs2.h"
// #else
// #include "ydhdfs1.h"
// #endif

// debug
#include "ydhdfs3.h"

#endif

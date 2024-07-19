#include "utils/ydhdfs.h"

int main(int argc, char** argv)
{
    // Call `put` function defined in ydhdfs.h,
    // upload from local path (argv[1]) to HDFS path (argv[2])
    put(argv[1], argv[2]);
    return 0;
}

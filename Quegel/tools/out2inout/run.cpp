#include "out2inout.h"

int main(int argc, char* argv[])
{
    init_workers();
    pregel_format("/toy_out", "/toy_inout");
    worker_finalize();
    return 0;
}

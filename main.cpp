#include <iostream>
#include <functional>//std::ref, std::bind, std::mem_fn
#include <utility>//std::move
#include <algorithm>//std::for_each
#include "cppconcurrent.h"

using namespace std;

int main(int argc, char *argv[])
{
    //ff();
    //test_Solution();
    //use_thrdsafe_stack();
    test_hierarchy_mutex();
    return 0;
}

#include <iostream>

using namespace std;

#include "ping.h"

int main(int argc, char** argv)
{
    if( argc < 3){
        std::cout <<  argv[0] << " dest_ip pkt_count" << std::endl;
        return 0;
    }

    Ping_Result result;

    Ping p(argv[1], 300);
    p.ping(atoi(argv[2]), result);

    std::cout << "result:" << std::endl;
    std::cout << "send: " << result.send_pkt_count << std::endl;
    std::cout << "recv: " << result.recv_pkt_count << std::endl;
    std::cout << "msec: " << result.total_msec << std::endl;

    return 0;
}

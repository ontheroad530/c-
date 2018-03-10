#ifndef PING_H
#define PING_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string>

#define PACKET_SIZE 1024*4
#define SEND_DATA_LEN 56

struct Ping_Result
{
    int32_t send_pkt_count;
    int32_t recv_pkt_count;
    float   total_msec;
};

class Ping
{
public:
    Ping(const std::string& dest_ip, int32_t time_msec);
    ~Ping();

    int32_t ping(int32_t pkt_count, Ping_Result& result);

private:
    int32_t create_socket();
    void close_socket();

    void send_packet();
    void recv_packet();

    int32_t pack();
    int32_t unpack(char* buf, int32_t len);

    uint16_t cal_cksum(uint16_t* picmp, int32_t len);

private:
    int32_t _socket_fd;
    int32_t _recv_pkt_count;
    int32_t _send_pkt_count;

    sockaddr_in _dest_addr;
    sockaddr_in _from_addr;
    std::string _dest_ip;
    timeval     _begin_tv;
    timeval     _end_tv;
    timeval     _time_out;
    char    _send_buf[PACKET_SIZE];
    char    _recv_buf[PACKET_SIZE];
};

#endif // PING_H

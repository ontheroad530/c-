#include "ping.h"

#include <netdb.h>
#include <unistd.h>
#include <strings.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <cstring>

#include <cstdio>
#include <iostream>
#include <cassert>

Ping::Ping(const std::string &dest_ip, int32_t time_msec)
    :_socket_fd(-1), _recv_pkt_count(0), _send_pkt_count(0), _dest_ip(dest_ip)
{
    _time_out.tv_sec = time_msec / 1000;
    _time_out.tv_usec = (time_msec % 1000) * 1000;
}

Ping::~Ping()
{
    close_socket();
}

int32_t Ping::ping(int32_t pkt_count, Ping_Result& result)
{
    int32_t ret = create_socket();
    if(-1 == ret) return -1;

    int32_t i = 0;
    while( i++ < pkt_count)
    {
        send_packet();
        recv_packet();
    }

    result.send_pkt_count = _send_pkt_count;
    result.recv_pkt_count = _recv_pkt_count;

    timeval time_result;
    timersub(&_end_tv, &_begin_tv, &time_result);
    result.total_msec = time_result.tv_sec * 1000 + (float)time_result.tv_usec / 1000;

    _send_pkt_count = _recv_pkt_count = 0;
    return 0;
}

int32_t Ping::create_socket()
{
    if(_socket_fd > 0 ) return 0;

    _socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if( -1 == _socket_fd){
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        return -1;
    }

    //绑定icmp id
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(getpid() );
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(_socket_fd, (sockaddr*)&addr, sizeof(addr));

    int32_t size = 50 * 1024;
    int32_t ret = setsockopt(_socket_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size));
    if(ret != 0)
    {
        std::cerr << "create_socket: set socket receive buf failed:" << errno << std::endl;
        return -1;
    }

    _dest_addr.sin_family = AF_INET;
    bzero( &(_dest_addr.sin_zero), 0);

    ulong   in_addr = inet_addr(_dest_ip.c_str());
    if( in_addr == INADDR_NONE){
        std::cerr << "error: " << _dest_ip << std::endl;
        return -1;
    }
    else
    {
        _dest_addr.sin_addr.s_addr = in_addr;
    }

    return 0;
}

void Ping::close_socket()
{
    if( _socket_fd != -1)
    {
        close(_socket_fd);
        _socket_fd = -1;
    }
}

void Ping::send_packet()
{
    int32_t packet_size = pack();

    if( sendto(_socket_fd, _send_buf, packet_size, 0, (sockaddr*)&_dest_addr, sizeof(_dest_addr)) < 0)
    {
        std::cerr << "sendto err" << std::endl;
        return;
    }

    _send_pkt_count++;
}

void Ping::recv_packet()
{
    int32_t n;
    int32_t from_len = sizeof(_from_addr);
     while(_recv_pkt_count < _send_pkt_count)
     {
         fd_set readfds;
         FD_ZERO(&readfds);
         FD_SET(_socket_fd,&readfds);

         int maxfds = _socket_fd +1;
         n = select(maxfds,&readfds,NULL,NULL,&_time_out);
         if( 0 == n)
         {
             std::cerr << "recv_packet: select time out: " << strerror(errno) << std::endl;
             return;
         }
         else if(n < 0)
         {
             std::cerr << "recv_packet: select error: " << strerror(errno) << std::endl;
             if(errno == EINTR)
                 continue;
             else
                 return;
         }
         if ((n = recvfrom(_socket_fd, _recv_buf, sizeof (_recv_buf), 0,
                     (struct sockaddr *) &_from_addr, (socklen_t *)&from_len)) <= 0)
         {
             std::cerr << "recv_packet: select error: " << strerror(errno) << std::endl;
             return;
         }

         gettimeofday(&_end_tv, NULL); /*记录接收时间*/
         if (unpack(_recv_buf, n) == -1)
         {
             continue;
         }
     }
}

int32_t Ping::pack()
{
    icmp* p_icmp = reinterpret_cast<icmp*>(_send_buf);
    assert(p_icmp);

    int32_t packet_size = 0;
    static int32_t packet_num = 0;

    p_icmp->icmp_type = ICMP_ECHO;
    p_icmp->icmp_code = 0;
    p_icmp->icmp_cksum = 0;
    p_icmp->icmp_seq = packet_num++;
//    p_icmp->icmp_id = getpid();

    packet_size = sizeof(icmphdr) + SEND_DATA_LEN;

    timeval* p_time = reinterpret_cast<timeval*>(p_icmp->icmp_data);
    gettimeofday(p_time, NULL);
    if(0 == _send_pkt_count)
    {
        _begin_tv = *p_time;
    }

    p_icmp->icmp_cksum = cal_cksum((uint16_t*)p_icmp, packet_size);

    return packet_size;
}

int32_t Ping::unpack(char *buf, int32_t len)
{
    struct ip* p_ip = reinterpret_cast<ip*>(buf);
    assert(p_ip);

    icmp* p_icmp = reinterpret_cast<icmp*>(buf);

    if( len < sizeof(icmphdr)){
        std::cerr << "ICMP packet\'length is less than 8" << std::endl;
        return -1;
    }

    if( p_icmp->icmp_type == ICMP_ECHOREPLY
            && _dest_ip == inet_ntoa(_from_addr.sin_addr)
            && p_icmp->icmp_id == ntohs(getpid()) )
    {
        _recv_pkt_count++;
//        std::cout << "check success, recv correct packet ..." << std::endl;
    }
    else
    {
        std::cerr << "check fail, incorrect packet..." << std::endl;
        return -1;
    }

    return 0;
}

uint16_t Ping::cal_cksum(uint16_t *picmp, int32_t len)
{
    int nleft = len;
    int sum = 0;
    uint16_t *w = picmp;
    uint16_t answer=0;

    //采用32bit加法
    while(nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if( nleft == 1)
    {
        *(uint16_t *)(&answer) = *(uint16_t *)w;
        sum += answer;
    }
    //把高16位加到低16位上去
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}


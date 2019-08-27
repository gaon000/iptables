#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h> /* for NF_ACCEPT */
#include <errno.h>
#include "protocol/all.h"
#include<string.h>

#include <libnetfilter_queue/libnetfilter_queue.h>

bool equalIPAddr(ip_addr x, ip_addr y){
	return memcmp(&x, &y, sizeof(ip_addr)) == 0;
}

void printPacket(const unsigned char *p, uint32_t size)
{
	int len = 0;
	while (len < size)
	{
		printf("%02X ", *(p++));
		if (!(++len % 16))
		{
			printf("\n");
		}
	}
	if (size % 16)
	{
		printf("\n");
	}
}

void printIPAddress(ip_addr ipAddr)
{
	printf("%d.%d.%d.%d", ipAddr.a, ipAddr.b, ipAddr.c, ipAddr.d);
}

/* returns packet id */
static u_int32_t print_pkt(struct nfq_data *tb, bool *isAccept)
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
	u_int32_t mark, ifi;
	int ret;
	unsigned char *data;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph)
	{
		id = ntohl(ph->packet_id);
		if (ntohs(ph->hw_protocol) == ETHERTYPE_ARP)
		{
			ret = nfq_get_payload(tb, &data);

			if (ret >= 0)
			{
				int packetIndex = 0;
				const ip_header *iph = (ip_header *)(ip_header *)data;
				packetIndex += sizeof(ip_header);
				printf("IPv4\n");
				printf("ip src:  ");
				printIPAddress(iph->ip_src);
				printf("\n");
				printf("ip dest:  ");
				printIPAddress(iph->ip_dst);
				printf("\n");
				ip_addr temp;
				temp.a = 8;
				temp.b = 8;
				temp.c = 8;
				temp.d = 8;
				if(equalIPAddr(temp, iph->ip_dst)){
					*isAccept = false;
				}
			}
		}
		fputc('\n', stdout);
	}
	return id;
}

static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
			  struct nfq_data *nfa, void *data)
{
	bool *isAccept = new bool(true);
	u_int32_t id = print_pkt(nfa, isAccept);
	printf("entering callback\n");
	return nfq_set_verdict(qh, id, *isAccept ? NF_ACCEPT : NF_DROP , 0, NULL);
}

int main(int argc, char **argv)
{
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	struct nfnl_handle *nh;
	int fd;
	int rv;
	char buf[4096] __attribute__((aligned));

	printf("opening library handle\n");
	h = nfq_open();
	if (!h)
	{
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET) < 0)
	{
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET) < 0)
	{
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	printf("binding this socket to queue '0'\n");
	qh = nfq_create_queue(h, 0, &cb, NULL);
	if (!qh)
	{
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

	printf("setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0)
	{
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	fd = nfq_fd(h);

	for (;;)
	{
		if ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0)
		{
			printf("pkt received\n");
			nfq_handle_packet(h, buf, rv);
			continue;
		}
		/* if your application is too slow to digest the packets that
		 * are sent from kernel-space, the socket buffer that we use
		 * to enqueue packets may fill up returning ENOBUFS. Depending
		 * on your application, this error may be ignored. nfq_nlmsg_verdict_putPlease, see
		 * the doxygen documentation of this library on how to improve
		 * this situation.
		 */
		if (rv < 0 && errno == ENOBUFS)
		{
			printf("losing packets!\n");
			continue;
		}
		perror("recv failed");
		break;
	}

	printf("unbinding from queue 0\n");
	nfq_destroy_queue(qh);

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	 * it detaches other programs/sockets from AF_INET, too ! */
	printf("unbinding from AF_INET\n");
	nfq_unbind_pf(h, AF_INET);
#endif

	printf("closing library handle\n");
	nfq_close(h);

	exit(0);
}
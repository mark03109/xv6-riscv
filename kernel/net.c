#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

#define MBUF_SIZE 2048
#define MAX_QUEUED 16
// a memory buffer holding one received UDP payload waiting to be
// delivered to a process via recv().
struct mbuf {
  struct mbuf *next;   // the next mbuf in the queue
  uint32 raddr;        // source IP address (host byte order)
  uint16 rport;        // source UDP port (host byte order)
  unsigned int len;    // number of payload bytes stored in buf
  char buf[MBUF_SIZE]; // the UDP payload
};

// a UDP socket. a process can bind() to a local port to
// receive UDP packets addressed to that port.
#define MAX_SOCKETS 16
struct sock {
  struct sock *next;  // the next socket in the list
  uint16 lport;       // the local UDP port number
  struct spinlock lock; // protects rxq
  struct mbuf *rxq;   // a queue of received packets
};

// sockets come from a fixed, statically-allocated pool so that
// bind()/unbind() don't leak pages. unused sockets live on
// freesocks; bound sockets are linked on sockets (protected by netlock).
static struct sock sockpool[MAX_SOCKETS];
static struct sock *freesocks = 0;
static struct sock *sockets = 0;

// allocate and zero an mbuf.
static struct mbuf *
mbufalloc(void)
{
  struct mbuf *m = (struct mbuf *) kalloc();
  if(m)
    memset(m, 0, sizeof(struct mbuf));
  return m;
}

// free an mbuf.
static void
mbuffree(struct mbuf *m)
{
  kfree((void *) m);
}

void
netinit(void)
{
  initlock(&netlock, "netlock");

  // put all the sockets on the free list.
  for(int i = 0; i < MAX_SOCKETS; i++){
    sockpool[i].next = freesocks;
    freesocks = &sockpool[i];
  }
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  argint(0, &port);

  struct sock *s;

  acquire(&netlock);
  for(s = sockets; s; s = s->next){
    if(s->lport == port){
      // port already in use.
      release(&netlock);
      return -1;
    }
  }

  // take an unused socket from the free list.
  s = freesocks;
  if(s == 0){
    release(&netlock);
    return -1;
  }
  freesocks = s->next;
  s->lport = port;
  initlock(&s->lock, "sock");
  s->rxq = 0;
  s->next = sockets;
  sockets = s;
  release(&netlock);

  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  int port;
  argint(0, &port);

  struct sock *s, *prev = 0;

  acquire(&netlock);
  for(s = sockets; s; s = s->next){
    if(s->lport == port)
      break;
    prev = s;
  }
  if(s == 0){
    release(&netlock);
    return -1;
  }

  // unlink from the list.
  if(prev)
    prev->next = s->next;
  else
    sockets = s->next;
  release(&netlock);

  // free any queued packets.
  acquire(&s->lock);
  struct mbuf *m = s->rxq;
  s->rxq = 0;
  release(&s->lock);
  while(m){
    struct mbuf *n = m->next;
    mbuffree(m);
    m = n;
  }

  // return the socket to the free list.
  acquire(&netlock);
  s->next = freesocks;
  freesocks = s;
  release(&netlock);

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int dport;
  uint64 srcaddr, sportaddr, bufaddr;
  int maxlen;

  argint(0, &dport);
  argaddr(1, &srcaddr);
  argaddr(2, &sportaddr);
  argaddr(3, &bufaddr);
  argint(4, &maxlen);

  struct proc *p = myproc();

  // find the socket bound to dport.
  struct sock *s;
  acquire(&netlock);
  for(s = sockets; s; s = s->next){
    if(s->lport == dport)
      break;
  }
  release(&netlock);

  if(s == 0)
    return -1;

  // wait for a packet, if necessary.
  acquire(&s->lock);
  while(s->rxq == 0){
    if(killed(p)){
      release(&s->lock);
      return -1;
    }
    sleep(s, &s->lock);
  }

  // pop the first packet from the queue.
  struct mbuf *m = s->rxq;
  s->rxq = m->next;
  release(&s->lock);

  // copy the source address, source port, and payload out to the user.
  uint32 src = m->raddr;
  uint16 sport = m->rport;
  int len = m->len;
  if(len > maxlen)
    len = maxlen;

  if(copyout(p->pagetable, srcaddr, (char *)&src, sizeof(src)) < 0 ||
     copyout(p->pagetable, sportaddr, (char *)&sport, sizeof(sport)) < 0 ||
     copyout(p->pagetable, bufaddr, m->buf, len) < 0){
    mbuffree(m);
    return -1;
  }

  mbuffree(m);
  return len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  if(e1000_transmit(buf, total) < 0){
    kfree(buf);
    return -1;
  }

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  struct ip *ip = (struct ip *)(buf + sizeof(struct eth));

  // only IPv4 UDP packets, with no options.
  if(ip->ip_vhl != 0x45){
    kfree(buf);
    return;
  }
  if(ip->ip_p != IPPROTO_UDP){
    kfree(buf);
    return;
  }

  int iplen = ntohs(ip->ip_len);
  if(iplen < sizeof(struct ip) + sizeof(struct udp) ||
     iplen > len - (int)sizeof(struct eth)){
    kfree(buf);
    return;
  }

  struct udp *udp = (struct udp *)((char *)ip + sizeof(struct ip));
  int ulen = ntohs(udp->ulen);
  if(ulen < (int)sizeof(struct udp) ||
     ulen > iplen - (int)sizeof(struct ip)){
    kfree(buf);
    return;
  }
  int plen = ulen - sizeof(struct udp);

  // find the socket bound to the destination port.
  struct sock *s;
  acquire(&netlock);
  for(s = sockets; s; s = s->next){
    if(s->lport == ntohs(udp->dport))
      break;
  }
  release(&netlock);

  if(s == 0){
    // nobody is listening on this port.
    kfree(buf);
    return;
  }

  // copy the payload into an mbuf and append it to the socket's queue.
  struct mbuf *m = mbufalloc();
  if(m == 0){
    kfree(buf);
    return;
  }
  m->raddr = ntohl(ip->ip_src);
  m->rport = ntohs(udp->sport);
  m->len = plen;
  if(m->len > MBUF_SIZE)
    m->len = MBUF_SIZE;
  memmove(m->buf, (char *)(udp + 1), m->len);

  acquire(&s->lock);
  if(s->rxq == 0){
    s->rxq = m;
  } else {
    // append at the tail, but keep the queue length bounded so that
    // a flood of packets for one port can't consume all of memory.
    struct mbuf *last = s->rxq;
    int n = 1;
    while(last->next){
      last = last->next;
      n++;
    }
    if(n >= MAX_QUEUED){
      mbuffree(m); // drop the newest packet.
    } else {
      last->next = m;
    }
  }
  wakeup(s);
  release(&s->lock);

  kfree(buf);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}

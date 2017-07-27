/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "peers.h"
#include "message.h"
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include "kernel_udp.h"


struct peer
{
	int id;
	struct sockaddr_in addr;
	struct peers* peers;
};

struct subscription
{
	paxos_message_type type;
	peer_cb callback;
	void* arg;
};

struct peers
{
	int peers_count, clients_count;
	struct peer** peers;   /* peers we connected to */
	struct peer** clients; /* peers we accepted connections from */
	struct socket * sock;
	struct peer * me;
	struct evpaxos_config* config;
	int subs_count;
	struct subscription subs[32];
};

// static struct timeval reconnect_timeout = {2,0};
static struct peer* make_peer(struct peers* p, int id, struct sockaddr_in* in);
static void free_peer(struct peer* p);
static void free_all_peers(struct peer** p, int count);
static int on_read(char * data,struct peer * arg, int size);
// static void on_client_event(struct bufferevent* bev, short events, void *arg);


struct peers*
peers_new(struct sockaddr_in * addr, struct evpaxos_config* config)
{
	struct peers* p = kmalloc(sizeof(struct peers), GFP_KERNEL);
	p->peers_count = 0;
	p->clients_count = 0;
	p->subs_count = 0;
	p->peers = NULL;
	p->clients = NULL;
	p->sock = NULL;
	p->me = make_peer(p, -1, addr);
	p->config = config;
	return p;
}

void
peers_free(struct peers* p)
{
	free_all_peers(p->peers, p->peers_count);
	free_all_peers(p->clients, p->clients_count);
	kfree(p->me);
	kfree(p);
}

int
peers_count(struct peers* p)
{
	return p->peers_count;
}

void
peers_foreach_acceptor(struct peers* p, peer_iter_cb cb, void* arg)
{
	int i;
	for (i = 0; i < p->peers_count; ++i)
		cb(p->peers[i], arg);
}

void
peers_foreach_client(struct peers* p, peer_iter_cb cb, void* arg)
{
	int i;
	paxos_log_debug("Sent this message to %d peers", p->clients_count);
	for (i = 0; i < p->clients_count; ++i)
		cb(p->clients[i], arg);
}

struct peer*
peers_get_acceptor(struct peers* p, int id)
{
	int i;
	for (i = 0; p->peers_count; ++i)
		if (p->peers[i]->id == id)
			return p->peers[i];
	return NULL;
}

int
peer_get_id(struct peer* p)
{
	return p->id;
}

int
peers_listen(struct peers* p, udp_service * k, char * ip, int * port)
{
	int ret;
	struct sockaddr_in address;
	int size_buf = 100;
	unsigned char * bigger_buff = NULL;
	unsigned char * in_buf = kmalloc(size_buf, GFP_KERNEL);

	int n_packet_toget =0, first_time = 0;

	udp_server_init(k, &p->sock, ip, port);

	while(1){
		if(kthread_should_stop() || signal_pending(current)){
      check_sock_allocation(k, p->sock);
      kfree(in_buf);
			if(bigger_buff != NULL){
				kfree(bigger_buff);
			}
      return 0;
    }

		memset(in_buf, '\0', size_buf);
    memset(&address, 0, sizeof(struct sockaddr_in));
		ret = udp_server_receive(p->sock, &address, in_buf, size_buf, MSG_WAITALL, k);
		if(ret > 0){
			if(first_time == 0){
				ret = on_read(in_buf, p->me, size_buf);
				if(ret != 0){
					while(ret > 0){
						ret -= size_buf;
						n_packet_toget++;
					}
					bigger_buff = krealloc(in_buf, size_buf * (n_packet_toget +1), GFP_KERNEL);
					in_buf+=size_buf;
					memset(in_buf, '\0', size_buf * n_packet_toget);
					first_time = 1;
				}
			}else{
				strncat(bigger_buff, in_buf, size_buf);
				n_packet_toget--;
				if(n_packet_toget == 0){
					first_time = 0;
				}
			}
		}
	}
	// paxos_log_info("Listening on port %d", port);
	return 1;
}

void
peers_subscribe(struct peers* p, paxos_message_type type, peer_cb cb, void* arg)
{
	struct subscription* sub = &p->subs[p->subs_count];
	sub->type = type;
	sub->callback = cb;
	sub->arg = arg;
	p->subs_count++;
}

static void
dispatch_message(struct peer* p, paxos_message* msg)
{
	int i;
	for (i = 0; i < p->peers->subs_count; ++i) {
		struct subscription* sub = &p->peers->subs[i];
		if (sub->type == msg->type){
			sub->callback(p, msg, sub->arg);
			break;
		}
	}
}

static int
on_read(char * data,struct peer * arg, int size)
{
	paxos_message msg;
	if(recv_paxos_message(data, &msg, size) == 1){// returns if the packet is partial
		return 1;
	}
	dispatch_message(arg, &msg);
	paxos_message_destroy(&msg);
	return 0;

}

// static void
// on_client_event(struct bufferevent* bev, short ev, void *arg)
// {
// 	struct peer* p = (struct peer*)arg;
// 	if (ev & BEV_EVENT_EOF || ev & BEV_EVENT_ERROR) {
// 		int i;
// 		struct peer** clients = p->peers->clients;
// 		for (i = p->id; i < p->peers->clients_count-1; ++i) {
// 			clients[i] = clients[i+1];
// 			clients[i]->id = i;
// 		}
// 		p->peers->clients_count--;
// 		p->peers->clients = realloc(p->peers->clients,
// 			sizeof(struct peer*) * (p->peers->clients_count));
// 		free_peer(p);
// 	} else {
// 		paxos_log_error("Event %d not handled", ev);
// 	}
// }

static struct peer*
make_peer(struct peers* peers, int id, struct sockaddr_in* addr)
{
	struct peer* p = kmalloc(sizeof(struct peer), GFP_KERNEL);
	p->id = id;
	p->addr = *addr;
	p->peers = peers;
	return p;
}

static void
free_all_peers(struct peer** p, int count)
{
	int i;
	for (i = 0; i < count; i++)
		free_peer(p[i]);
	if (count > 0)
		kfree(p);
}

static void
free_peer(struct peer* p)
{
	kfree(p);
}
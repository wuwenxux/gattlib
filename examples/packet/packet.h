/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;

//---------------------------------------------------------------------
// queue init                                                         
//---------------------------------------------------------------------
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )
#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)

#define IQUEUE_ADD(node, head) (\
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) (\
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )
	

#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)

#endif

#ifndef FSM_H
#define FSM_H
#include <readline/readline.h>
#include <gattlib.h>
#define HEADERLEN	4

#define BL_SEND_CTR 1
#define BL_SEND_MNG 2
#define BL_SEND_SINGLE_CTR 3
#define BL_RECV_ACK 4 
#define BL_RECV_SINGLE_ACK 5
#define BL_RECV_ACK 6
#define BL_RECV_CTR 7
#define BL_RECV_SINGLE_CTR 8
#define BL_RECV_DATA 9

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int i32;
typedef short i16;
typedef char i8;

/**
 * 
 * 
 * |<--------- 4 bytes Header--------->|
 * +--------+--------+--------+--------+ SN：Sequence Number, 0: Flow 非0: Data 
 * |       SN        | type   |   cmd  | type: CMD, ACK, Single CMD, Single ACK
 * +--------+--------+--------+--------+ cmd: Command, 指令类型，代表这个Packet的类型
 * |		len                        | len: Length, 后续数据的长度
 * +--------+--------+--------+--------+
**/

enum type {
    CTR,
    ACK,
    DATA,
    SINGLE_ACK,
    SINGLE,
    INVALD
} packet_type;

struct pkt_seg{
    struct IQUEUEHEAD node;
    u16 sn;
    u8 type;
    u8 value;
    u8 data[1];
}
struct pkt_pcb
{
    gatt_connection_t* connection;
    struct IQUEUEHEAD snd_queue;
    struct IQUEUEHEAD rcv_queue;
    u32 send_timstamp;
    char *buffer;
#define MAX_TIMEOUT 6
    u32 mtu;
    u8 seg_size;
}

/**
 * 
 * Finite state machine
 *
 * */
typedef struct fsm{
    struct pkt_pcb *pcb;
    const struct fsm_callbacks *callbacks;
    const char *term_reason;
    u8 term_reason_len;
    u8 seen_ack;
    u8 state;
};

typedef struct fsm_callbacks{
    void (*reset_state)(struct fsm*);
    int (*send_ctr)(struct fsm*);
    int (*send_mng)(struct fsm*);
    int (*recv_ack)(struct fsm*);   
    int (*up)(struct fsm *);
    int (*down)(struct fsm *);
    int (*exit)(struct fsm*, int, int, u8 *, int);
}
enum bl_event
{
    SEND_CTR,
    SEND_MNG,
    SEND_SINGLE_CTR,
    RECV_ACK,
    RECV_SINGLE_ACK,
    RECV_CTR,
    RECV_SINGLE_CTR,
    RECV_DATA,
}

enum status{
    READY,
    WAIT_START_ACK,
    WAIT_SINGLE_ACK,
    SYNC,
    WRITING,
    IDLE,
    READING,
    SYNC_ACK
} conn_status;

/**
 *  Prototypes 
 **/
void fsm_init(struct fsm *f);
void fsm_open(struct fsm *f);
void fsm_close(struct fsm *f);
void fsm_input(struct fsm *f, u8 *input_packet, int l);
void fsm_protoreject(struct fsm *);
void fsm_sdata(fsm *f, u8 code, u8 id, const u8 *data, int len);
#endif
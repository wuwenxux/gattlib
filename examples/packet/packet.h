/*
 *
 *  Packet - Flow control protocl for bluetooth
 * Copyright (c) 2022, Wenxuan Wu <wenxuan.wu@xjsd.com>
 *
 *
 */

#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;

/**
 *
 * @name Queue init                                                         
 *
 */
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


/**
 * @name queue operations 
 *
 *
 */

//@{
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
//@}

#endif

#ifndef FSM_H
#define FSM_H
#include <readline/readline.h>
#include <gattlib.h>
#include <stdbool.h>

/**
 * @brief bluetooth flow control protocl event filter
 *  @note events filter.
 */
//@{
#define PKT_HEADERLEN	4
#define BL_SEND_CTR 1
#define BL_SEND_MNG 2
#define BL_SEND_SINGLE_CTR 3
#define BL_RECV_ACK 4 
#define BL_RECV_SINGLE_ACK 5
#define BL_RECV_ACK 6
#define BL_RECV_CTR 7
#define BL_RECV_SINGLE_CTR 8
#define BL_RECV_DATA 9
//@}

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
 * |		MSS                        | MSS: Length, 数据(without pkt_header)长度
 * +--------+--------+--------+--------+
 * @brief protocol types
**/
//@{
#define BL_PACKET_CTRL_FLAG 0
#define BL_PCCKET_DATA_FLAG 1


#define BL_PACKET_OPTCODE_ACK 1
#define BL_PACKET_OPTCODE_DATA 2
#define BL_PACKET_TYPE_SINGLE_ACK 3
#define BL_PACKET_TYPE_SINGLE 4
#define BL_PACKET_TYPE_INVALID 5

//@{
#define PKT_CMD_DATA 0 
#define PKT_CMD_UNBOND 1
//@}
#ifdef __cplusplus
extern "C" {
#endif
struct pkt_seg {
    struct IQUEUEHEAD node;
    u16 sn;
    union frm {
        struct ctrl_frm {
            u8 optcode;
            u8 cmd;
        } ctrl;
        struct data_frm {
            u8 data[];
        } data;
    };
};


/**
 * @name blue tooth connection control block.
 * 
 */
struct pkt_cb
{
    struct IQUEUEHEAD snd_queue;    /**< Writer's packet queue to send */
    struct IQUEUEHEAD rcv_queue;    /**< Receiver's packet queue to receive */
    struct IQUEUEHEAD sync_queue;   /**< Sync queue to sync lost packets */
    u32 max_frame_count;                /**< Number of frames based on mtu */
    char *buffer;
    u32 dmtu;
    u32 timeout;
    int logmask;
    int max_nak_loops;
    int nsnd_queue;
    int nrcv_queue;
    int nsyn_queue;
    void (*output)(const char *buf, int len, struct pkt_seg *seg);
    void (*log)(const char *log, struct pkt_cb* pcb);
};

#define PKT_LOG_OUTPUT 1
#define PKT_LOG_INPUT  2
#define PKT_LOG_SEND   4
#define PKT_LOG_RECV   8
#define PKT_LOG_IN_SYNC 16
#define PKT_LOG_IN_DATA 32
#define PKT_LOG_IN_ACK  64
#define PKT_LOG_IN_PROBE 128
#define PKT_LOG_OUT_DATA 256
#define PKT_LOG_OUT_ACK   512
#define PKT_LOG_OUT_PROBE 1024
#define PKT_LOG_OUT_SYNC  2048


#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htobs(d)  (d)
#define htobl(d)  (d)
#define btohs(d)  (d)
#define btohl(d)  (d)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define htobs(d)  bswap_16(d)
#define htobl(d)  bswap_32(d)
#define btohs(d)  bswap_16(d)
#define btohl(d)  bswap_32(d)
#else
#error "Unknown byte order"
#endif



/**
 * @brief maxium params related
 */
//@{
#define MAX_TIMEOUT 6000
#define MAX_SEG_MTU 20
#define MAX_NAKLOOPS 6
#define MAX_ATT_PAYLOAD 244
//@}
/**
 * @brief flow control events.
 * 
 */
enum bl_fsm_event
{
    SEND_CTR,
    SEND_MNG,
    SEND_SINGLE_CTR,
    RECV_ACK,
    RECV_SINGLE_ACK,
    RECV_CTR,
    RECV_SINGLE_CTR,
    RECV_DATA,
};

#define BL_PACKET_FSM_READY   0           /* Ready to send */
#define BL_PACKET_FSM_WAIT_START_ACK    1
#define BL_PACKET_FSM_WAIT_SINGLE_ACK   2
#define BL_PACKET_FSM_SYNC              3
#define BL_PACKET_FSM_WRITING           4
#define BL_PACKET_FSM_IDLE              5 /* Ready to receive */
#define BL_PACKET_FSM_READING   6
#define BL_PACKET_FSM_SYNC_ACK  7

/**
 *  Prototypes 
 **/
struct pkt_cb* pcb_create(int conv);
void pcb_release(struct pkt_cb *pcb);
int pcb_recv(struct pkt_cb *pcb, char *buffer, int len);
void pcb_send(struct pkt_cb *pcb);
void pcb_update(struct pkt_cb *pcb);
void pcb_input(struct pkt_cb *pcb, u8 *input_packet, int l);
void pcb_log(struct pkt_cb *pcb, int mask, const char *fmt, ...);
void set_mtu(struct pkt_cb *pcb, int mtu);
void read_charac(uuid_t read_uuid,on_read);

#ifdef __cplusplus
}
#endif
#endif
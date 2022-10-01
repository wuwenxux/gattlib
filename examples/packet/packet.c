#include <assert.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GATTLIB_LOG_BACKEND_SYSLOG
#include <syslog.h>
#endif

#include "gattlib.h"
#include "packet.h"
static gatt_connection_t *g_connection;
static uuid_t starry_read_uuid = uuid;
static uuid_t g_write_uuid;

static GMainLoop *m_main_loop;

int max_framecnt;
static void send_single_ack(struct pkt_cb *pcb, u8 frame_count, u8 dmtu);
static void wait_start_ack(struct pkt_cb *pcb);
static void wait_start_mng_ack(struct pkt_cb *pcb);
static void wait_start_single_ack(struct pkt_cb *pcb);
static void recv_singleack(struct pkt_cb *pcb);
static void recv_ack(struct pkt_cb *pcb);
static void recv_ctr(struct pkt_cb *pcb);
static void recv_single_ctr(struct pkt_cb *pcb);
static void recv_data(struct pkt_cb *pcb);
static void sync_packet(struct pkt_cb *pcb);
static struct pkt_seg * parse_header(struct pkt_cb *pcb, void *data, u32 len);
static void timeout (void *);
inline int get_frame_count(u32 totalbytes)
{

	if (totalbytes > MAX_SEG_MTU)
		max_framecnt = totalbytes/MAX_SEG_MTU + 1;
	else 
		max_framecnt = 1;
	return max_framecnt;
}
/**
 * pkt_seg - Data segment management
 * 
 * 
 */
struct pkt_seg;

static struct pkt_seg* pkt_seg_malloc(size_t data)
{
	return (struct pkt_seg *) malloc(sizeof(struct pkt_seg) + size);
}

static void pkt_seg_free(struct pkt_cb *pcb, struct pkt_seg *seg)
{
	free(seg);
}

void pcb_log(struct pkt_cb *pcb, int mask, const char *fmt, ...)
{
	char buffer[1024];
	va_list argptr;
	if ((mask & pcb->logmask) == 0 || pcb->log == NULL) return;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	pcb->log(buffer,pcb);
}
//check log mask
static int pcb_canlog(const struct pkt_cb *pcb, int mask)
{
	if((mask & pcb->logmask) == 0) || (pcb->log == NULL)) return 0;
	return 1;
}
// output segment
static int pcb_output(struct pkt_cb *pcb, const void *data, len)
{
	assert(pcb);
	assert(pcb->output);
	if( pcb_canlog(pcb, PKT_LOG_OUTPUT)){
		pcb_log(pcb, PKT_LOG_OUTPUT, "[RO] %ld bytes", (long)size);
	}
}

void pcb_qprint(const char *name, struct IQUEUEHEAD *head)
{
	const struct IQUEUEHEAD *p;
	printf("<%s>: [", name);
	
	for (p = head->next; p != head; p = p->next) {
			const  struct pkt_seg *seg = iqueue_entry(p, const struct pkt_seg*, node);
			printf("(%u %u %u)", (unsigned short)seg->sn, (u8)(seg->type), (u8)(seg->value));
			if (p->next != head) printf(",");
	}
	printf("]\n");
}

/**
 * pkt_cb - Packet control block.
 * 
 * Initialize pkt control block.
 * 
 */
struct pkt_cb * pcb_create(int conv)
{
	struct pkt_cb *pcb = malloc(sizeof(struct pkt_cb));
	
	pcb->dmtu = MAX_SEG_MTU;
	pcb->buffer = (char*)malloc( (pcb->dmtu + PKT_HEADERLEN) * 3);
	
	if (pcb->buffer == NULL) {
		free(pcb->buffer);
		return NULL;
	}

	iqueue_init(&pcb->rcv_queue);
	iqueue_init(&pcb->snd_queue);
	iqueue_init(&pcb->sync_queue);
	pcb->max_frame_count = get_frame_count(u32 datasize);
	pcb->max_nak_loops = MAX_NAKLOOPS;
	pcb->output = NULL;
	pcb->log = NULL;
	pcb->nrcv_queue = 0;
	pcb->nsnd_queue = 0;
	pcb->nsyn_queue = 0;
	pcb->timeout = MAX_TIMEOUT;

	return pcb;
}

void pcb_release(struct pkt_cb *pcb)
{
	assert(pcb);
	if(pcb){
		struct pkt_seg *seg;
		while(!iqueue_is_empty(&pcb->snd_queue)){
			seg = iqueue_entry(pcb->snd_queue.next, struct pkt_seg, node);
			iqueue_del(&seg->node);
			pkt_seg_free(pcb, seg);
		}
		while(!iqueue_is_empty(&pcb->rcv_queue)){
			seg = iqueue_entry(pcb->rcv_queue.next, struct pkt_seg, node);
			iqueue_del(&seg->node);
			pkt_seg_free(pcb, seg);
		}
		while(!iqueue_is_empty(&pcb->sync_queue)){
			seg = iqueue_entry(pcb->sync_queue.next, struct pkt_seg, node);
			iqueue_del(&seg->node);
			pkt_seg_free(pcb, seg);
		}
		if(pcb->buffer)
			free(pcb->buffer);

		pcb->max_frame_count = 0;
		pcb->max_nak_loops = 0;
		pcb->dmtu = 0;
		pcb->log = 	NULL;
		pcb->output = NULL;
		pcb->nrcv_queue = 0;
		pcb->nsnd_queue = 0;
		pcb->nsyn_queue = 0;
		pcb->timeout = 0;
	}
}

void seg_set_mtu(struct pkt_seg *seg, int mtu)
{

	if(mtu > MAX_ATT_PAYLOAD || mtu < PKT_HEADERLEN)
		return;
	seg->mtu = mtu;
}
/**
 * 
 * set output callback, which will be invoked by pcb.
 * 
 **/
void pcb_set_output(struct pkt_cb * pcb, int (*output)(const u8 *buf, int len, struct pkt_cb *pcb))
{
	pcb->output = output;
}
/**
 * 
 * Calculate the current data size we received.
 * 
 **/
static pcb_peeksize(const struct pkt_cb *pcb)
{
	struct IQUEUEHEAD *p;
	struct pkt_seg *seg;
	int length = 0;

	assert(pcb);

	if(iqueue_is_empty(pcb->rcv_queue)) return -1;

	seg = iqueue_entry(pcb->rcv_queue.next, struct pkt_seg, node);
	if (seg.sn > max_framecnt || seg.sn == PKT_CMD_DATA)
		return -1;

	if(seg->sn ==  BL_PACKET_CTRL_FLAG)
		return -2;

	for( p = seg->node.next; p != &pcb->rcv_queue; p = p->next){
		seg = iqueue_entry(p, struct ptk_seg, node);

		if (seg->sn == BL_PACKET_CTRL_FLAG)
			continue;
		length += seg->len;
	}
	return length;
}

int pcb_recv(struct pkt_cb *pcb, u8 *buffer, int len)
{
	//not supported
	return -1;
}

int pcb_read(struct pkt_cb *pcb, u8 *buffer, int len)
{
	return -1;
}

void read_handler(const uuid_t* uuid, const u8* data, u8 data_length, void* user_data) {
	int i;

	printf("Notification Handler: ");

	for (i = 0; i < data_length; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}

static void on_user_abort(int arg) {
	g_main_loop_quit(m_main_loop);
}

static void usage(char *argv[]) {
	printf("%s <device_address> <notification_characteristic_uuid> [<write_characteristic_uuid> <write_characteristic_hex_data> ...]\n", argv[0]);
}


int main(int argc, char *argv[]) {
	int ret;
	int argid;
	gatt_connection_t* connection;

	if (argc < 3) {
		usage(argv);
		return 1;
	}

	if (gattlib_string_to_uuid(argv[2], strlen(argv[2]) + 1, &g_notify_uuid) < 0) {
		usage(argv);
		return 1;
	}

	if (argc > 3) {
		if (gattlib_string_to_uuid(argv[3], strlen(argv[3]) + 1, &g_write_uuid) < 0) {
			usage(argv);
			return 1;
		}
	}

#ifdef GATTLIB_LOG_BACKEND_SYSLOG
	openlog("gattlib_notification", LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));
#endif

	connection = gattlib_connect(NULL, argv[1], GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT);
	if (connection == NULL) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to connect to the bluetooth device.");
		return 1;
	}

	gattlib_register_notification(connection, notification_handler, NULL);

#ifdef GATTLIB_LOG_BACKEND_SSLOGY
	openlog("gattlib_notification", LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));
#endif

	ret = gattlib_notification_start(connection, &g_notify_uuid);
	if (ret) {
		GATTLIB_LOG(GATTLIB_ERROR, "Fail to start notification.");
		goto DISCONNECT;
	}

	// Optional byte writes to make to trigger notifications
	for (argid = 4; argid < argc; argid ++) {
		unsigned char data[256];
		char * charp;
		unsigned char * datap;
		for (charp = argv[4], datap = data; charp[0] && charp[1]; charp += 2, datap ++) {
			sscanf(charp, "%02hhx", datap);
		}
		ret = gattlib_write_char_by_uuid(connection, &g_write_uuid, data, datap - data);
//#ifdef 
		ret = gattlib_write_without_response_char_by_uuid(connection, &g_write_uuid, data, datap - data);

		if (ret != GATTLIB_SUCCESS) {
			if (ret == GATTLIB_NOT_FOUND) {
				GATTLIB_LOG(GATTLIB_ERROR, "Could not find GATT Characteristic with UUID %s.", argv[3]);
			} else {
				GATTLIB_LOG(GATTLIB_ERROR, "Error while writing GATT Characteristic with UUID %s (ret:%d)",
					argv[3], ret);
			}
			goto DISCONNECT;
		}
	}

	// Catch CTRL-C
	signal(SIGINT, on_user_abort);

	m_main_loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(m_main_loop);

	// In case we quiet the main loop, clean the connection
	gattlib_notification_stop(connection, &g_notify_uuid);
	g_main_loop_unref(m_main_loop);

DISCONNECT:
	gattlib_disconnect(connection);
	puts("Done");
	return ret;
}
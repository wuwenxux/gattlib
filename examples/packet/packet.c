#include "packet.h"
static void single_ack_handler(struct fsm *fsm);
static void wait_start_ack_handler();
static void wait_start_mng_ack_handler();
static void wait_start_single_ack_handler();
static void single_handler();
static void recv_ack_handler();
static void recv_ctr_handler();
static void recv_single_ctr_handler();
static void recv_data_handler();
static void sync_packet_handler();

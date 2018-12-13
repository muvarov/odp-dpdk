/* Copyright (c) 2014-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP packet descriptor - implementation internal
 */

#ifndef ODP_PACKET_INTERNAL_H_
#define ODP_PACKET_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/align.h>
#include <odp_debug_internal.h>
#include <odp/api/debug.h>
#include <odp_buffer_internal.h>
#include <odp_pool_internal.h>
#include <odp/api/packet.h>
#include <odp/api/plat/packet_inline_types.h>
#include <odp/api/packet_io.h>
#include <odp/api/crypto.h>
#include <odp_ipsec_internal.h>
#include <odp/api/abi/packet.h>
#include <protocols/eth.h>
#include <odp_queue_if.h>

#include <rte_config.h>
#if defined(__clang__)
#undef RTE_TOOLCHAIN_GCC
#endif
#include <rte_acl_osdep.h>

/** Minimum segment length expected by packet_parse_common() */
#define PACKET_PARSE_SEG_LEN 96

ODP_STATIC_ASSERT(sizeof(_odp_packet_input_flags_t) == sizeof(uint64_t),
		  "INPUT_FLAGS_SIZE_ERROR");

ODP_STATIC_ASSERT(sizeof(_odp_packet_flags_t) == sizeof(uint32_t),
		  "PACKET_FLAGS_SIZE_ERROR");

/**
 * Packet output flags
 */
typedef union {
	/* All output flags */
	uint32_t all;

	struct {
		/** adjustment for traffic mgr */
		uint32_t shaper_len_adj:8;

		/* Bitfield flags for each output option */
		uint32_t l3_chksum_set:1; /**< L3 chksum bit is valid */
		uint32_t l3_chksum:1;     /**< L3 chksum override */
		uint32_t l4_chksum_set:1; /**< L3 chksum bit is valid */
		uint32_t l4_chksum:1;     /**< L4 chksum override  */
	};
} output_flags_t;

ODP_STATIC_ASSERT(sizeof(output_flags_t) == sizeof(uint32_t),
		  "OUTPUT_FLAGS_SIZE_ERROR");

/**
 * Packet parser metadata
 */
typedef struct {
	/* Packet input flags */
	_odp_packet_input_flags_t  input_flags;

	/* Other flags */
	_odp_packet_flags_t        flags;

	 /* offset to L2 hdr, e.g. Eth */
	uint16_t l2_offset;

	/* offset to L3 hdr, e.g. IPv4, IPv6 */
	uint16_t l3_offset;

	/* offset to L4 hdr (TCP, UDP, SCTP, also ICMP) */
	uint16_t l4_offset;
} packet_parser_t;

/**
 * Internal Packet header
 *
 * To optimize fast path performance this struct is not initialized to zero in
 * packet_init(). Because of this any new fields added must be reviewed for
 * initialization requirements.
 */
typedef struct {
	/* common buffer header */
	odp_buffer_hdr_t buf_hdr;

	/*
	 * Following members are initialized by packet_init()
	 */

	packet_parser_t p;

	odp_pktio_t input;

	/* Event subtype */
	int8_t subtype;

	/*
	 * Members below are not initialized by packet_init()
	 */

	/* Timestamp value */
	odp_time_t timestamp;

	/* Classifier destination queue */
	odp_queue_t dst_queue;

	/* Result for crypto packet op */
	odp_crypto_packet_result_t crypto_op_result;

	/* Temp storage for digest */
#define PACKET_DIGEST_MAX 64
	uint8_t crypto_digest_buf[PACKET_DIGEST_MAX];

	/* Temp storage for AAD */
#define PACKET_AAD_MAX 32
	uint8_t crypto_aad_buf[PACKET_AAD_MAX];

	/* Context for IPsec */
	odp_ipsec_packet_result_t ipsec_ctx;

} odp_packet_hdr_t __rte_cache_aligned;

/**
 * Return the packet header
 */
static inline odp_packet_hdr_t *packet_hdr(odp_packet_t pkt)
{
	return (odp_packet_hdr_t *)(uintptr_t)pkt;
}

static inline odp_packet_t packet_handle(odp_packet_hdr_t *pkt_hdr)
{
	return (odp_packet_t)pkt_hdr;
}

static inline struct rte_mbuf *pkt_to_mbuf(odp_packet_t  pkt)
{
	return (struct rte_mbuf *)(uintptr_t)pkt;
}

static inline odp_buffer_hdr_t *packet_to_buf_hdr(odp_packet_t pkt)
{
	return &packet_hdr(pkt)->buf_hdr;
}

static inline odp_packet_t packet_from_buf_hdr(odp_buffer_hdr_t *buf_hdr)
{
	return (odp_packet_t)(odp_packet_hdr_t *)buf_hdr;
}

static inline void packet_subtype_set(odp_packet_t pkt, int ev)
{
	packet_hdr(pkt)->subtype = ev;
}

/**
 * Initialize ODP headers
 */
static inline void packet_init(odp_packet_hdr_t *pkt_hdr)
{
	pkt_hdr->p.input_flags.all  = 0;
	pkt_hdr->p.flags.all_flags  = 0;

	pkt_hdr->p.l2_offset        = 0;
	pkt_hdr->p.l3_offset        = ODP_PACKET_OFFSET_INVALID;
	pkt_hdr->p.l4_offset        = ODP_PACKET_OFFSET_INVALID;

	if (odp_unlikely(pkt_hdr->subtype != ODP_EVENT_PACKET_BASIC))
		pkt_hdr->subtype = ODP_EVENT_PACKET_BASIC;

	pkt_hdr->input = ODP_PKTIO_INVALID;
}

static inline void copy_packet_parser_metadata(odp_packet_hdr_t *src_hdr,
					       odp_packet_hdr_t *dst_hdr)
{
	dst_hdr->p = src_hdr->p;
}

static inline void copy_packet_cls_metadata(odp_packet_hdr_t *src_hdr,
					    odp_packet_hdr_t *dst_hdr)
{
	dst_hdr->p = src_hdr->p;
	dst_hdr->dst_queue = src_hdr->dst_queue;
	dst_hdr->timestamp = src_hdr->timestamp;
}

static inline uint32_t packet_len(odp_packet_hdr_t *pkt_hdr)
{
	return rte_pktmbuf_pkt_len(&pkt_hdr->buf_hdr.mb);
}

static inline void packet_set_len(odp_packet_hdr_t *pkt_hdr, uint32_t len)
{
	rte_pktmbuf_pkt_len(&pkt_hdr->buf_hdr.mb) = len;
}

/* Forward declarations */
int _odp_packet_copy_md_to_packet(odp_packet_t srcpkt, odp_packet_t dstpkt);

/* Perform packet parse up to a given protocol layer */
int packet_parse_layer(odp_packet_hdr_t *pkt_hdr,
		       odp_proto_layer_t layer,
		       odp_proto_chksums_t chksums);

/* Reset parser metadata for a new parse */
void packet_parse_reset(odp_packet_hdr_t *pkt_hdr);

static inline int packet_hdr_has_l2(odp_packet_hdr_t *pkt_hdr)
{
	return pkt_hdr->p.input_flags.l2;
}

static inline void packet_hdr_has_l2_set(odp_packet_hdr_t *pkt_hdr, int val)
{
	pkt_hdr->p.input_flags.l2 = val;
}

static inline int packet_hdr_has_eth(odp_packet_hdr_t *pkt_hdr)
{
	return pkt_hdr->p.input_flags.eth;
}

static inline int packet_hdr_has_ipv6(odp_packet_hdr_t *pkt_hdr)
{
	return pkt_hdr->p.input_flags.ipv6;
}

static inline void packet_set_ts(odp_packet_hdr_t *pkt_hdr, odp_time_t *ts)
{
	if (ts != NULL) {
		pkt_hdr->timestamp = *ts;
		pkt_hdr->p.input_flags.timestamp = 1;
	}
}

int packet_parse_common(packet_parser_t *pkt_hdr, const uint8_t *ptr,
			uint32_t pkt_len, uint32_t seg_len, int layer,
			odp_proto_chksums_t chksums);

int _odp_packet_set_data(odp_packet_t pkt, uint32_t offset,
			 uint8_t c, uint32_t len);

int _odp_packet_cmp_data(odp_packet_t pkt, uint32_t offset,
			 const void *s, uint32_t len);

int _odp_packet_ipv4_chksum_insert(odp_packet_t pkt);
int _odp_packet_tcp_chksum_insert(odp_packet_t pkt);
int _odp_packet_udp_chksum_insert(odp_packet_t pkt);
int _odp_packet_sctp_chksum_insert(odp_packet_t pkt);

/* We can't enforce tailroom reservation for received packets */
ODP_STATIC_ASSERT(CONFIG_PACKET_TAILROOM == 0,
		  "ERROR: Tailroom has to be 0, DPDK doesn't support this");

#ifdef __cplusplus
}
#endif

#endif

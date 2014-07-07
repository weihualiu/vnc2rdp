/**
 * vnc2rdp: proxy for RDP client connect to VNC server
 *
 * Copyright 2014 Yiwei Li <leeyiw@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "sec.h"

static int
v2r_sec_build_conn(int client_fd, v2r_sec_t *s)
{
	return 0;
}

v2r_sec_t *
v2r_sec_init(int client_fd, v2r_session_t *session)
{
	v2r_sec_t *s = NULL;

	s = (v2r_sec_t *)malloc(sizeof(v2r_sec_t));
	if (s == NULL) {
		goto fail;
	}
	memset(s, 0, sizeof(v2r_sec_t));

	s->session = session;

	s->mcs = v2r_mcs_init(client_fd, session);
	if (s->mcs == NULL) {
		goto fail;
	}

	if (v2r_sec_build_conn(client_fd, s) == -1) {
		goto fail;
	}

	return s;

fail:
	v2r_sec_destory(s);
	return NULL;
}

void
v2r_sec_destory(v2r_sec_t *s)
{
	if (s == NULL) {
		return;
	}
	if (s->mcs != NULL) {
		v2r_mcs_destory(s->mcs);
	}
	free(s);
}

int
v2r_sec_recv(v2r_sec_t *s, v2r_packet_t *p, uint16_t *sec_flags,
			 uint16_t *channel_id)
{
	uint8_t choice = 0;

	if (v2r_mcs_recv(s->mcs, p, &choice, channel_id) == -1) {
		goto fail;
	}
	/* check if is send data request */
	if (choice != MCS_SEND_DATA_REQUEST) {
		goto fail;
	}
	/* parse security header */
	V2R_PACKET_READ_UINT16_LE(p, *sec_flags);
	/* skip flagsHi */
	V2R_PACKET_SEEK(p, 2);

	return 0;

fail:
	return -1;
}

int
v2r_sec_send(v2r_sec_t *s, v2r_packet_t *p, uint16_t sec_flags,
			 uint16_t channel_id)
{
	p->current = p->sec;
	V2R_PACKET_WRITE_UINT16_LE(p, sec_flags);
	V2R_PACKET_WRITE_UINT16_LE(p, 0);
	return v2r_mcs_send(s->mcs, p, MCS_SEND_DATA_INDICATION, channel_id);
}

void
v2r_sec_init_packet(v2r_packet_t *p)
{
	v2r_mcs_init_packet(p);
	p->sec = p->current;
	V2R_PACKET_SEEK(p, 4);
}

int
v2r_sec_generate_server_random(v2r_sec_t *s)
{
	int i;
	unsigned int seed;
	struct timespec tp;

	if (clock_gettime(CLOCK_MONOTONIC_COARSE, &tp) == -1) {
		v2r_log_error("get current time error: %s", ERRMSG);
		goto fail;
	}
	seed = (unsigned int)tp.tv_nsec;
	for (i = 0; i < SERVER_RANDOM_LEN; i++) {
		s->server_random[i] = rand_r(&seed);
	}

	return 0;

fail:
	return -1;
}

int
v2r_sec_write_server_certificate(v2r_sec_t *s, v2r_packet_t *p)
{
	/* Currently we only support server proprietary certificate */
	uint32_t keylen, bitlen, datalen;

	bitlen = 64 * 8;
	keylen = bitlen / 8 + 8;
	datalen = bitlen / 8 - 1;

	/* dwVersion */
	V2R_PACKET_WRITE_UINT32_LE(p, CERT_CHAIN_VERSION_1);
	/* dwSigAlgId */
	V2R_PACKET_WRITE_UINT32_LE(p, 0x00000001);
	/* dwKeyAlgId */
	V2R_PACKET_WRITE_UINT32_LE(p, 0x00000001);
	/* wPublicKeyBlobType */
	V2R_PACKET_WRITE_UINT16_LE(p, 0x0006);
	/* wPublicKeyBlobLen */
	V2R_PACKET_WRITE_UINT16_LE(p, 0);
	/* PublicKeyBlob */
	V2R_PACKET_WRITE_UINT16_LE(p, 0);
	/* magic */
	V2R_PACKET_WRITE_UINT32_LE(p, 0x31415352);
	/* keylen */
	V2R_PACKET_WRITE_UINT32_LE(p, keylen);
	/* bitlen */
	V2R_PACKET_WRITE_UINT32_LE(p, bitlen);
	/* datalen */
	V2R_PACKET_WRITE_UINT32_LE(p, datalen);
	/* TODO: fill in the pubExp and modulus field */

	return 0;
}

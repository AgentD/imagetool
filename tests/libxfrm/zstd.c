/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * zstd.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "xfrm.h"
#include "test.h"

#ifdef HAVE_ZSTD_STREAM
static const uint8_t zstd_in[] = {
	0x28, 0xb5, 0x2f, 0xfd, 0x04, 0x88, 0xa5, 0x08,
	0x00, 0x46, 0x97, 0x3a, 0x1a, 0x80, 0x37, 0xcd,
	0x01, 0xc0, 0x8a, 0xec, 0xfe, 0x2d, 0xf2, 0xb9,
	0x44, 0x6b, 0xb9, 0x24, 0x77, 0x56, 0x5a, 0x33,
	0x17, 0x0b, 0x67, 0x83, 0x2e, 0x47, 0x07, 0x31,
	0x00, 0x32, 0x00, 0x33, 0x00, 0xc5, 0x2c, 0x5a,
	0x92, 0x93, 0x0f, 0x7b, 0xd1, 0x1d, 0x63, 0x2c,
	0xc8, 0x99, 0x94, 0x77, 0x8f, 0x94, 0x38, 0x75,
	0x80, 0x2f, 0xae, 0xc1, 0x3e, 0xd2, 0xcf, 0x49,
	0x15, 0x25, 0x1a, 0x87, 0x93, 0xdd, 0xe8, 0x00,
	0x6d, 0xaa, 0xf8, 0x54, 0x74, 0xe5, 0x48, 0x4d,
	0xa6, 0xf3, 0x1a, 0xa3, 0x13, 0x08, 0xe5, 0x26,
	0xdc, 0x73, 0xcc, 0x3e, 0xfd, 0x86, 0xa9, 0x52,
	0xb2, 0x76, 0xc7, 0xc2, 0x0f, 0xe4, 0x84, 0x4b,
	0x12, 0x61, 0x3a, 0x6b, 0x7a, 0x1e, 0x8a, 0x81,
	0xa9, 0x9b, 0x11, 0x37, 0x25, 0x55, 0x73, 0x73,
	0x71, 0xa0, 0x84, 0xca, 0xc3, 0x4b, 0xb5, 0xcc,
	0x50, 0xa6, 0x46, 0xd7, 0xe8, 0x08, 0xaa, 0x04,
	0x28, 0xb1, 0x8e, 0xea, 0xb4, 0x4a, 0x49, 0x2b,
	0xd6, 0x0d, 0x59, 0x68, 0xda, 0x64, 0x29, 0x1f,
	0x85, 0x53, 0x72, 0xf1, 0xc5, 0x88, 0x1a, 0x0b,
	0x4f, 0x96, 0x43, 0xe0, 0x91, 0x89, 0xb9, 0xc0,
	0xe8, 0x18, 0xd5, 0x6e, 0x94, 0xe8, 0x35, 0x66,
	0x01, 0x94, 0x80, 0x95, 0x87, 0xe2, 0xc8, 0x19,
	0x73, 0xa3, 0x01, 0x05, 0xc1, 0x64, 0x72, 0xc9,
	0x6b, 0x6e, 0x55, 0x7c, 0x29, 0x67, 0x90, 0x93,
	0x49, 0xeb, 0xe3, 0x85, 0xc2, 0xf5, 0x79, 0x68,
	0x9d, 0x92, 0xc3, 0x32, 0x75, 0x80, 0x66, 0xf2,
	0x43, 0xa7, 0xb0, 0xc3, 0x22, 0x3f, 0x39, 0x8a,
	0x35, 0x5c, 0x63, 0x5c, 0xd1, 0x9e, 0x8a, 0xd2,
	0x78, 0x3c, 0x12, 0x01, 0x25, 0x04, 0x0e, 0x08,
	0x10, 0x88, 0xb6, 0x1b, 0xb7, 0x96, 0x35, 0xa8,
	0x0d, 0x1e, 0xae, 0xac, 0x4a, 0x70, 0xa5, 0x31,
	0xd0, 0x0c, 0x78, 0xbf, 0xdd, 0xc5, 0x24, 0x3e,
	0xcb, 0x0a, 0x0a, 0x69, 0x40, 0xba, 0xb0, 0xc4,
	0x2a, 0x9b, 0x1e, 0x0a, 0x51, 0xa6, 0x16, 0x98,
	0x76
};

static const char orig[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod\n"
"tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam,\n"
"quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo\n"
"consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse\n"
"cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non\n"
"proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n";

int main(void)
{
	uint32_t in_diff = 0, out_diff = 0;
	xfrm_stream_t *xfrm;
	char buffer[1024];
	int ret;

	xfrm = decompressor_stream_zstd_create();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, zstd_in, sizeof(zstd_in),
				 buffer, sizeof(buffer),
				 &in_diff, &out_diff, 0);
	TEST_EQUAL_I(ret, XFRM_STREAM_OK);

	TEST_EQUAL_UI(in_diff, sizeof(zstd_in));
	TEST_EQUAL_UI(out_diff, sizeof(orig) - 1);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	fwrite(buffer, 1, out_diff, stderr);

	object_drop(xfrm);
	return EXIT_SUCCESS;
}
#else /* HAVE_ZSTD_STREAM */
int main(void)
{
	return EXIT_SUCCESS;
}
#endif /* HAVE_ZSTD_STREAM */

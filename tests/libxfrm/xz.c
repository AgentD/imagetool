/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm.h"
#include "test.h"

static const uint8_t xz_in[] = {
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x00,
	0xff, 0x12, 0xd9, 0x41, 0x02, 0x00, 0x21, 0x01,
	0x1c, 0x00, 0x00, 0x00, 0x10, 0xcf, 0x58, 0xcc,
	0xe0, 0x01, 0xbd, 0x01, 0x43, 0x5d, 0x00, 0x26,
	0x1b, 0xca, 0x46, 0x67, 0x5a, 0xf2, 0x77, 0xb8,
	0x7d, 0x86, 0xd8, 0x41, 0xdb, 0x05, 0x35, 0xcd,
	0x83, 0xa5, 0x7c, 0x12, 0xa5, 0x05, 0xdb, 0x90,
	0xbd, 0x2f, 0x14, 0xd3, 0x71, 0x72, 0x96, 0xa8,
	0x8a, 0x7d, 0x84, 0x56, 0x71, 0x8d, 0x6a, 0x22,
	0x98, 0xab, 0x9e, 0x3d, 0xc3, 0x55, 0xef, 0xcc,
	0xa5, 0xc3, 0xdd, 0x5b, 0x8e, 0xbf, 0x03, 0x81,
	0x21, 0x40, 0xd6, 0x26, 0x91, 0x02, 0x45, 0x4e,
	0x20, 0x91, 0xcf, 0x8c, 0x51, 0x22, 0x02, 0x70,
	0xba, 0x05, 0x6b, 0x83, 0xef, 0x3f, 0x8e, 0x09,
	0xef, 0x88, 0xf5, 0x37, 0x1b, 0x89, 0x8d, 0xff,
	0x1e, 0xee, 0xe8, 0xb0, 0xac, 0xf2, 0x6e, 0xd4,
	0x3e, 0x25, 0xaf, 0xa0, 0x6d, 0x2e, 0xc0, 0x7f,
	0xb5, 0xa0, 0xcb, 0x90, 0x1f, 0x08, 0x1a, 0xe2,
	0x90, 0x20, 0x19, 0x71, 0x0c, 0xe8, 0x3f, 0xe5,
	0x39, 0xeb, 0x9a, 0x62, 0x4f, 0x06, 0xda, 0x3c,
	0x32, 0x59, 0xcc, 0x83, 0xe3, 0x83, 0x0f, 0x38,
	0x7d, 0x43, 0x37, 0x6c, 0x0b, 0x05, 0x65, 0x98,
	0x25, 0xdb, 0xf2, 0xc0, 0x2d, 0x39, 0x36, 0x5d,
	0xd4, 0xb6, 0xc2, 0x79, 0x73, 0x3e, 0xc2, 0x6e,
	0x54, 0xec, 0x78, 0x2b, 0x5d, 0xf1, 0xd1, 0xb4,
	0xb3, 0xcd, 0xf3, 0x89, 0xf5, 0x81, 0x3e, 0x2c,
	0x65, 0xd6, 0x73, 0xd3, 0x1b, 0x20, 0x68, 0x0c,
	0x93, 0xd4, 0xfc, 0x9f, 0xf8, 0xa7, 0xd4, 0xfa,
	0x3a, 0xb1, 0x13, 0x93, 0x4b, 0xec, 0x78, 0x7d,
	0x5c, 0x81, 0x80, 0xe5, 0x14, 0x78, 0xfe, 0x7e,
	0xde, 0xf7, 0xad, 0x9e, 0x84, 0xba, 0xf1, 0x00,
	0xe9, 0xbd, 0x2c, 0xf4, 0x70, 0x7d, 0xbe, 0x29,
	0xb9, 0xf0, 0x10, 0xb9, 0x01, 0xf1, 0x76, 0x8a,
	0x5a, 0xad, 0x02, 0xa1, 0x32, 0xc8, 0x53, 0x59,
	0x11, 0x4c, 0xe2, 0x98, 0x34, 0xd9, 0x23, 0x51,
	0x4a, 0x40, 0x2b, 0x87, 0x41, 0xdd, 0x50, 0xcd,
	0x98, 0x1e, 0x29, 0x86, 0x23, 0x93, 0x3e, 0x9b,
	0x6b, 0x16, 0xa1, 0x40, 0xac, 0xe7, 0x40, 0xfe,
	0xa9, 0x87, 0x48, 0x25, 0x52, 0x02, 0x8b, 0xc4,
	0x68, 0x08, 0x5a, 0x62, 0xc1, 0xb2, 0x07, 0x3b,
	0x26, 0x1e, 0x59, 0x5c, 0x47, 0x24, 0xae, 0x8e,
	0xe5, 0xf7, 0xe6, 0x4b, 0x13, 0xb4, 0x6d, 0x46,
	0x65, 0x4f, 0xd0, 0x48, 0xcc, 0x51, 0x4b, 0x80,
	0xcb, 0xf1, 0xd4, 0x6c, 0x45, 0x98, 0x92, 0x47,
	0xeb, 0x60, 0x00, 0x00, 0x00, 0x01, 0xd7, 0x02,
	0xbe, 0x03, 0x00, 0x00, 0xda, 0x2c, 0x45, 0x49,
	0xa8, 0x00, 0x0a, 0xfc, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x59, 0x5a
};

static const uint8_t xz_in_concat[] = {
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04,
	0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01,
	0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3,
	0xe0, 0x00, 0xdc, 0x00, 0xb3, 0x5d, 0x00, 0x26,
	0x1b, 0xca, 0x46, 0x67, 0x5a, 0xf2, 0x77, 0xb8,
	0x7d, 0x86, 0xd8, 0x41, 0xdb, 0x05, 0x35, 0xcd,
	0x83, 0xa5, 0x7c, 0x12, 0xa5, 0x05, 0xdb, 0x90,
	0xbd, 0x2f, 0x14, 0xd3, 0x71, 0x72, 0x96, 0xa8,
	0x8a, 0x7d, 0x84, 0x56, 0x71, 0x8d, 0x6a, 0x22,
	0x98, 0xab, 0x9e, 0x3d, 0xc3, 0x55, 0xef, 0xcc,
	0xa5, 0xc3, 0xdd, 0x5b, 0x8e, 0xbf, 0x03, 0x81,
	0x21, 0x40, 0xd6, 0x26, 0x91, 0x02, 0x45, 0x4e,
	0x20, 0x91, 0xcf, 0x8c, 0x51, 0x22, 0x02, 0x70,
	0xba, 0x05, 0x6b, 0x83, 0xef, 0x3f, 0x8e, 0x09,
	0xef, 0x88, 0xf5, 0x37, 0x1b, 0x89, 0x8d, 0xff,
	0x1e, 0xee, 0xe8, 0xb0, 0xac, 0xf2, 0x6e, 0xd4,
	0x3e, 0x25, 0xaf, 0xa0, 0x6d, 0x2e, 0xc0, 0x7f,
	0xb5, 0xa0, 0xcb, 0x90, 0x1f, 0x08, 0x1a, 0xe2,
	0x90, 0x20, 0x19, 0x71, 0x0c, 0xe8, 0x3f, 0xe5,
	0x39, 0xeb, 0x9a, 0x62, 0x4f, 0x06, 0xda, 0x3c,
	0x32, 0x59, 0xcc, 0x83, 0xe3, 0x83, 0x0f, 0x38,
	0x7d, 0x43, 0x37, 0x6c, 0x0b, 0x05, 0x65, 0x98,
	0x25, 0xdb, 0xf2, 0xc0, 0x2d, 0x39, 0x36, 0x5d,
	0xd4, 0xb6, 0xc2, 0x79, 0x73, 0x3e, 0xc2, 0x6e,
	0x54, 0xec, 0x78, 0x2b, 0x5d, 0xf1, 0xd1, 0xb4,
	0xb3, 0xcd, 0xf3, 0x89, 0xf5, 0x80, 0x79, 0x46,
	0xc0, 0x00, 0x00, 0x00, 0xc4, 0xf5, 0x1d, 0x08,
	0xf0, 0x34, 0x3a, 0x59, 0x00, 0x01, 0xcf, 0x01,
	0xdd, 0x01, 0x00, 0x00, 0x7f, 0x5a, 0x77, 0xcb,
	0xb1, 0xc4, 0x67, 0xfb, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x59, 0x5a,
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04,
	0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01,
	0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3,
	0xe0, 0x00, 0xe0, 0x00, 0xb7, 0x5d, 0x00, 0x31,
	0x9b, 0xca, 0x19, 0xc5, 0x54, 0xec, 0xb6, 0x54,
	0xe7, 0xb1, 0x7d, 0xc4, 0x57, 0x9e, 0x6c, 0x89,
	0xad, 0x4a, 0x6d, 0x16, 0xd8, 0x3c, 0x05, 0x94,
	0x10, 0x16, 0x99, 0x38, 0x21, 0xa3, 0xb9, 0xc5,
	0x80, 0xff, 0xfc, 0xee, 0xd4, 0xd5, 0x3f, 0xdd,
	0x8c, 0xd7, 0x3d, 0x8f, 0x76, 0xec, 0x96, 0x9d,
	0x20, 0xac, 0xcb, 0x18, 0xf5, 0xb2, 0x9c, 0x12,
	0xf6, 0x7c, 0x33, 0xdc, 0x4f, 0x9a, 0xe5, 0x2d,
	0x63, 0x68, 0xa4, 0x2b, 0x1d, 0x0a, 0x1e, 0xf0,
	0xfe, 0x73, 0xf2, 0x5f, 0x7b, 0xb4, 0xea, 0x54,
	0xad, 0x27, 0xd1, 0xff, 0xb6, 0x50, 0x06, 0x7b,
	0x51, 0x3f, 0x25, 0x8a, 0xcf, 0x4c, 0x03, 0x3e,
	0xc3, 0xad, 0x47, 0x34, 0xcf, 0xba, 0x45, 0x79,
	0xd0, 0x7b, 0xf6, 0x66, 0x63, 0xc0, 0xc6, 0x69,
	0xa7, 0x51, 0x84, 0xa8, 0xa0, 0x0b, 0xbc, 0x6f,
	0x13, 0x89, 0xd6, 0x5e, 0xac, 0xca, 0x2f, 0xd2,
	0xe7, 0xe1, 0x1e, 0x78, 0x22, 0x3a, 0x59, 0x6c,
	0x9c, 0x8c, 0x65, 0xf1, 0x5b, 0xf4, 0xbf, 0xd5,
	0xdc, 0x05, 0xeb, 0x70, 0x10, 0xb8, 0x6c, 0xf2,
	0x13, 0x20, 0xb0, 0xdd, 0x3e, 0xb2, 0x92, 0x5b,
	0xa3, 0xf7, 0x94, 0xa1, 0xa1, 0x74, 0x36, 0x9a,
	0xf1, 0xd8, 0xc2, 0xf0, 0xc6, 0x29, 0x7e, 0x85,
	0x28, 0xf5, 0xf2, 0x21, 0x00, 0x00, 0x00, 0x00,
	0xc8, 0x80, 0x67, 0x40, 0xc3, 0xaa, 0x17, 0x57,
	0x00, 0x01, 0xd3, 0x01, 0xe1, 0x01, 0x00, 0x00,
	0x86, 0xdf, 0x9e, 0x05, 0xb1, 0xc4, 0x67, 0xfb,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a
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

	/* normal XZ stream */
	xfrm = decompressor_stream_xz_create();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, xz_in, sizeof(xz_in),
				 buffer, sizeof(buffer),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(xz_in));
	TEST_EQUAL_UI(out_diff, sizeof(orig) - 1);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	object_drop(xfrm);

	/* concatenated XZ streams */
	xfrm = decompressor_stream_xz_create();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((object_t *)xfrm)->refcount, 1);

	in_diff = 0;
	out_diff = 0;

	ret = xfrm->process_data(xfrm, xz_in_concat, sizeof(xz_in_concat),
				 buffer, sizeof(buffer),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(xz_in_concat));
	TEST_EQUAL_UI(out_diff, sizeof(orig) - 1);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	object_drop(xfrm);
	return EXIT_SUCCESS;
}

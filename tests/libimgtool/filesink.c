/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesink.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesink.h"

#include "test.h"

static volume_t root_vol = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 512,

	.min_block_count = 0,
	.max_block_count = 10,

	.read_block = NULL,
	.write_block = NULL,
	.move_block = NULL,
	.discard_blocks = NULL,
	.commit = NULL,
};

static filesystem_t rootfs = {
	.base = {
		.destroy = NULL,
		.refcount = 1,
	},

	.fstree = NULL,
	.build_format = NULL,
};

/*****************************************************************************/

static char usr_buffer[64];
static size_t usr_blk_used = 0;

static int usr_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	if (buffer == NULL) {
		memset(usr_buffer + index * 16, 0, 16);
		if ((index + 1) == usr_blk_used)
			usr_blk_used -= 1;
	} else {
		memcpy(usr_buffer + index * 16, buffer, 16);
		if (index >= usr_blk_used)
			usr_blk_used = index + 1;
	}
	return 0;
}

static int usr_write_partial_block(volume_t *vol, uint64_t index,
				   const void *buffer, uint32_t offset,
				   uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	TEST_ASSERT(offset < 16);
	TEST_ASSERT(size <= (16 - offset));
	if (buffer == NULL) {
		memset(usr_buffer + index * 16 + offset, 0, size);
	} else {
		memcpy(usr_buffer + index * 16 + offset, buffer, size);
		if (index >= usr_blk_used)
			usr_blk_used = index + 1;
	}
	return 0;
}

static int usr_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	while (count--)
		usr_write_block(vol, index++, NULL);
	return 0;
}

static volume_t usr_vol = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 16,

	.min_block_count = 0,
	.max_block_count = 4,

	.write_block = usr_write_block,
	.write_partial_block = usr_write_partial_block,
	.discard_blocks = usr_discard_blocks,
};

static filesystem_t usrfs = {
	.base = {
		.destroy = NULL,
		.refcount = 1,
	},

	.fstree = NULL,
	.build_format = NULL,
};

/*****************************************************************************/

static volume_t boot_vol = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 512,

	.min_block_count = 0,
	.max_block_count = 10,

	.read_block = NULL,
	.write_block = NULL,
	.move_block = NULL,
	.discard_blocks = NULL,
	.commit = NULL,
};

static filesystem_t bootfs = {
	.base = {
		.destroy = NULL,
		.refcount = 1,
	},

	.fstree = NULL,
	.build_format = NULL,
};

/*****************************************************************************/

static char hello_str[] = "Hello, world!\n";

static const char *get_hello_filename(istream_t *strm)
{
	(void)strm;
	return "usr/bin/hello.txt";
}

static istream_t hello_file = {
	.base = {
		.destroy = NULL,
		.refcount = 1,
	},

	.buffer_used = 14,
	.buffer_offset = 0,
	.eof = true,
	.buffer = (uint8_t *)hello_str,

	.precache = NULL,
	.get_filename = get_hello_filename,
};

static file_source_record_t records[] = {
	{
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"lib",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"boot",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"dev",
	}, {
		.type = FILE_SOURCE_CHAR_DEV,
		.permissions = 0644,
		.devno = 1337,
		.full_path = (char *)"dev/console",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_FILE,
		.permissions = 0644,
		.full_path = (char *)"usr/bin/hello.txt",
	}, {
		.type = FILE_SOURCE_HARD_LINK,
		.permissions = 0777,
		.full_path = (char *)"usr/lib64",
		.link_target = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_SOCKET,
		.permissions = 0600,
		.uid = 1000,
		.gid = 100,
		.full_path = (char *)"boot/whatever",
	},
};

static size_t rec_idx = 0;

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	(void)fs;

	if (rec_idx >= sizeof(records) / sizeof(records[0]))
		return 1;

	*out = calloc(1, sizeof(**out));
	TEST_NOT_NULL(*out);

	memcpy(*out, &records[rec_idx], sizeof(**out));
	(*out)->full_path = strdup(records[rec_idx].full_path);
	TEST_NOT_NULL((*out)->full_path);

	if (records[rec_idx].link_target == NULL) {
		(*out)->link_target = NULL;
	} else {
		(*out)->link_target = strdup(records[rec_idx].link_target);
		TEST_NOT_NULL((*out)->link_target);
	}

	if (stream_out != NULL) {
		*stream_out = NULL;

		if (!strcmp((*out)->full_path, get_hello_filename(NULL))) {
			*stream_out = object_grab(&hello_file);
		}
	}

	rec_idx += 1;
	return 0;
}

static file_source_t filesource = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},
	.get_next_record = get_next_record,
};

/*****************************************************************************/

int main(void)
{
	tree_node_t *it, *jt;
	file_sink_t *sink;
	int ret;

	/* setup */
	rootfs.fstree = fstree_create(&root_vol);
	TEST_NOT_NULL(rootfs.fstree);
	bootfs.fstree = fstree_create(&boot_vol);
	TEST_NOT_NULL(bootfs.fstree);
	usrfs.fstree = fstree_create(&usr_vol);
	TEST_NOT_NULL(usrfs.fstree);

	sink = file_sink_create();
	TEST_NOT_NULL(sink);
	TEST_EQUAL_UI(((object_t *)sink)->refcount, 1);

	TEST_EQUAL_I(((object_t *)&bootfs)->refcount, 1);
	ret = file_sink_bind(sink, "/boot", &bootfs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(((object_t *)&bootfs)->refcount, 2);

	TEST_EQUAL_I(((object_t *)&rootfs)->refcount, 1);
	ret = file_sink_bind(sink, "/", &rootfs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(((object_t *)&rootfs)->refcount, 2);

	TEST_EQUAL_I(((object_t *)&usrfs)->refcount, 1);
	ret = file_sink_bind(sink, "/usr", &usrfs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(((object_t *)&usrfs)->refcount, 2);

	/* process the dummy source */
	ret = file_sink_add_data(sink, &filesource);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_I(((object_t *)&hello_file)->refcount, 1);

	fstree_sort(rootfs.fstree);
	fstree_sort(bootfs.fstree);
	fstree_sort(usrfs.fstree);

	/* check root filesystem */
	it = rootfs.fstree->root;
	TEST_NOT_NULL(it);
	it = it->data.dir.children;
	TEST_NOT_NULL(it);

	TEST_STR_EQUAL(it->name, "bin");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);
	TEST_NULL(it->data.dir.children);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "boot");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);
	TEST_NULL(it->data.dir.children);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "dev");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);

	jt = it->data.dir.children;
	TEST_NOT_NULL(jt);
	TEST_STR_EQUAL(jt->name, "console");
	TEST_EQUAL_UI(jt->type, FILE_SOURCE_CHAR_DEV);
	TEST_EQUAL_UI(jt->permissions, 0644);
	TEST_EQUAL_UI(jt->data.device_number, 1337);
	TEST_NULL(jt->next);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "lib");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);
	TEST_NULL(it->data.dir.children);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "usr");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);
	TEST_NULL(it->next);
	TEST_NULL(it->data.dir.children);

	/* check boot filesystem */
	it = bootfs.fstree->root;
	TEST_NOT_NULL(it);

	it = it->data.dir.children;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "whatever");
	TEST_EQUAL_UI(it->type, FILE_SOURCE_SOCKET);
	TEST_EQUAL_UI(it->permissions, 0600);
	TEST_EQUAL_UI(it->uid, 1000);
	TEST_EQUAL_UI(it->gid, 100);
	TEST_NULL(it->next);

	/* check user filesystem */
	it = usrfs.fstree->root;
	TEST_NOT_NULL(it);

	it = it->data.dir.children;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "bin");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);

	jt = it->data.dir.children;
	TEST_NOT_NULL(jt);
	TEST_STR_EQUAL(jt->name, "hello.txt");
	TEST_EQUAL_UI(jt->type, TREE_NODE_FILE);
	TEST_EQUAL_UI(jt->permissions, 0644);
	TEST_EQUAL_UI(jt->data.file.size, 14);
	TEST_EQUAL_UI(jt->data.file.start_index, 0);
	TEST_NULL(jt->data.file.sparse);
	TEST_NULL(jt->next);

	TEST_EQUAL_UI(usr_blk_used, 1);
	TEST_EQUAL_UI(usrfs.fstree->data_offset, 1);
	ret = memcmp(usr_buffer, hello_str, 14);
	TEST_EQUAL_I(ret, 0);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "lib");
	TEST_EQUAL_UI(it->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(it->permissions, 0755);

	it = it->next;
	TEST_NOT_NULL(it);
	TEST_STR_EQUAL(it->name, "lib64");
	TEST_EQUAL_UI(it->type, TREE_NODE_HARD_LINK);
	TEST_EQUAL_UI(it->permissions, 0777);
	TEST_STR_EQUAL(it->data.link.target, "lib");
	TEST_NULL(it->next);

	/* cleanup */
	object_drop(sink);

	TEST_EQUAL_I(((object_t *)&bootfs)->refcount, 1);
	TEST_EQUAL_I(((object_t *)&rootfs)->refcount, 1);
	TEST_EQUAL_I(((object_t *)&usrfs)->refcount, 1);

	rootfs.fstree = object_drop(rootfs.fstree);
	bootfs.fstree = object_drop(bootfs.fstree);
	usrfs.fstree = object_drop(usrfs.fstree);
	return EXIT_SUCCESS;
}

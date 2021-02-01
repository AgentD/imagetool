/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * format.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "cpiofs.h"

static const char *cpio_magic = "070701";
static const char *cpio_trailer = "TRAILER!!!";

int cpio_write_header(ostream_t *strm, tree_node_t *n, bool hardlink)
{
	uint32_t pathlen, dev_major = 0, dev_minor = 0, padding = 0;
	uint16_t mode = n->permissions;
	uint64_t size = 0;
	char *path;
	int ret;

	switch (n->type) {
	case TREE_NODE_DIR:
		mode |= S_IFDIR;
		break;
	case TREE_NODE_FILE:
		mode |= S_IFREG;
		size = hardlink ? 0 : n->data.file.size;
		break;
	case TREE_NODE_FIFO:
		mode |= S_IFIFO;
		break;
	case TREE_NODE_SOCKET:
		mode |= S_IFSOCK;
		break;
	case TREE_NODE_CHAR_DEV:
		mode |= S_IFCHR;
		dev_major = major(n->data.device_number);
		dev_minor = minor(n->data.device_number);
		break;
	case TREE_NODE_BLOCK_DEV:
		mode |= S_IFBLK;
		dev_major = major(n->data.device_number);
		dev_minor = minor(n->data.device_number);
		break;
	case TREE_NODE_SYMLINK:
		mode |= S_IFLNK;
		size = strlen(n->data.link.target) + 1;
		break;
	default:
		return 0;
	}

	path = fstree_get_path(n);
	if (path == NULL) {
		perror("generating cpio header");
		return -1;
	}

	fstree_canonicalize_path(path);
	pathlen = strlen(path);

	ret = ostream_printf(strm,
			     "%s%08X%08X%08lX%08lX%08X%08lX"
			     "%08X%08X%08X%08X%08X%08X%08X",
			     cpio_magic, n->inode_num,
			     mode, (long)n->uid, (long)n->gid, n->link_count,
			     (long)n->mtime,
			     size,
			     3, 1,
			     dev_major, dev_minor,
			     pathlen + 1, 0);
	if (ret < 0) {
		free(path);
		return -1;
	}

	if (ostream_append(strm, path, pathlen + 1))
		return -1;
	free(path);

	/* add null-terminator + header size, padd accordingly */
	pathlen += 1 + 110;

	if (pathlen % 4) {
		if (ostream_append(strm, &padding, 4 - (pathlen % 4)))
			return -1;
	}

	if (n->type == TREE_NODE_SYMLINK) {
		pathlen = strlen(n->data.link.target) + 1;

		if (ostream_append(strm, n->data.link.target, pathlen))
			return -1;

		if (pathlen % 4) {
			if (ostream_append(strm, &padding, 4 - (pathlen % 4)))
				return -1;
		}
	}
	return 0;
}

int cpio_write_trailer(uint64_t offset, ostream_t *strm)
{
	unsigned int namelen = strlen(cpio_trailer);
	int ret;

	ret = ostream_printf(strm,
			     "%s%08X%08X%08lX%08lX%08X%08lX"
			     "%08X%08X%08X%08X%08X%08X%08X",
			     cpio_magic, 0, 0, 0L, 0L, 1, 0L,
			     0, 0, 0, 0, 0, namelen + 1, 0);
	if (ret < 0)
		return -1;

	if (ostream_append(strm, cpio_trailer, namelen + 1))
		return -1;

	offset += (uint64_t)ret + namelen + 1;
	while (offset % 512) {
		if (ostream_append(strm, "\0", 1))
			return -1;
		++offset;
	}

	return 0;
}

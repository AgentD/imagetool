/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_header.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tarfs.h"

static unsigned int get_checksum(const tar_header_t *hdr)
{
	const unsigned char *header_start = (const unsigned char *)hdr;
	const unsigned char *chksum_start = (const unsigned char *)hdr->chksum;
	const unsigned char *header_end = header_start + sizeof(*hdr);
	const unsigned char *chksum_end = chksum_start + sizeof(hdr->chksum);
	const unsigned char *p;
	unsigned int chksum = 0;

	for (p = header_start; p < chksum_start; p++)
		chksum += *p;
	for (; p < chksum_end; p++)
		chksum += ' ';
	for (; p < header_end; p++)
		chksum += *p;
	return chksum;
}

static void update_checksum(tar_header_t *hdr)
{
	unsigned int chksum = get_checksum(hdr);

	sprintf(hdr->chksum, "%06o", chksum);
	hdr->chksum[6] = '\0';
	hdr->chksum[7] = ' ';
}

static void write_binary(char *dst, uint64_t value, int digits)
{
	memset(dst, 0, digits);

	while (digits > 0) {
		((unsigned char *)dst)[digits - 1] = value & 0xFF;
		--digits;
		value >>= 8;
	}

	((unsigned char *)dst)[0] |= 0x80;
}

static void write_number(char *dst, uint64_t value, int digits)
{
	uint64_t mask = 0;
	char buffer[64];
	int i;

	for (i = 0; i < (digits - 1); ++i)
		mask = (mask << 3) | 7;

	if (value <= mask) {
		sprintf(buffer, "%0*o ", digits - 1, (unsigned int)value);
		memcpy(dst, buffer, digits);
	} else if (value <= ((mask << 3) | 7)) {
		sprintf(buffer, "%0*o", digits, (unsigned int)value);
		memcpy(dst, buffer, digits);
	} else {
		write_binary(dst, value, digits);
	}
}

static void write_number_signed(char *dst, int64_t value, int digits)
{
	uint64_t neg;

	if (value < 0) {
		neg = -value;
		write_binary(dst, ~neg + 1, digits);
	} else {
		write_number(dst, value, digits);
	}
}

static int write_header(ostream_t *fp, const tree_node_t *n, const char *name,
			const char *slink_target, int type)
{
	int maj = 0, min = 0;
	uint64_t size = 0;
	tar_header_t hdr;

	if (n->type == TREE_NODE_CHAR_DEV || n->type == TREE_NODE_BLOCK_DEV) {
		maj = major(n->data.device_number);
		min = minor(n->data.device_number);
	}

	if (n->type == TREE_NODE_FILE)
		size = n->data.file.size;

	memset(&hdr, 0, sizeof(hdr));

	strncpy(hdr.name, name, sizeof(hdr.name) - 1);
	write_number(hdr.mode, n->permissions, sizeof(hdr.mode));
	write_number(hdr.uid, n->uid, sizeof(hdr.uid));
	write_number(hdr.gid, n->gid, sizeof(hdr.gid));
	write_number(hdr.size, size, sizeof(hdr.size));
	write_number_signed(hdr.mtime, n->mtime, sizeof(hdr.mtime));
	hdr.typeflag = type;
	if (slink_target != NULL)
		memcpy(hdr.linkname, slink_target, strlen(slink_target));
	memcpy(hdr.magic, TAR_MAGIC_OLD, sizeof(hdr.magic));
	memcpy(hdr.version, TAR_VERSION_OLD, sizeof(hdr.version));
	sprintf(hdr.uname, "%u", n->uid);
	sprintf(hdr.gname, "%u", n->gid);
	write_number(hdr.devmajor, maj, sizeof(hdr.devmajor));
	write_number(hdr.devminor, min, sizeof(hdr.devminor));

	update_checksum(&hdr);

	return ostream_append(fp, &hdr, sizeof(hdr));
}

static int write_gnu_header(ostream_t *fp, const tree_node_t *node,
			    const char *payload, size_t payload_len,
			    int type, const char *name)
{
	tree_node_t dummy = *node;
	size_t padd_sz;

	dummy.type = TREE_NODE_FILE;
	dummy.permissions = 0644;
	dummy.data.file.size = payload_len;
	dummy.next = NULL;
	dummy.parent = NULL;
	dummy.next_by_type = NULL;
	dummy.name = NULL;

	if (write_header(fp, &dummy, name, NULL, type))
		return -1;

	if (ostream_append(fp, payload, payload_len))
		return -1;

	padd_sz = payload_len % TAR_RECORD_SIZE;
	if (padd_sz == 0)
		return 0;

	padd_sz = TAR_RECORD_SIZE - padd_sz;
	return ostream_append_sparse(fp, padd_sz);
}

int tarfs_write_header(ostream_t *fp, const tree_node_t *n,
		       unsigned int counter)
{
	const char *reason, *name, *slink_target = NULL;
	char buffer[64], *path;
	int type, ret;
	size_t len;

	if (n->type == TREE_NODE_SYMLINK) {
		slink_target = n->data.link.target;
		len = strlen(slink_target);

		if (len >= 100) {
			sprintf(buffer, "gnu/target%u", counter);
			if (write_gnu_header(fp, n, n->data.link.target, len,
					     TAR_TYPE_GNU_SLINK, buffer))
				return -1;
			slink_target = NULL;
		}
	}

	path = fstree_get_path(n);
	if (path == NULL) {
		perror("generating tar file header");
		return -1;
	}

	fstree_canonicalize_path(path);
	name = path;

	if (strlen(name) >= 100) {
		sprintf(buffer, "gnu/name%u", counter);

		if (write_gnu_header(fp, n, name, strlen(name),
				     TAR_TYPE_GNU_PATH, buffer)) {
			free(path);
			return -1;
		}

		sprintf(buffer, "gnu/data%u", counter);
		name = buffer;
	}

	switch (n->type) {
	case TREE_NODE_CHAR_DEV: type = TAR_TYPE_CHARDEV; break;
	case TREE_NODE_BLOCK_DEV: type = TAR_TYPE_BLOCKDEV; break;
	case TREE_NODE_SYMLINK: type = TAR_TYPE_SLINK; break;
	case TREE_NODE_FILE: type = TAR_TYPE_FILE; break;
	case TREE_NODE_DIR: type = TAR_TYPE_DIR; break;
	case TREE_NODE_FIFO: type = TAR_TYPE_FIFO; break;
	case TREE_NODE_SOCKET:
		reason = "cannot pack socket";
		goto out_skip;
	default:
		reason = "unknown type";
		goto out_skip;
	}

	ret = write_header(fp, n, name, slink_target, type);
	free(path);
	return ret;
out_skip:
	fprintf(stderr, "WARNING: %s: %s\n", name, reason);
	free(path);
	return 1;
}

int tarfs_write_hard_link(ostream_t *fp, const tree_node_t *n,
			  unsigned int counter)
{
	tar_header_t hdr;
	char buffer[64];
	size_t len;
	char *path;

	memset(&hdr, 0, sizeof(hdr));

	len = strlen(n->data.link.target);
	if (len >= 100) {
		sprintf(buffer, "gnu/target%u", counter);
		if (write_gnu_header(fp, n, n->data.link.target, len,
				     TAR_TYPE_GNU_SLINK, buffer))
			return -1;
		sprintf(hdr.linkname, "hardlink_%u", counter);
	} else {
		memcpy(hdr.linkname, n->data.link.target, len);
	}

	path = fstree_get_path(n);
	if (path == NULL) {
		perror("generating tar hard link header");
		return -1;
	}

	fstree_canonicalize_path(path);

	len = strlen(path);
	if (len >= 100) {
		sprintf(buffer, "gnu/name%u", counter);
		if (write_gnu_header(fp, n, path, len,
				     TAR_TYPE_GNU_PATH, buffer)) {
			free(path);
			return -1;
		}
		sprintf(hdr.name, "gnu/data%u", counter);
	} else {
		memcpy(hdr.name, path, len);
	}

	write_number(hdr.mode, n->permissions, sizeof(hdr.mode));
	write_number(hdr.uid, n->uid, sizeof(hdr.uid));
	write_number(hdr.gid, n->gid, sizeof(hdr.gid));
	write_number(hdr.size, 0, sizeof(hdr.size));
	write_number_signed(hdr.mtime, n->mtime, sizeof(hdr.mtime));
	hdr.typeflag = TAR_TYPE_LINK;
	memcpy(hdr.magic, TAR_MAGIC_OLD, sizeof(hdr.magic));
	memcpy(hdr.version, TAR_VERSION_OLD, sizeof(hdr.version));
	sprintf(hdr.uname, "%u", n->uid);
	sprintf(hdr.gname, "%u", n->gid);
	write_number(hdr.devmajor, 0, sizeof(hdr.devmajor));
	write_number(hdr.devminor, 0, sizeof(hdr.devminor));

	free(path);
	update_checksum(&hdr);
	return ostream_append(fp, &hdr, sizeof(hdr));
}

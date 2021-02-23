/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dirent.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

struct short_list_t {
	struct short_list_t *next;
	uint16_t used;
	uint8_t name[11 * 10];
};

static bool is_in_short_name_list(struct short_list_t *list,
				  const uint8_t name[11])
{
	struct short_list_t *it = list;
	size_t i;

	for (it = list; it != NULL; it = it->next) {
		for (i = 0; i < (size_t)it->used; ++i) {
			if (memcmp(it->name + i * 11, name, 11) == 0)
				return true;
		}
	}

	return false;
}

static int add_to_short_name_list(struct short_list_t **list,
				  const uint8_t name[11])
{
	struct short_list_t *ent, *it;

	for (it = *list; it != NULL; it = it->next) {
		if ((size_t)it->used < (sizeof(it->name) / 11)) {
			memcpy(it->name + it->used * 11, name, 11);
			it->used += 1;
			return 0;
		}
	}

	ent = calloc(1, sizeof(*ent));
	if (ent == NULL) {
		perror("memorizing FAT short name");
		return -1;
	}

	memcpy(ent->name, name, 11);
	ent->next = *list;
	ent->used = 1;

	*list = ent;
	return 0;
}

static void cleanup_short_name_list(struct short_list_t *list)
{
	while (list != NULL) {
		struct short_list_t *ent = list;
		list = list->next;
		free(ent);
	}
}

/*****************************************************************************/

static bool is_non_ascii(const uint8_t *str)
{
	while (*str != '\0') {
		if (*(str++) & 0x80)
			return true;
	}

	return false;
}

static uint8_t short_name_checksum(const uint8_t *shortname)
{
	uint8_t sum = 0;
	int i;

	for (i = 11; i; --i)
		sum = ((sum & 1) << 7) + (sum >> 1) + *(shortname++);

	return sum;
}

static uint32_t get_cluster_index(fatfs_filesystem_t *fs, uint64_t location)
{
	uint64_t index = location / (SECTOR_SIZE * fs->secs_per_cluster);

	return (uint32_t)(index + CLUSTER_OFFSET);
}

/*****************************************************************************/

static int write_short_entry(fatfs_filesystem_t *fs, tree_node_t *n,
			     ostream_t *ostrm, uint8_t shortname[11])
{
	uint32_t location, size, ctime, mtime;
	fatfs_dir_ent_t ent;
	char *path;

	memset(&ent, 0, sizeof(ent));
	memcpy(ent.name, shortname, 8);
	memcpy(ent.extension, shortname + 8, 3);

	switch (n->type) {
	case TREE_NODE_DIR:
		location = get_cluster_index(fs, n->data.dir.start);
		size = (uint32_t)n->data.dir.size;

		ent.flags |= DIR_ENT_DIRECTORY;
		break;
	case TREE_NODE_FILE:
		location = (uint32_t)n->data.file.start_index + CLUSTER_OFFSET;
		size = (uint32_t)n->data.file.size;
		break;
	default:
		path = fstree_get_path(n);
		if (path != NULL)
			fstree_canonicalize_path(path);

		fprintf(stderr, "%s: cannot store non-file entry "
			"on FAT filesystem\n", path);
		free(path);
		return -1;
	}

	ent.cluster_index_high = htole16((uint16_t)(location >> 16));
	ent.cluster_index_low = htole16((uint16_t)location);
	ent.size = htole32(size);

	ctime = fatfs_convert_timestamp(n->ctime);
	mtime = fatfs_convert_timestamp(n->mtime);

	ent.ctime_hms = htole16( ctime        & 0x0FFFF);
	ent.ctime_ymd = htole16((ctime >> 16) & 0x0FFFF);
	ent.mtime_hms = htole16( mtime        & 0x0FFFF);
	ent.mtime_ymd = htole16((mtime >> 16) & 0x0FFFF);
	ent.atime_ymd = ent.mtime_ymd;

	return ostream_append(ostrm, &ent, sizeof(ent));
}

static int write_long_entry(ostream_t *ostrm, const uint8_t *name,
			    const uint8_t shortname[11])
{
	size_t i, count, len = strlen((const char *)name) + 1;
	uint8_t checksum = short_name_checksum(shortname);

	count = len / FAT_CHAR_PER_LONG_ENT;
	if (len % FAT_CHAR_PER_LONG_ENT)
		count += 1;

	for (i = 0; i < count; ++i) {
		fatfs_long_dir_ent_t lent;
		size_t offset = (count - 1 - i) * FAT_CHAR_PER_LONG_ENT;
		size_t avail = len - offset;
		size_t j;

		avail = avail > FAT_CHAR_PER_LONG_ENT ?
			FAT_CHAR_PER_LONG_ENT : avail;

		memset(&lent, 0, sizeof(lent));
		memset(lent.name_part, 0xFF, sizeof(lent.name_part));
		memset(lent.name_part2, 0xFF, sizeof(lent.name_part2));
		memset(lent.name_part3, 0xFF, sizeof(lent.name_part3));

		lent.seq_number = (count - i);
		if (i == 0)
			lent.seq_number |= SEQ_NUMBER_LAST_FLAG;

		lent.attrib = DIR_ENT_LFN;
		lent.short_checksum = checksum;

		for (j = 0; j < avail; ++j) {
			uint16_t wchar = htole16((uint16_t)name[offset + j]);
			size_t idx = j;

			if (idx < (sizeof(lent.name_part) / sizeof(uint16_t))) {
				lent.name_part[idx] = wchar;
				continue;
			}

			idx -= (sizeof(lent.name_part) / sizeof(uint16_t));

			if (idx < (sizeof(lent.name_part2) / sizeof(uint16_t))) {
				lent.name_part2[idx] = wchar;
				continue;
			}

			idx -= (sizeof(lent.name_part2) / sizeof(uint16_t));
			lent.name_part3[idx] = wchar;
		}

		if (ostream_append(ostrm, &lent, sizeof(lent)))
			return -1;
	}

	return 0;
}

static int init_directory(fatfs_filesystem_t *fatfs, tree_node_t *n,
			  ostream_t *ostrm)
{
	fatfs_dir_ent_t ent;
	uint32_t location;

	if (n->parent == NULL)
		return 0;

	memset(&ent, 0, sizeof(ent));
	memset(ent.name, ' ', sizeof(ent.name));
	memset(ent.extension, ' ', sizeof(ent.extension));
	ent.flags |= DIR_ENT_DIRECTORY;

	/* dot entry */
	ent.name[0] = '.';

	location = get_cluster_index(fatfs, n->data.dir.start);
	ent.cluster_index_high = htole16((uint16_t)(location >> 16));
	ent.cluster_index_low = htole16((uint16_t)location);

	if (ostream_append(ostrm, &ent, sizeof(ent)))
		return -1;

	/* dot-dot entry */
	ent.name[1] = '.';

	if (n->parent->parent == NULL) {
		location = 0;
	} else {
		location = get_cluster_index(fatfs, n->parent->data.dir.start);
	}

	ent.cluster_index_high = htole16((uint16_t)(location >> 16));
	ent.cluster_index_low = htole16((uint16_t)location);

	return ostream_append(ostrm, &ent, sizeof(ent));
}

int fatfs_serialize_directory(fatfs_filesystem_t *fatfs, tree_node_t *root,
			      ostream_t *ostrm)
{
	struct short_list_t *list = NULL;
	uint8_t shortname[11];
	FAT_SHORTNAME conv;
	unsigned int gen;
	tree_node_t *it;
	int ret;

	if (init_directory(fatfs, root, ostrm))
		return -1;

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (is_non_ascii((const uint8_t *)it->name))
			goto fail_conv;

		gen = 1;

		do {
			conv = fatfs_mk_shortname((const uint8_t *)it->name,
						  shortname, strlen(it->name),
						  gen);
			if (conv == FAT_SHORTNAME_ERROR)
				goto fail_conv;

			gen += 1;
		} while (is_in_short_name_list(list, shortname));

		if (add_to_short_name_list(&list, shortname))
			goto fail;

		if (conv == FAT_SHORTNAME_OK ||
		    conv == FAT_SHORTNAME_SUFFIXED) {
			ret = write_long_entry(ostrm, (const uint8_t *)it->name,
					       shortname);
			if (ret)
				goto fail;
		}

		if (write_short_entry(fatfs, it, ostrm, shortname))
			goto fail;
	}

	cleanup_short_name_list(list);
	return 0;
fail_conv:
	fprintf(stderr, "%s: cannot convert to a FAT filename\n", it->name);
fail:
	cleanup_short_name_list(list);
	return -1;
}

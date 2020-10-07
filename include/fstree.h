
/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREE_H
#define FSTREE_H

#include "config.h"
#include "predef.h"

enum {
	TREE_NODE_DIR,
	TREE_NODE_FILE,
	TREE_NODE_FIFO,
	TREE_NODE_SOCKET,
	TREE_NODE_CHAR_DEV,
	TREE_NODE_BLOCK_DEV,
	TREE_NODE_SYMLINK,
	TREE_NODE_HARD_LINK,
};

typedef struct {
	uint64_t disk_location;
	uint64_t file_location;
	uint64_t size;
} file_block_t;

typedef struct tree_node_t {
	uint64_t ctime;
	uint64_t mtime;

	uint32_t uid;
	uint32_t gid;
	uint32_t inode_num;
	uint32_t link_count;

	struct tree_node_t *next;
	struct tree_node_t *parent;

	const char *name;

	uint16_t type;
	uint16_t permissions;

	union {
		struct {
			struct tree_node_t *children;

			bool created_implicitly;
		} dir;

		struct {
			size_t max_blocks;
			size_t num_blocks;
			file_block_t *blocks;
		} file;

		struct {
			const char *target;

			struct tree_node_t *resolved;
		} link;

		uint32_t device_number;
	} data;

	uint8_t payload[];
} tree_node_t;

typedef struct {
	object_t base;

	/* default settings for implicitly created directories */
	uint64_t default_ctime;
	uint64_t default_mtime;
	uint32_t default_uid;
	uint32_t default_gid;
	uint16_t default_permissions;

	tree_node_t *root;

	size_t num_inodes;
	tree_node_t **inode_table;
} fstree_t;

#ifdef __cplusplus
extern "C" {
#endif

fstree_t *fstree_create(void);

void fstree_canonicalize_path(char *path);

tree_node_t *fstree_node_from_path(fstree_t *fs, const char *path,
				   size_t len, bool create_implicit);

char *fstree_get_path(tree_node_t *node);

tree_node_t *fstree_add_direcory(fstree_t *fs, const char *path);

tree_node_t *fstree_add_file(fstree_t *fs, const char *path);

tree_node_t *fstree_add_fifo(fstree_t *fs, const char *path);

tree_node_t *fstree_add_socket(fstree_t *fs, const char *path);

tree_node_t *fstree_add_block_device(fstree_t *fs, const char *path,
				     uint32_t devno);

tree_node_t *fstree_add_character_device(fstree_t *fs, const char *path,
					 uint32_t devno);

tree_node_t *fstree_add_symlink(fstree_t *fs, const char *path,
				const char *target);

tree_node_t *fstree_add_hard_link(fstree_t *fs, const char *path,
				  const char *target);

tree_node_t *fstree_sort_node_list(tree_node_t *head);

void fstree_sort_node_recursive(tree_node_t *root);

void fstree_sort(fstree_t *tree);

int fstree_resolve_hard_links(fstree_t *fs);

int fstree_create_inode_table(fstree_t *fs);

int fstree_add_file_block(tree_node_t *n, uint64_t on_disk_location,
			  uint64_t in_file_location, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FSTREE_H */

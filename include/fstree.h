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
#include "volume.h"

enum {
	TREE_NODE_DIR = 0,
	TREE_NODE_FILE,
	TREE_NODE_FIFO,
	TREE_NODE_SOCKET,
	TREE_NODE_CHAR_DEV,
	TREE_NODE_BLOCK_DEV,
	TREE_NODE_SYMLINK,
	TREE_NODE_HARD_LINK,

	TREE_NODE_TYPE_COUNT
};

typedef struct file_sparse_holes_t {
	struct file_sparse_holes_t *next;

	uint32_t index;
	uint32_t count;
} file_sparse_holes_t;

typedef struct tree_node_t {
	uint64_t ctime;
	uint64_t mtime;

	uint32_t uid;
	uint32_t gid;
	uint32_t inode_num;
	uint32_t link_count;

	struct tree_node_t *next;
	struct tree_node_t *parent;

	/* used in nodes_by_type lists in fstree_t */
	struct tree_node_t *next_by_type;

	const char *name;

	uint16_t type;
	uint16_t permissions;

	union {
		struct {
			struct tree_node_t *children;

			bool created_implicitly;
		} dir;

		struct {
			/* total size in bytes */
			uint64_t size;

			/* index of the first block */
			uint64_t start_index;

			/* Linked list of sparse regions */
			file_sparse_holes_t *sparse;
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

	tree_node_t *nodes_by_type[TREE_NODE_TYPE_COUNT];

	/* default settings for implicitly created directories */
	uint64_t default_ctime;
	uint64_t default_mtime;
	uint32_t default_uid;
	uint32_t default_gid;
	uint16_t default_permissions;

	tree_node_t *root;

	size_t num_inodes;
	tree_node_t **inode_table;

	volume_t *volume;
	uint64_t data_offset;
} fstree_t;

#ifdef __cplusplus
extern "C" {
#endif

fstree_t *fstree_create(volume_t *volume);

void fstree_canonicalize_path(char *path);

tree_node_t *fstree_node_from_path(fstree_t *fs, tree_node_t *root,
				   const char *path, size_t len,
				   bool create_implicit);

char *fstree_get_path(tree_node_t *node);

tree_node_t *fstree_add_directory(fstree_t *fs, const char *path);

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

void fstree_sort(fstree_t *tree);

int fstree_resolve_hard_links(fstree_t *fs);

int fstree_create_inode_table(fstree_t *fs);

uint64_t fstree_file_sparse_bytes(const fstree_t *fs, const tree_node_t *n);

uint64_t fstree_file_physical_size(const fstree_t *fs, const tree_node_t *n);

int fstree_file_mark_sparse(tree_node_t *n, uint64_t index);

int fstree_file_mark_not_sparse(tree_node_t *n, uint64_t index);

int fstree_file_read(fstree_t *fs, tree_node_t *n,
		     uint64_t offset, void *data, size_t size);

int fstree_file_move_to_end(fstree_t *fs, tree_node_t *n);

int fstree_file_append(fstree_t *fs, tree_node_t *n,
		       const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FSTREE_H */

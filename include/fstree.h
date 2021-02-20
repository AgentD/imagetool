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

enum {
	FSTREE_FLAG_NO_SPARSE = 0x01,
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

			/* Used by the filesystem driver to store the on disk
			   location of the FS specific data structure. */
			uint64_t start;

			/* Used by the filesystem driver to store the on disk
			   size of the FS specific data structure. */
			uint64_t size;

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

	uint64_t flags;
} fstree_t;

typedef int (*node_compare_fun_t)(const tree_node_t *lhs,
				  const tree_node_t *rhs);

#ifdef __cplusplus
extern "C" {
#endif

fstree_t *fstree_create(volume_t *volume);

void fstree_canonicalize_path(char *path);

tree_node_t *fstree_node_from_path(fstree_t *fs, tree_node_t *root,
				   const char *path, size_t len,
				   bool create_implicit);

char *fstree_get_path(const tree_node_t *node);

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

/*
  Sort a list of nodes using a custom comparison callback.

  The nodes are chained together using their next pointer, i.e. they
  represent the contents of a directory.
 */
tree_node_t *fstree_sort_node_list(tree_node_t *head, node_compare_fun_t cb);

/*
  Sort a list of nodes using a custom comparison callback.

  The nodes are chained together using their next_by_type pointer,
  i.e. they represent nodes with the same type, not necessarily in
  the same directory.
 */
tree_node_t *fstree_sort_node_list_by_type(tree_node_t *head,
					   node_compare_fun_t cb);

/*
  Recursively sort the entire tree by name.

  All the node by type lists are also sorted.
 */
void fstree_sort(fstree_t *tree);

int fstree_resolve_hard_links(fstree_t *fs);

int fstree_create_inode_table(fstree_t *fs);

/*
  Returns the total number of sparse bytes a file has. This can be less than
  the sum of all sparse blocks times the block size, since the last sparse
  region could actually be a tail end only.
 */
uint64_t fstree_file_sparse_bytes(const fstree_t *fs, const tree_node_t *n);

/*
  Returns the total number of bytes a file has stored on a volume. This is
  basically the files size minus the number of sparse bytes.
 */
uint64_t fstree_file_physical_size(const fstree_t *fs, const tree_node_t *n);

/*
  Mark a block as sparse. The given block index is relative to the beginning
  of the file.

  No actual data is changed, only the sparse file accounting of
  the file itself.

  Returns zero on success.
 */
int fstree_file_mark_sparse(tree_node_t *n, uint64_t index);

/*
  Mark a block as not being sparse. The given block index is relative to the
  beginning of the file.

  No actual data is changed, only the sparse file accounting of
  the file itself.

  Returns zero on success.
 */
int fstree_file_mark_not_sparse(tree_node_t *n, uint64_t index);

/*
  Read back data from a file at an arbitrary byte offset. Any read
  beyond the end of a file returns zero bytes.

  Returns zero on success.
 */
int fstree_file_read(fstree_t *fs, tree_node_t *n,
		     uint64_t offset, void *data, size_t size);

/*
  Move the on-disk data of a file to the end of the used region on the
  onderlying volume.

  Returns zero on success.
 */
int fstree_file_move_to_end(fstree_t *fs, tree_node_t *n);

/*
  Append a chunk of data to a file. The given pointer can be NULL to append a
  bunch of zero bytes. Sparse blocks are detected internally and the file
  accounting is updated accordingly.

  Returns zero on success.
 */
int fstree_file_append(fstree_t *fs, tree_node_t *n,
		       const void *data, size_t size);

/*
  Write a chunk of data at an arbitary byte offset in a file. Writing past
  the end-of-file causes data to be appended to the file. If writing starts
  after the end, the region in between is filled with (possibly sparse)
  zero-bytes.

  Internally takes care of breaking up sparse regions if writing into one, or
  detecting sparse block writes and updating the file accounting accordingly.

  Returns zero on success.
 */
int fstree_file_write(fstree_t *fs, tree_node_t *n,
		      uint64_t offset, const void *data, size_t size);

/*
  Make sure a file has the exact specified size. If data is cut off, the files
  that come after it are moved up. If the size is greater than the current file
  size, the file is expanded and padded with zero-bytes (if possible sparse).

  Returns zero on success.
 */
int fstree_file_truncate(fstree_t *fs, tree_node_t *n, uint64_t size);

/*
  Create an instance of a volume implementation that is itself based on
  an fstree file. An arbitrary block size can be specified, which doesn't
  have to be the same as the one of the underlying volume.
 */
volume_t *fstree_file_volume_create(fstree_t *fs, tree_node_t *n,
				    uint32_t blocksize, uint64_t min_size,
				    uint64_t max_size);
/*
  At a specified index on the underlying volume, move the file data out of
  the way to create space for a specified amount of bytes. The file accounting
  is adjusted accordingly.
 */
int fstree_add_gap(fstree_t *fs, uint64_t index, uint64_t size);

#ifdef __cplusplus
}
#endif

#endif /* FSTREE_H */

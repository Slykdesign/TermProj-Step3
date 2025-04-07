#include "ext2.h"
#include "partition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Ext2File *ext2Open(char *fn) {
    struct Ext2File *ext2 = malloc(sizeof(struct Ext2File));
    if (!ext2) return NULL;

    ext2->partition = openPartition(fn, 0);
    if (!ext2->partition) {
        free(ext2);
        return NULL;
    }

    if (!fetchSuperblock(ext2, 0, &ext2->superblock)) {
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    ext2->block_size = 1024 << ext2->superblock.s_log_block_size;
    ext2->num_block_groups = (ext2->superblock.s_blocks_count + ext2->superblock.s_blocks_per_group - 1) / ext2->superblock.s_blocks_per_group;

    ext2->bgdt = malloc(ext2->num_block_groups * sizeof(struct Ext2BlockGroupDescriptor));
    if (!ext2->bgdt) {
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    if (!fetchBGDT(ext2, 1, ext2->bgdt)) {
        free(ext2->bgdt);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    return ext2;
}

void ext2Close(struct Ext2File *f) {
    if (f) {
        closePartition(f->partition);
        free(f->bgdt);
        free(f);
    }
}

bool fetchBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (off_t)blockNum * f->block_size;
    if (lseekPartition(f->partition, offset, SEEK_SET) == (off_t)-1) return false;
    return readPartition(f->partition, buf, f->block_size) == f->block_size;
}

bool writeBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (off_t)blockNum * f->block_size;
    if (lseekPartition(f->partition, offset, SEEK_SET) == (off_t)-1) return false;
    return writePartition(f->partition, buf, f->block_size) == f->block_size;
}

bool fetchSuperblock(struct Ext2File *f, uint32_t blockNum, struct Ext2Superblock *sb) {
    if (!fetchBlock(f, blockNum, sb)) return false;
    return sb->s_magic == 0xEF53;
}

bool writeSuperblock(struct Ext2File *f, uint32_t blockNum, struct Ext2Superblock *sb) {
    return writeBlock(f, blockNum, sb);
}

bool fetchBGDT(struct Ext2File *f, uint32_t blockNum, struct Ext2BlockGroupDescriptor *bgdt) {
    return fetchBlock(f, blockNum, bgdt);
}

bool writeBGDT(struct Ext2File *f, uint32_t blockNum, struct Ext2BlockGroupDescriptor *bgdt) {
    return writeBlock(f, blockNum, bgdt);
}

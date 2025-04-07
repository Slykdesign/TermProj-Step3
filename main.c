#include "partition.h"
#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>

// Function prototypes
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);
void displaySuperblock(struct Ext2Superblock *sb);
void displayBGDT(struct Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups);

int main() {
    struct Ext2File *ext2 = ext2Open("./good-fixed-1k.vdi");
    if (!ext2) return 1;

    printf("Superblock from block 0\n");
    displaySuperblock(&ext2->superblock);

    printf("\nBlock group descriptor table:\n");
    displayBGDT(ext2->bgdt, ext2->num_block_groups);

    ext2Close(ext2);
    return 0;
}

// Function Definitions
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset) {
    printf("Offset: 0x%lx\n", offset);
    for (uint32_t i = skip; i < count; i += 16) {
        printf("%04x | ", i);
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%02x ", buf[i + j]);
        }
        printf("| ");
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
        printf("\n");
    }
}

void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset) {
    uint32_t step = 256;
    for (uint32_t i = 0; i < count; i += step) {
        displayBufferPage(buf, count, i, offset + i);
    }
}

void displaySuperblock(struct Ext2Superblock *sb) {
    printf("Superblock contents:\n");
    printf("  Number of inodes: %u\n", sb->s_inodes_count);
    printf("  Number of blocks: %u\n", sb->s_blocks_count);
    printf("  Number of reserved blocks: %u\n", sb->s_r_blocks_count);
    printf("  Number of free blocks: %u\n", sb->s_free_blocks_count);
    printf("  Number of free inodes: %u\n", sb->s_free_inodes_count);
    printf("  First data block: %u\n", sb->s_first_data_block);
    printf("  Log block size: %u (1024)\n", sb->s_log_block_size);
    printf("  Log fragment size: %u (1024)\n", sb->s_log_frag_size);
    printf("  Blocks per group: %u\n", sb->s_blocks_per_group);
    printf("  Fragments per group: %u\n", sb->s_frags_per_group);
    printf("  Inodes per group: %u\n", sb->s_inodes_per_group);
    printf("  Last mount time: %u\n", sb->s_mtime);
    printf("  Last write time: %u\n", sb->s_wtime);
    printf("  Mount count: %u\n", sb->s_mnt_count);
    printf("  Max mount count: %u\n", sb->s_max_mnt_count);
    printf("  Magic number: 0x%x\n", sb->s_magic);
    printf("  State: %u\n", sb->s_state);
    printf("  Error processing: %u\n", sb->s_errors);
    printf("  Revision level: %u\n", sb->s_minor_rev_level);
    printf("  Last system check: %u\n", sb->s_lastcheck);
    printf("  Check interval: %u\n", sb->s_checkinterval);
    printf("  OS creator: %u\n", sb->s_creator_os);
    printf("  Default reserve UID: %u\n", sb->s_def_resuid);
    printf("  Default reserve GID: %u\n", sb->s_def_resgid);
    printf("  First inode number: %u\n", sb->s_first_ino);
    printf("  Inode size: %u\n", sb->s_inode_size);
    printf("  Block group number: %u\n", sb->s_block_group_nr);
    printf("  Feature compatibility bits: 0x%x\n", sb->s_feature_compat);
    printf("  Feature incompatibility bits: 0x%x\n", sb->s_feature_incompat);
    printf("  Feature read/only compatibility bits: 0x%x\n", sb->s_feature_ro_compat);
    printf("  UUID: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", sb->s_uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) printf("-");
    }
    printf("\n");
    printf("  Volume name: %s\n", sb->s_volume_name);
    printf("  Last mount point: %s\n", sb->s_last_mounted);
    printf("  Algorithm bitmap: 0x%x\n", sb->s_algorithm_usage_bitmap);
    printf("  Number of blocks to preallocate: %u\n", sb->s_prealloc_blocks);
    printf("  Number of blocks to preallocate for directories: %u\n", sb->s_prealloc_dir_blocks);
    printf("  Journal UUID: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", sb->s_journal_uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) printf("-");
    }
    printf("\n");
    printf("  Journal inode number: %u\n", sb->s_journal_inum);
    printf("  Journal device number: %u\n", sb->s_journal_dev);
    printf("  Journal last orphan inode number: %u\n", sb->s_last_orphan);
    printf("  Default hash version: %u\n", sb->s_def_hash_version);
    printf("  Default mount option bitmap: 0x%x\n", sb->s_default_mount_opts);
    printf("  First meta block group: %u\n", sb->s_first_meta_bg);
}

void displayBGDT(struct Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups) {
    printf("Block group descriptor table:\n");
    printf("Block\tBlock\tInode\tInode\tFree\tFree\tUsed\n");
    printf("Number\tBitmap\tBitmap\tTable\tBlocks\tInodes\tDirs\n");
    printf("--------------------------------------\n");
    for (uint32_t i = 0; i < num_block_groups; i++) {
        printf("%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
               i, bgdt[i].bg_block_bitmap, bgdt[i].bg_inode_bitmap,
               bgdt[i].bg_inode_table, bgdt[i].bg_free_blocks_count,
               bgdt[i].bg_free_inodes_count, bgdt[i].bg_used_dirs_count);
    }
}

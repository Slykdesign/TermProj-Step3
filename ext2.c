#include "ext2.h"
#include "partition.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define SECTOR_SIZE 512
#define EXT2_MAGIC_NUMBER 0xEF53
#define EXT2_PARTITION_TYPE 0x83  // Define ext2 partition type

struct Ext2File *openExt2(char *fn) {
    struct Ext2File *ext2 = malloc(sizeof(struct Ext2File));
    if (!ext2) {
        perror("Failed to allocate memory for Ext2File");
        return NULL;
    }

    ext2->partition = openPartition(fn, 0);
    if (!ext2->partition) {
        perror("Failed to open MBR partition");
        free(ext2);
        return NULL;
    }

    int partIndex; // -1
    for (int i = 0; i < 4; ++i) {
        MBRPartitionEntry *entry = (MBRPartitionEntry *)(ext2->partition->partitionTable + i * sizeof(MBRPartitionEntry));
        if (entry->partitionType == EXT2_PARTITION_TYPE) {
            partIndex = i;
            break;
        }
    }

    if (partIndex < 0) {
        printf("No ext2 partition of type 0x%02X found.\n", EXT2_PARTITION_TYPE);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Open the ext2 partition
    struct Partition *tempPartition = openPartition(fn, partIndex);
    if (!tempPartition) {
        printf("Error in openExt2: Failed to open partition %d.\n", partIndex);
        free(ext2);
        return NULL;
    }
    ext2->partition = tempPartition;

    off_t superblockOffset = ext2->partition->startSector * SECTOR_SIZE + EXT2_SUPERBLOCK_OFFSET;

    if (vdiSeekPartition(ext2->partition, superblockOffset, SEEK_SET) != superblockOffset) {
        printf("Failed to seek to superblock in partition %d.\n", partIndex);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    if (!fetchSuperblock(ext2, 0, &ext2->superblock)) {
        printf("Failed to fetch the superblock in partition %d.\n", partIndex);
        closePartition(ext2->partition);
        free(ext2->bgdt); // Free bgdt if allocated
        free(ext2);       // Free ext2
        return NULL;
    }

    if (ext2->superblock.s_magic != EXT2_MAGIC_NUMBER) {
        printf("Invalid superblock magic number (0x%X).\n", ext2->superblock.s_magic);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    ext2->blockSize = 1024 << ext2->superblock.s_log_block_size;
    ext2->numBlockGroups = (ext2->superblock.s_blocks_count + ext2->superblock.s_blocks_per_group - 1) / ext2->superblock.s_blocks_per_group;

    ext2->bgdt = malloc(ext2->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));
    if (!ext2->bgdt) {
        printf("Failed to allocate memory for block group descriptor table.\n");
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    uint32_t bgdt_block = (ext2->blockSize == 1024) ? 2 : 1;
    if (!fetchBGDT(ext2, bgdt_block, ext2->bgdt)) {
        printf("Failed to fetch block group descriptor table.\n");
        closePartition(ext2->partition);
        free(ext2->bgdt);
        free(ext2);
        return NULL;
    }

    return ext2;
}

void closeExt2(struct Ext2File *f) {
    if (f) {
        closePartition(f->partition);
        free(f->bgdt);
        free(f);
    }
}

bool fetchBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    uint64_t offset = (blockNum + f->superblock.s_first_data_block) * f->blockSize;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) != offset) {
        printf("Failed to seek to block %u\n", blockNum);
        return false;
    }
    if (vdiReadPartition(f->partition, buf, f->blockSize) != f->blockSize) {
        printf("Failed to read block %u\n", blockNum);
        return false;
    }
    return true;
}

bool writeBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (blockNum + f->superblock.s_first_data_block) * f->blockSize;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) != offset) {
        printf("Failed to seek to block %u for writing\n", blockNum);
        return false;
    }
    if (writePartition(f->partition, buf, f->blockSize) != f->blockSize) {
        printf("Failed to write block %u\n", blockNum);
        return false;
    }
    return true;
}

bool fetchSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    // Read the superblock directly into the provided structure
    if (blockNum == 0) {
        if (vdiSeekPartition(f->partition, EXT2_SUPERBLOCK_OFFSET, SEEK_SET) != EXT2_SUPERBLOCK_OFFSET) {
            printf("Failed to seek to main superblock offset\n");
            return false;
        }
        if (vdiReadPartition(f->partition, sb, sizeof(Ext2Superblock)) != sizeof(Ext2Superblock)) {
            printf("Failed to read main superblock\n");
            return false;
        }
    } else {
        // For backup superblocks, calculate the offset based on block size
        uint32_t blockSize = 1024 << sb->s_log_block_size;
        if (blockNum > 0) {
            blockNum++;
        }

        // Seek and read the backup superblock directly
        off_t offset = blockNum * blockSize;
        if (vdiSeekPartition(f->partition, offset, SEEK_SET) != offset) {
            printf("Failed to seek to backup superblock at block %u\n", blockNum);
            return false;
        }
        if (vdiReadPartition(f->partition, sb, sizeof(Ext2Superblock)) != sizeof(Ext2Superblock)) {
            printf("Failed to read backup superblock at block %u\n", blockNum);
            return false;
        }
    }

    // Validate the magic number
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        printf("Invalid superblock magic number (0x%X).\n", sb->s_magic);
        return false;
    }
    return true;
}

bool writeSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    if (blockNum == 0) {
        if (vdiSeekPartition(f->partition, EXT2_SUPERBLOCK_OFFSET, SEEK_SET) != EXT2_SUPERBLOCK_OFFSET) {
            printf("Failed to seek to main superblock offset for writing\n");
            return false;
        }
        if (writePartition(f->partition, sb, sizeof(Ext2Superblock)) != sizeof(Ext2Superblock)) {
            printf("Failed to write main superblock\n");
            return false;
        }
    } else {
        uint32_t blockSize = f->blockSize;
        if (blockSize == 1024 && blockNum > 0) {
            blockNum++;
        }
        uint8_t *buf = (uint8_t *)malloc(blockSize);
        if (!buf) {
            printf("Failed to allocate buffer for backup superblock\n");
            return false;
        }
        memcpy(buf, sb, sizeof(Ext2Superblock));

        if (!writeBlock(f, blockNum, buf)) {
            printf("Failed to write backup superblock\n");
            free(buf);
            return false;
        }

        uint8_t *verifyBuf = malloc(blockSize);
        if (!verifyBuf) {
            printf("Failed to allocate buffer for verification\n");
            free(buf);
            return false;
        }

        if (!fetchBlock(f, blockNum, verifyBuf)) {
            printf("Failed to read back backup superblock for verification\n");
            free(buf);
            free(verifyBuf);
            return false;
        }

        Ext2Superblock verifySB;
        memcpy(&verifySB, verifyBuf, sizeof(Ext2Superblock));
        free(verifyBuf);

        if (verifySB.s_magic != EXT2_MAGIC_NUMBER) {
            printf("Invalid backup superblock after writing: 0x%x\n", verifySB.s_magic);
            free(buf);
            return false;
        }
        free(buf);
    }
    return true;
}

bool fetchBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    uint8_t *buffer = malloc(f->blockSize);
    if (!buffer) return false;

    if (!fetchBlock(f, blockNum, buffer)) {
        printf("Failed to fetch Block Group Descriptor Table at block %u\n", blockNum);
        free(buffer); // Free the buffer on error
        return false;
    }

    memcpy(bgdt, buffer, f->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));
    free(buffer); // Free the buffer after use

    return true;
}

bool writeBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    uint8_t *buffer = malloc(f->blockSize);
    if (!buffer) return false;

    memset(buffer, 0, f->blockSize);
    memcpy(buffer, bgdt, f->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));

    bool result = writeBlock(f, blockNum, buffer);
    free(buffer);

    return result;
}

// Function to display the contents of the superblock
void displaySuperblock(Ext2Superblock *sb) {
    printf("Superblock contents:\n"); // Print header for superblock display
    printf("Number of inodes: %u\n", sb->s_inodes_count); // Display total inodes
    printf("Number of blocks: %u\n", sb->s_blocks_count); // Display total blocks
    printf("Number of reserved blocks: %u\n", sb->s_r_blocks_count); // Display reserved blocks
    printf("Number of free blocks: %u\n", sb->s_free_blocks_count); // Display free blocks
    printf("Number of free inodes: %u\n", sb->s_free_inodes_count); // Display free inodes
    printf("First data block: %u\n", sb->s_first_data_block); // Display the first data block number
    printf("Log block size: %u (%u)\n", sb->s_log_block_size, 1024 << sb->s_log_block_size); // Display block size
    printf("Log fragment size: %u (%u)\n", sb->s_log_frag_size, 1024 << sb->s_log_frag_size); // Display fragment size
    printf("Blocks per group: %u\n", sb->s_blocks_per_group); // Display blocks per group
    printf("Fragments per group: %u\n", sb->s_frags_per_group); // Display fragments per group
    printf("Inodes per group: %u\n", sb->s_inodes_per_group); // Display inodes per group
    printf("Last mount time: %s", ctime((time_t*)&sb->s_mtime)); // Display last mount time
    printf("Last write time: %s", ctime((time_t*)&sb->s_wtime)); // Display last write time
    printf("Mount count: %u\n", sb->s_mnt_count); // Display current mount count
    printf("Max mount count: %u\n", sb->s_max_mnt_count); // Display max allowed mount count
    printf("Magic number: 0x%x\n", sb->s_magic); // Display magic number
    printf("State: %u\n", sb->s_state); // Display file system state
    printf("Error processing: %u\n", sb->s_errors); // Display error handling behavior
    printf("Revision level: %u.%u\n", sb->s_rev_level, sb->s_minor_rev_level); // Display revision level
    printf("Last system check: %s", ctime((time_t*)&sb->s_lastcheck)); // Display last system check time
    printf("Check interval: %u\n", sb->s_checkinterval); // Display system check interval
    printf("OS creator: %u\n", sb->s_creator_os); // Display OS creator ID
    printf("Default reserve UID: %u\n", sb->s_def_resuid); // Display default reserved UID
    printf("Default reserve GID: %u\n", sb->s_def_resgid); // Display default reserved GID
    printf("First inode number: %u\n", sb->s_first_ino); // Display first inode number
    printf("Inode size: %u\n", sb->s_inode_size); // Display inode size
    printf("Block group number: %u\n", sb->s_block_group_nr); // Display block group number
    printf("Feature compatibility bits: 0x%08x\n", sb->s_feature_compat); // Display feature compatibility bits
    printf("Feature incompatibility bits: 0x%08x\n", sb->s_feature_incompat); // Display feature incompatibility bits
    printf("Feature read/only compatibility bits: 0x%08x\n", sb->s_feature_ro_compat); // Display read-only compatibility bits
    printf("UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        sb->s_uuid[0], sb->s_uuid[1], sb->s_uuid[2], sb->s_uuid[3],
        sb->s_uuid[4], sb->s_uuid[5], sb->s_uuid[6], sb->s_uuid[7],
        sb->s_uuid[8], sb->s_uuid[9], sb->s_uuid[10], sb->s_uuid[11],
        sb->s_uuid[12], sb->s_uuid[13], sb->s_uuid[14], sb->s_uuid[15]); // Display file system UUID
    printf("Volume name: [%s]\n", sb->s_volume_name); // Display volume name
    printf("Last mount point: [%s]\n", sb->s_last_mounted); // Display last mount point
    printf("Algorithm bitmap: 0x%08x\n", sb->s_algorithm_usage_bitmap); // Display algorithm usage bitmap
    printf("Number of blocks to preallocate: %u\n", sb->s_prealloc_blocks); // Display number of blocks to preallocate
    printf("Number of blocks to preallocate for directories: %u\n", sb->s_prealloc_dir_blocks); // Display blocks preallocated for directories
    printf("Journal UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        sb->s_journal_uuid[0], sb->s_journal_uuid[1], sb->s_journal_uuid[2], sb->s_journal_uuid[3],
        sb->s_journal_uuid[4], sb->s_journal_uuid[5], sb->s_journal_uuid[6], sb->s_journal_uuid[7],
        sb->s_journal_uuid[8], sb->s_journal_uuid[9], sb->s_journal_uuid[10], sb->s_journal_uuid[11],
        sb->s_journal_uuid[12], sb->s_journal_uuid[13], sb->s_journal_uuid[14], sb->s_journal_uuid[15]); // Display journal UUID
    printf("Journal inode number: %u\n", sb->s_journal_inum); // Display journal inode number
    printf("Journal device number: %u\n", sb->s_journal_dev); // Display journal device number
    printf("Journal last orphan inode number: %u\n", sb->s_last_orphan); // Display last orphaned inode in journal
    printf("Default hash version: %u\n", sb->s_def_hash_version); // Display default hash version
    printf("Default mount option bitmap: 0x%08x\n", sb->s_default_mount_opts); // Display default mount options
    printf("First meta block group: %u\n", sb->s_first_meta_bg); // Display first meta block group
}

// Function to display the block group descriptor table
void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups) {
    printf("Block Group Descriptor Table:\n"); // Print header for descriptor table
    for (uint32_t i = 0; i < num_block_groups; i++) { // Iterate through all block groups
        printf("  Block number %u:\n", i); // Print block group number
        printf("    Block bitmap: %u\n", bgdt[i].bg_block_bitmap); // Display block bitmap location
        printf("    Inode bitmap: %u\n", bgdt[i].bg_inode_bitmap); // Display inode bitmap location
        printf("    Inode table: %u\n", bgdt[i].bg_inode_table); // Display inode table location
        printf("    Free blocks count: %u\n", bgdt[i].bg_free_blocks_count); // Display free blocks count
        printf("    Free inodes count: %u\n", bgdt[i].bg_free_inodes_count); // Display free inodes count
        printf("    Used directories count: %u\n", bgdt[i].bg_used_dirs_count); // Display used directories count
    }
}
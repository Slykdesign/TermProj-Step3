// partition.c
#include "partition.h"
#include "vdi.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Open a specific partition from a VDI file
MBRPartition* openPartition(const char *filename, int part) {
    // Allocate memory for the partition struct
    MBRPartition *partition = malloc(sizeof(MBRPartition));
    if (!partition) return NULL;

    // Open the VDI file
    partition->vdi = vdiOpen(filename);
    if (!partition->vdi) {
        free(partition);
        return NULL;
    }

    // Seek to the MBR partition table location (byte 446)
    lseek(partition->vdi->fd, 446, SEEK_SET);
    // Read the 64-byte partition table (4 entries)
    read(partition->vdi->fd, partition->partitionTable, 64);

    // Select the partition entry specified by 'part'
    uint8_t *entry = partition->partitionTable + part * 16;
    // Extract the starting LBA and sector count from the entry
    partition->startSector = *(uint32_t *)(entry + 8);
    partition->sectorCount = *(uint32_t *)(entry + 12);
    // Initialize cursor to beginning of partition
    partition->cursor = 0;

    return partition;
}

// Close the partition and free resources
void closePartition(MBRPartition *partition) {
    if (partition) {
        vdiClose(partition->vdi); // Close underlying VDI file
        free(partition);          // Free the partition struct
    }
}

// Read from a partition
ssize_t vdiReadPartition(MBRPartition *partition, void *buf, size_t count) {
    size_t bytesRead = 0;
    uint8_t *buffer = (uint8_t *)buf;

    // Read in chunks (up to 512 bytes per loop)
    while (count > 0) {
        // Calculate logical offset within the VDI
        off_t logicalOffset = partition->startSector * 512 + partition->cursor;
        // Translate to physical offset using VDI's metadata
        off_t physicalOffset = vdiTranslate(partition->vdi, logicalOffset);
        if (physicalOffset == -1) return bytesRead; // Translation failed

        // Read up to a sector (or remaining count)
        size_t toRead = (count < 512) ? count : 512;
        lseek(partition->vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = read(partition->vdi->fd, buffer, toRead);
        if (result <= 0) break; // EOF or error

        // Update counters and pointers
        bytesRead += result;
        buffer += result;
        count -= result;
        partition->cursor += result;
    }
    return bytesRead;
}

// Write to a partition
ssize_t writePartition(MBRPartition *partition, void *buf, size_t count) {
    size_t bytesWritten = 0;
    uint8_t *buffer = (uint8_t *)buf;

    // Write in chunks (up to 512 bytes per loop)
    while (count > 0) {
        off_t logicalOffset = partition->startSector * 512 + partition->cursor;
        off_t physicalOffset = vdiTranslate(partition->vdi, logicalOffset);
        if (physicalOffset == -1) return bytesWritten;

        size_t toWrite = (count < 512) ? count : 512;
        lseek(partition->vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = write(partition->vdi->fd, buffer, toWrite);
        if (result <= 0) break;

        bytesWritten += result;
        buffer += result;
        count -= result;
        partition->cursor += result;
    }
    return bytesWritten;
}

// Seek within the partition
off_t vdiSeekPartition(MBRPartition *partition, off_t offset, int anchor) {
    off_t newCursor = 0;  // Initialize the new cursor

    // Set newCursor based on anchor type
    if (anchor == SEEK_SET) {
        newCursor = offset;
    } else if (anchor == SEEK_CUR) {
        newCursor = partition->cursor + offset;
    } else if (anchor == SEEK_END) {
        newCursor = partition->sectorCount * 512 + offset;
    } else {
        return -1; // Invalid anchor
    }

    // Bounds checking to ensure we don't seek outside the partition
    if (newCursor < 0 || newCursor > (off_t)(partition->sectorCount * 512))
        return -1;

    partition->cursor = newCursor;
    return partition->cursor;
}

// Displays partition table entries for debugging
void displayPartitionTable(MBRPartition *partition) {
    for (int i = 0; i < 4; i++) {
        // Calculate pointer to the current partition entry (16 bytes per entry)
        uint8_t *entry = partition->partitionTable + i * 16;
        printf("Partition table entry %d:\n", i);
        // Display active/inactive status
        printf("  Status: %s\n", (entry[0] == 0x80) ? "Active" : "Inactive");
        // Display first sector CHS values (bytes 1-3)
        printf("  First sector CHS: %d-%d-%d\n", entry[1], entry[2], entry[3]);
        // Display last sector CHS values (bytes 5-7)
        printf("  Last sector CHS: %d-%d-%d\n", entry[5], entry[6], entry[7]);
        // Display partition type (byte 4)
        printf("  Partition type: %02x\n", entry[4]);
        // Display starting LBA (bytes 8-11)
        printf("  First LBA sector: %u\n", *(uint32_t *)(entry + 8));
        // Display total number of sectors (bytes 12-15)
        printf("  LBA sector count: %u\n", *(uint32_t *)(entry + 12));
    }
}
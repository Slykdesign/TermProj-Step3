#ifndef PARTITION_H
#define PARTITION_H

#include "vdi.h"

// Structure representing a single partition entry in the MBR
typedef struct {
    uint8_t status;          // Bootable flag (0x80 = bootable, 0x00 = not bootable)
    uint8_t firstCHS[3];     // CHS address of the first sector (not used much today)
    uint8_t partitionType;   // Partition type (e.g., 0x83 = Linux, 0x07 = NTFS)
    uint8_t lastCHS[3];      // CHS address of the last sector
    uint32_t firstLBA;       // Logical Block Address of first sector
    uint32_t sectorCount;    // Total number of sectors in partition
} MBRPartitionEntry;

// Structure representing an open partition within a VDI file
typedef struct {
    VDIFile *vdi;               // Pointer to the underlying open VDI file
    uint8_t partitionTable[64]; // Raw partition table data (4 entries × 16 bytes)
    uint32_t startSector;       // Start sector (LBA) of selected partition
    uint32_t sectorCount;       // Total sector count for selected partition
    size_t cursor;              // Logical read/write position within partition
} MBRPartition;

MBRPartition* openPartition(const char *filename, int part); // Open a partition from a VDI file (selecting by partition number 0–3)
void closePartition(MBRPartition *partition); // Close a previously opened partition
ssize_t vdiReadPartition(MBRPartition *partition, void *buf, size_t count); // Read bytes from the partition
ssize_t writePartition(MBRPartition *partition, void *buf, size_t count); // Write bytes to the partition
off_t vdiSeekPartition(MBRPartition *partition, off_t offset, int anchor); // Move the partition cursor (like lseek)
void displayPartitionTable(MBRPartition *partition); // Print a human-readable view of the partition table

#endif

#ifndef VDI_H
#define VDI_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// --- VDIHeader struct describes the layout of a VDI file header ---
typedef struct {
    char magic[4];          // Magic signature: should be "<<< "
    char creator[60];       // Text identifying VirtualBox ("Oracle VM VirtualBox Disk Image >>>")
    uint32_t signature;     // File signature: should be 0xbeda107f
    uint32_t version;       // Version of VDI format (e.g., 0x00010001)
    uint32_t headerSize;    // Size of the header in bytes
    uint32_t imageType;     // 0x00020000 for fixed-size or 0x00010000 for dynamic
    uint32_t flags;         // Flags for features or options
    uint64_t virtualSize;   // Size of the virtual disk
    uint32_t sectorSize;    // Size of a sector (usually 512 bytes)
    uint32_t cylinders;     // Number of cylinders (for CHS addressing)
    uint32_t heads;         // Number of heads (for CHS addressing)
    uint32_t sectors;       // Number of sectors per track (for CHS addressing)
    uint64_t diskOffset;    // Offset where the actual disk data starts
    uint32_t sectorsPerCylinder; // Number of sectors per cylinder
    uint32_t headsPerCylinder;   // Number of heads per cylinder
    uint64_t mapOffset;     // Offset where the block translation map starts
    uint64_t frameOffset;   // Offset where the actual data frames start
    uint32_t frameSize;     // Size of a frame (aka a page/block)
    uint32_t extraFrameSize;// Usually unused
    uint32_t totalFrames;   // Total number of frames/pages
    uint32_t framesAllocated;// Frames that have been allocated (for dynamic disks)
    char uuid[16];          // Unique ID for this image
    char lastSnapUuid[16];  // UUID of last snapshot (if any)
    char linkUuid[16];      // UUID of linked image (for differencing disks)
    char parentUuid[16];    // UUID of parent image (for differencing disks)
    char comment[256];      // Optional comment or description
} VDIHeader;

// --- VDIFile struct holds an open VDI file and necessary metadata ---
typedef struct {
    int fd;                 // File descriptor of the open VDI
    uint8_t header[400];     // Buffer to hold the first 400 bytes (header)
    uint32_t *map;           // Block translation map (logical block to physical)
    size_t cursor;           // Current logical position (for read/write operations)
    uint32_t pageSize;       // Size of each page (frame)
    uint32_t totalPages;     // Number of total pages/frames
} VDIFile;

// --- Function declarations for operations on VDI files ---
VDIFile *vdiOpen(const char *filename);    // Open a VDI file
void vdiClose(VDIFile *vdi);                // Close a VDI file
ssize_t vdiRead(VDIFile *vdi, void *buf, size_t count);    // Read bytes from VDI
ssize_t vdiWrite(VDIFile *vdi, void *buf, size_t count);   // Write bytes to VDI
off_t vdiSeek(VDIFile *vdi, off_t offset, int anchor);     // Seek inside the VDI
off_t vdiTranslate(VDIFile *vdi, off_t logicalOffset);     // Translate logical offset to physical offset
void displayVDIHeader(VDIFile *vdi);       // Display the parsed VDI header
void displayVDITranslationMap(VDIFile *vdi);// Display translation map
void displayMBR(VDIFile *vdi);              // Display Master Boot Record
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);  // Display the buffer

#endif
// Include necessary header files
#include "ext2.h"     // Custom header for ext2 file system operations
#include <stdio.h>    // Standard I/O functions
#include <stdint.h>   // Standard integer types like uint8_t, uint32_t
#include <ctype.h>    // Functions like isprint() to check printable characters

// Function prototypes for displaying buffer contents
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);

// Main function: entry point of the program
int main() {
    // Open the ext2 filesystem stored in a VDI file
    struct Ext2File *ext2 = openExt2("./good-dynamic-1k.vdi");
    if (!ext2) {  // Check if opening failed
        return 1; // Return error code
    }

    // Print the contents of the superblock
    printf("Superblock:\n");
    displaySuperblock(&ext2->superblock);

    // Inform about the next operation
    printf("\nReading first 1KB block from the file system:\n");

    // Prepare a 1KB buffer
    uint8_t buffer[1024];

    // Read block 0 into buffer
    if (fetchBlock(ext2, 0, buffer)) {
        // If successful, display buffer content
        displayBuffer(buffer, 1024, 0);
    } else {
        // If failed, print error
        printf("Failed to read block.\n");
    }

    // Close the ext2 filesystem and clean up
    closeExt2(ext2);

    return 0; // Program ended successfully
}

// Function to display a "page" (up to 256 bytes) of buffer content nicely formatted
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset) {
    count = (count > 256) ? 256 : count;  // Ensure no more than 256 bytes are shown

    // Print offset header
    printf("Offset: 0x%lx\n", offset);
    printf("  00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 0...4...8...c...\n");
    printf("  +------------------------------------------------+   +----------------+\n");

    // Loop over 16 rows (each with 16 bytes)
    for (uint32_t i = 0; i < 16; i++) {
        // Print row offset
        printf("%02x|", (unsigned int)(offset + i * 16));

        // Hexadecimal byte display
        for (uint32_t j = 0; j < 16; j++) {
            size_t pos = i * 16 + j; // Calculate buffer position
            if (pos >= skip && pos < skip + count) {
                printf("%02x ", buf[pos]); // Print byte as hex
            } else {
                printf("   "); // Print empty spaces for skipped bytes
            }
        }

        // Print mirrored offset again
        printf("|%02x|", (unsigned int)(offset + i * 16));

        // ASCII character display
        for (uint32_t j = 0; j < 16; j++) {
            size_t pos = i * 16 + j;
            if (pos >= skip && pos < skip + count) {
                // If printable, print the character, else print '.'
                printf("%c", isprint(buf[pos]) ? buf[pos] : '.');
            } else {
                printf(" "); // Empty space for skipped bytes
            }
        }
        printf("|\n"); // End of the row
    }

    // Print footer
    printf("  +------------------------------------------------+   +----------------+\n\n");
}

// Function to display an entire buffer in 256-byte chunks
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset) {
    // Loop over the buffer in steps of 256 bytes
    for (uint32_t i = 0; i < count; i += 256) {
        uint32_t chunk_size = (count - i > 256) ? 256 : count - i; // Calculate actual chunk size
        displayBufferPage(buf + i, chunk_size, 0, offset + i);     // Display each 256-byte chunk
    }
}
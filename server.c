#include <stdio.h>
#include "udp.h"
#include "types.h"
#include "mfs.h"

// INODE # math:
//  Given inode #:
//      imap piece # = inode # / 16 (truncated/rounded down)
//      imap piece index = inode # % 16

// GLobals
int sd; // Socekt descriptor of server
struct sockaddr_in addr; // Address of socket for communcation with client
int fd; // File descriptor of open FS image file
struct checkpoint_region cr; // Checkpoint region - in memory
struct inode imap[NUM_INODES]; // Full imap - array of imap pieces

int checkDataBlockForMatchingEntry(int blockLocation, char* name) {
    lseek(fd, blockLocation, SEEK_SET);

    for (int i = 0; i < (MFS_BLOCK_SIZE); i+= sizeof(MFS_DirEnt_t)) {
        MFS_DirEnt_t entry;
        
        read(fd, &entry, sizeof(MFS_DirEnt_t));

        if (strcmp(entry.name, name) == 0) {
            return entry.inum;
        }
    }

    return -1;
}

/**
 * Takes the parent inode number (which should be the inode number of a directory) 
 * and looks up the entry name in it. 
 * The inode number of name is returned. 
 * Success: return inode number of name; failure: return -1. 
 * Failure modes: invalid pinum, name does not exist in pinum.
 */
void fs_lookup(int pinum, char* name) { // Nate
    struct inode node = imap[pinum];

    int inodeNumber = -1;

    for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
        if (node.pointers[i] != -1) {
            int num = checkDataBlockForMatchingEntry(node.pointers[i], name);
            if (num > -1) {
                inodeNumber = num;
                break;
            }
        }
    }

    // res struct and use response code to send back inodeNumber.
    struct response res;
    res.rc = inodeNumber;

    // write response
    UDP_Write(sd, &addr, (char *) &res, RESP_SIZE);

    return;
}

/**
 * Returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int fs_stat(int inum) {
    // Stat struct to be returned in respose
    MFS_Stat_t m;

    // Check for valid inum
    int rc = -1;
    if  (imap[inum].size > -1) {
        // Populate stat struct from inode
        m.size = imap[inum].size;
        m.type = imap[inum].type;
        rc = 0;
    }

    // Construct response to client
    struct response resp;
    resp.rc = rc;
    resp.m = m;

    // Send response
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);

    return rc;
}

/**
* Sends failure response to client TODO
*/
void sendFailedResponse() {
    struct response res;
    res.rc = -1;
    UDP_Write(fd, &addr, (char *) &res, sizeof(RESP_SIZE));
    return;
}

/**
 * Writes a block of size 4096 bytes at the block offset specified by block. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: invalid inum, invalid block, not a regular file (because you can't write to directories).
 */

void fs_write(int inum, char* buffer, int block) {
    if (imap[inum].size < 0 || imap[inum].type != MFS_REGULAR_FILE || block < 0 || block >= NUM_POINTERS_PER_INODE) {
        sendFailedResponse();
        return;
    }

    // get imap piece
    int impaPieceIndex = inum / 16;
    int imapPieceLocation = cr.imap_piece_ptrs[impaPieceIndex];
    lseek(fd, imapPieceLocation, SEEK_SET);
    struct imap_piece imapPiece;
    int bytesRead = read(fd, &imapPiece, sizeof(struct imap_piece));
    if (bytesRead == -1) sendFailedResponse();

    // write block to end of FS
    int newBlockLocation = lseek(fd, cr.log_end_ptr, SEEK_SET);
    int bytesWritten = write(fd, buffer, BUFFER_SIZE);
    if (bytesWritten < 0) sendFailedResponse();

    // update inode in memory imap
    imap[inum].pointers[block] = newBlockLocation;
    int previousLastBlock = imap[inum].size / MFS_BLOCK_SIZE;
    if (previousLastBlock <= block) {
        imap[inum].size = (block + 1) * MFS_BLOCK_SIZE;
    }

    // write indoe with new ptr to block location
    int newInodeLocation = lseek(fd, 0, SEEK_CUR);
    bytesWritten = write(fd, (char *)&(imap[inum]), sizeof(struct inode));
    if (bytesWritten < 0) sendFailedResponse();

    // write new imap piece with new ptr to inode
    int newImapPieceLocation = lseek(fd, 0, SEEK_CUR);
    imapPiece.inode_ptrs[(inum % 16)] = newInodeLocation;
    bytesWritten = write(fd, &imapPiece, sizeof(struct imap_piece));
    if (bytesWritten < 0) sendFailedResponse();

    // update CR in memory with new imap piece location and end ptr
    cr.imap_piece_ptrs[impaPieceIndex] = newImapPieceLocation;
    cr.log_end_ptr = newImapPieceLocation + bytesWritten;

    // write CR
    lseek(fd, 0, SEEK_SET);
    bytesWritten = write(fd, &cr, sizeof(struct checkpoint_region));
    if (bytesWritten < 0) sendFailedResponse();

    // res message
    struct response res;
    res.rc = 0;

    UDP_Write(sd, &addr, (char *) &res, RESP_SIZE);
}


/**
 * Reads a block specified by block into the buffer from file specified by inum. 
 * The routine should work for either a file or directory;
 * directories should return data in the format specified by MFS_DirEnt_t. 
 * Success: 0, failure: -1. 
 * Failure modes: invalid inum, invalid block.
 */
void fs_read(int inum, int block) {
    if (imap[inum].size < 0 || imap[inum].type != MFS_REGULAR_FILE || block < 0 || block >= NUM_POINTERS_PER_INODE) {
        sendFailedResponse();
        return;
    }

    struct inode node = imap[inum];

    // Get data block location
    int blockLocation = node.pointers[block];

    // reposition file offset
    lseek(fd, blockLocation, SEEK_SET);

    // create struct for response
    struct response res;
    
    // read data block into res buffer
    int bytesRead = read(fd, (char *)&(res.buffer), BUFFER_SIZE);
    if (bytesRead == -1) sendFailedResponse();

    // set response code
    res.rc = 0;
    
    // write back response
    UDP_Write(sd, &addr, (char *)&res, RESP_SIZE);

    return;
}

/**
 * Makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY) 
 * in the parent directory specified by pinum of name name. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success (think about why).
 */
int fs_create(int pinum, int type, char* name) {
    // Name length checked in MFS_Creat
    // Check if pinum exists
    int piece_num = pinum / NUM_INODES_PER_PIECE;
    int idx = pinum % NUM_INODES_PER_PIECE;

    int rc = -1;
    struct response resp;
    if (imap[pinum].size != -1 && imap[pinum].type == MFS_DIRECTORY) {
        // Pinum exists
        // Get p imap piece and p inode
        struct imap_piece ppiece;
        lseek(fd, cr.imap_piece_ptrs[piece_num], SEEK_SET);
        read(fd, (char *)&ppiece, sizeof(ppiece));
        struct inode pnode = imap[pinum];

        // Search data blocks for name existing
        for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
            // Loop through this data block
            if (pnode.pointers[i] != -1) {
                lseek(fd, pnode.pointers[i], SEEK_SET); // Seek to the data block
                MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                read(fd, (char *)&block, sizeof(block));

                // Loop through entries
                for (int j = 0; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
                    if (block[j].inum != -1) {
                        if (strcmp(block[j].name, name) == 0) {
                            rc = 0;
                            break;
                        }
                    }
                }
            }

            if (rc != -1)
                break;
        }

        // Send response if already success (name found)
        if (rc != -1) {
            resp.rc = rc;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return rc;
        }

        // Find free inode number
        int inode_num = -1;
        for (int i = 0; i < NUM_INODES; i++) {
            if (imap[i].size == -1) {
                // Free inode, save num
                inode_num = i;
                break;
            }
        }

        if (inode_num == -1) {
            // Return failure, no avaiable inode numbers
            resp.rc = -1;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return -1;
        }

        // Determine type (must create new file)
        int dir_entry_written = 0;
        if (type == MFS_REGULAR_FILE) {
            // Create regular file

            // Create new inode
            struct inode new_node;
            new_node.size = 0;
            new_node.type = type;
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                new_node.pointers[i] = -1;
            }
            
            // Write the new inode
            lseek(fd, cr.log_end_ptr, SEEK_SET);
            int new_node_ptr = lseek(fd, 0, SEEK_CUR);
            write(fd, (char *)&new_node, sizeof(new_node));

            // Create new version of imap piece
            int new_piece_ptr = lseek(fd, 0, SEEK_CUR);
            int new_piece_num = inode_num / NUM_INODES_PER_PIECE;
            int new_piece_idx = inode_num % NUM_INODES_PER_PIECE;
            // Read in old piece to new piece and update pointer
            struct imap_piece new_piece;
            lseek(fd, cr.imap_piece_ptrs[new_piece_num], SEEK_SET);
            read(fd, (char *)&new_piece, sizeof(new_piece));
            new_piece.inode_ptrs[new_piece_idx] = new_node_ptr; // Problem where this is already set TODO? I don't think so, if we implement other things correctly
            
            // Write new imap piece
            write(fd, (char *)&new_piece, sizeof(new_piece));

            // Save to CR and in memory imap
            cr.imap_piece_ptrs[new_piece_num] = new_piece_ptr;
            imap[inode_num] = new_node;
            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

            // Update p data block, p inode and p imap piece (if necessary) 
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                int data_ptr = pnode.pointers[i];
                if (data_ptr != -1 && dir_entry_written == 0) { // TODO Logic of this if?
                    lseek(fd, data_ptr, SEEK_SET); // Seek to data block
                    MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                    read(fd, (char *)&block, sizeof(block));
                    
                    // Loop through data block (Should the above check for existence of name be moved here? TODO)
                    for (int j = 0; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
                        if (block[j].inum == -1) {
                            // Insert directory entry
                            block[j].inum = inode_num;
                            strcpy(block[j].name, name);

                            lseek(fd, -sizeof(block[j]), SEEK_CUR); // Seek back to beginning of this entry
                            // Write the updated data block
                            data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                            write(fd, (char *)&block, sizeof(block));
                            dir_entry_written = 1;

                            // Update and write pnode
                            pnode.pointers[i] = data_ptr;
                            // pnode.size += sizeof(MFS_DirEnt_t); // TODO Don't update size because writting to an already allocated block?
                            imap[pinum] = pnode;
                            int pnode_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (char *)&pnode, sizeof(pnode));

                            // Update and write ppiece
                            ppiece.inode_ptrs[idx] = pnode_ptr;
                            int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (char *)&ppiece, sizeof(ppiece));

                            // Save to CR
                            cr.imap_piece_ptrs[piece_num] = ppiece_ptr;
                            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

                            break;
                        }
                    }
                    
                    if (dir_entry_written) {
                        rc = 0;
                        break;
                    }
                }
                else if (data_ptr == -1 && dir_entry_written != 1) {
                    // New block needs to be written to/allocated
                    data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to end of log

                    // Write/allocate data block
                    // Write first entry in this data block
                    MFS_DirEnt_t new_block[NUM_DIR_ENTRIES_PER_BLOCK];
                    new_block[0].inum = inode_num;
                    strcpy(new_block[0].name, name);
                    // Write remaining, empty entries
                    for (int j = 1; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
                        new_block[j].inum = -1;
                    }
                    write(fd, (char *)&new_block, sizeof(new_block));

                    // Update and write pinode
                    pnode.pointers[i] = data_ptr;
                    pnode.size += MFS_BLOCK_SIZE; // TODO Add whole new block's worth of size?
                    imap[pinum] = pnode;
                    int new_pinode_ptr = lseek(fd, 0, SEEK_CUR);
                    write(fd, (char *)&pnode, sizeof(pnode));

                    // Update and write new imap piece version
                    int new_ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                    ppiece.inode_ptrs[idx] = new_pinode_ptr;
                    write(fd, (char *)&ppiece, sizeof(ppiece));

                    // Save to CR
                    cr.imap_piece_ptrs[piece_num] = new_ppiece_ptr;
                    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
                }
            }

            rc = 0;

        } else if (type == MFS_DIRECTORY) {
            // Create directory

            // Create new data block for directory
            lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to log end

            // Write . and .. entries
            MFS_DirEnt_t new_dir_block[NUM_DIR_ENTRIES_PER_BLOCK];
            new_dir_block[0].inum = inode_num;
            strcpy(new_dir_block[0].name, ".");
            new_dir_block[1].inum = pinum;
            strcpy(new_dir_block[1].name, "..");
            // Write remaining empty entries
            for (int i = 2; i < NUM_DIR_ENTRIES_PER_BLOCK; i++) {
                new_dir_block[i].inum = -1;
            }
            int new_dir_block_ptr = lseek(fd, 0, SEEK_CUR);
            write(fd, (char *)&new_dir_block, sizeof(new_dir_block));

            // Create new inode
            struct inode new_node;
            new_node.size = MFS_BLOCK_SIZE; // TODO only one block written to
            new_node.type = type;
            new_node.pointers[0] = new_dir_block_ptr;
            for (int i = 1; i < NUM_POINTERS_PER_INODE; i++) {
                new_node.pointers[i] = -1;
            }
                        
            // Write the new inode
            imap[inode_num] = new_node;
            int new_node_ptr = lseek(fd, 0, SEEK_CUR);
            write(fd, (char *)&new_node, sizeof(new_node));

            // Create new version imap piece
            int new_piece_ptr = lseek(fd, 0, SEEK_CUR);
            int new_piece_num = inode_num / NUM_INODES_PER_PIECE;
            int new_piece_idx = inode_num % NUM_INODES_PER_PIECE;
            struct imap_piece new_piece;
            lseek(fd, cr.imap_piece_ptrs[piece_num], SEEK_SET);
            read(fd, (char *)&new_piece, sizeof(new_piece));
            new_piece.inode_ptrs[new_piece_idx] = new_node_ptr;
            

            // Write new imap piece
            write(fd, (char *)&new_piece, sizeof(new_piece));

            // Save to CR
            cr.imap_piece_ptrs[new_piece_num] = new_piece_ptr;
            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

            // Update p data block, p inode and p imap piece (if necessary) 
            for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
                int dir_entry_written = 0;
                int data_ptr = pnode.pointers[i];
                if (data_ptr != -1) {
                    lseek(fd, data_ptr, SEEK_SET); // Seek to data block
                    MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                    read(fd, (char *)&block, sizeof(block));
                    
                    // Loop through data block (Should the above check for existence of name be moved here? TODO)
                    for (int j = 0; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
                        if (block[j].inum == -1) {
                            // Insert directory entry
                            block[j].inum = inode_num;
                            strcpy(block[j].name, name);

                            lseek(fd, -sizeof(block[j]), SEEK_CUR); // Seek back to beginning of this entry
                            // Write the updated data block
                            data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                            write(fd, (char *)&block, sizeof(block));
                            dir_entry_written = 1;

                            // Update and write pnode
                            pnode.pointers[i] = data_ptr;
                            // pnode.size += sizeof(MFS_DirEnt_t); // TODO no update to size in this case?
                            imap[pinum] = pnode;
                            int pnode_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (char *)&pnode, sizeof(pnode));

                            // Update and write ppiece
                            ppiece.inode_ptrs[idx] = pnode_ptr;
                            int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (char *)&ppiece, sizeof(ppiece));

                            // Save to CR
                            cr.imap_piece_ptrs[piece_num] = ppiece_ptr;
                            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

                            break;
                        }
                    }
                    
                    if (dir_entry_written) {
                        rc = 0;
                        break;
                    }
                }
                else if (data_ptr == -1 && dir_entry_written != 1) {
                    // New block needs to be written to/allocated
                    data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to end of log

                    // Write/allocate data block
                    // Write first entry in this data block
                    MFS_DirEnt_t new_block[NUM_DIR_ENTRIES_PER_BLOCK];
                    new_block[0].inum = inode_num;
                    strcpy(new_block[0].name, name);
                    // Write remaining, empty entries
                    for (int j = 1; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
                        new_block[j].inum = -1;
                    }
                    write(fd, (char *)&new_block, sizeof(new_block));

                    // Update and write pinode
                    pnode.pointers[i] = data_ptr;
                    pnode.size += MFS_BLOCK_SIZE; // TODO full new block?
                    imap[pinum] = pnode;
                    int new_pinode_ptr = lseek(fd, 0, SEEK_CUR);
                    write(fd, (char *)&pnode, sizeof(pnode));

                    // Update and write new imap piece version
                    int new_ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                    ppiece.inode_ptrs[idx] = new_pinode_ptr;
                    write(fd, (char *)&ppiece, sizeof(ppiece));

                    // Save to CR
                    cr.imap_piece_ptrs[piece_num] = new_ppiece_ptr;
                    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

                    rc = 0;
                }
            }
        }
        if (dir_entry_written == 0) { // TODO ?
            sendFailedResponse();
        }

        // Write CR
        if (rc == 0) {
            lseek(fd, 0, SEEK_SET);
            write(fd, (char *)&cr, sizeof(cr));
        }
    }

    fsync(fd);
    resp.rc = rc;
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
    return rc;
}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int fs_unlink(int pinum, char* name) {
    // Check if pinum exists
    int ppiece_num = pinum / NUM_INODES_PER_PIECE;
    int idx = pinum % NUM_INODES_PER_PIECE;

    int rc = -1;
    struct response resp;
    if  (imap[pinum].size != -1) {
        // Pinum exists
        // Get p imap piece
        struct imap_piece ppiece;
        lseek(fd, cr.imap_piece_ptrs[ppiece_num], SEEK_SET);
        read(fd, (char *)&ppiece, sizeof(ppiece));

        // Get pinode
        lseek(fd, ppiece.inode_ptrs[idx], SEEK_SET);
        struct inode pnode;
        read(fd, (char *)&pnode, sizeof(pnode));

        // Search data blocks for name existing
        rc = 0;
        for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
            // Loop through this data block
            if (pnode.pointers[i] != -1) {
                lseek(fd, pnode.pointers[i], SEEK_SET); // Seek to the data block
                // Read directory data block
                MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                read(fd, (char *)&block, sizeof(block));

                // Loop through dir entries in data block
                int valid_entries = 0;
                for (int j = 0; j < (NUM_DIR_ENTRIES_PER_BLOCK); j++) {
                    if (block[j].inum != -1) {
                        valid_entries += 1;
                        // Find name
                        if (strcmp(block[j].name, name) == 0) {
                            // Get inode to determine type
                            int cur_num = block[j].inum;
                            struct inode cur_node = imap[cur_num];
                            
                            // Regular File Case
                            if (cur_node.type == MFS_REGULAR_FILE) {
                                // Just set the inum to be -1
                                block[j].inum = -1;
                            }
                            // Directory Case
                            else {
                                // Check if directory NOT empty
                                if (cur_node.size > MFS_BLOCK_SIZE) {
                                    sendFailedResponse();
                                    return -1;
                                }
                                lseek(fd, cur_node.pointers[0], SEEK_SET); // Seek to the data block (only need to check first block)
                                // Read directory data block
                                MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                                read(fd, (char *)&block, sizeof(block));
                                for (int k = 0; k < NUM_DIR_ENTRIES_PER_BLOCK; k++) {
                                    if (block[k].inum != -1) { 
                                        if (!(strcmp(block[k].name, ".") == 0 || strcmp(block[k].name, "..") == 0)) {
                                            // Non-. and .. entry exists, so not full
                                            sendFailedResponse();
                                            return -1;
                                        }
                                    }
                                }

                                // Empty directory, remove just like a regular file
                                // Just set the inum to be -1
                                block[j].inum = -1;
                            }

                            if (block[j].inum == -1) {
                                // This inum/entry is the one to be deallocated
                                // Write data block
                                int data_ptr;
                                if (valid_entries > 1) {
                                    // Do not deallocated data block
                                    data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                                    write(fd, (char *)&block, sizeof(block));
                                } 
                                else {
                                    // Deallocate this block
                                    data_ptr = -1;
                                    lseek(fd, cr.log_end_ptr, SEEK_SET);
                                    pnode.size -= MFS_BLOCK_SIZE;
                                }

                                // Update and write pinode
                                int pinode_ptr = lseek(fd, 0, SEEK_CUR);
                                pnode.pointers[i] = data_ptr;
                                imap[pinum] = pnode;
                                write(fd, (char *)&pnode, sizeof(pnode));

                                // Update and write new imap piece
                                int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                                ppiece.inode_ptrs[idx] = pinode_ptr;
                                write(fd, (char *)&ppiece, sizeof(ppiece));

                                // Update in memory imap and CR
                                cr.imap_piece_ptrs[ppiece_num] = ppiece_ptr;
                                cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
                                rc = 1;
                            }
                            
                            // Deallocate inode
                            // Update in memeory piece and CR then write piece
                            imap[cur_num].size = -1;
                            struct imap_piece cur_piece;
                            lseek(fd, cr.imap_piece_ptrs[cur_num / NUM_INODES_PER_PIECE], SEEK_SET);
                            read(fd, (char *)&cur_piece, sizeof(cur_piece));
                            cur_piece.inode_ptrs[cur_num % NUM_INODES_PER_PIECE] = -1;
                            int cur_piece_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                            write(fd, (char *)&cur_piece, sizeof(cur_piece));
                            cr.imap_piece_ptrs[cur_num / NUM_INODES_PER_PIECE] = cur_piece_ptr;
                            cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
                        }
                    }
                }
            }

            if (rc != 0) {
                if (rc == 1) {
                    rc = 0; // Hacky logic TODO
                }
                break;
            }
        }

        // Write CR
        lseek(fd, 0, SEEK_SET);
        write(fd, (char *)&cr, sizeof(cr));
    }

    fsync(fd);
    resp.rc = rc;
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
    return rc;
}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). 
 * This interface will mostly be used for testing purposes.
 * Returns 0 on success and -1 on failure.
 */
int fs_shutdown() {
    fsync(fd);
    close(fd);
    UDP_Close(sd);
    exit(0);
}

void initNewFS() {
    // Create checkpoint region
    write(fd, (char *)&cr, sizeof(cr)); // Write empty CR

    // Create and write initial imap pieces
    for (int i = 0; i < NUM_IMAP_PIECES; i++) {
        // Create empty pieces and fill in memory imap
        struct imap_piece piece;
        for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
            piece.inode_ptrs[j] = -1;
        }
        int piece_ptr = lseek(fd, 0, SEEK_CUR);
        write(fd, (char *)&piece, sizeof(piece));
        cr.imap_piece_ptrs[i] = piece_ptr;
    }

    // Write out initial, empty CR with valid imap pointers (and pieces) and save log end pointer
    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    write(fd, (char *)&cr, sizeof(cr));

    // Add root directory
    MFS_DirEnt_t root_block[NUM_DIR_ENTRIES_PER_BLOCK];
    // Write . and .. entries to the data block and then the remaining empty entries
    strcpy(root_block[0].name, ".");
    root_block[0].inum = 0;
    strcpy(root_block[1].name, "..");
    root_block[1].inum = 0;
    int root_data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET); // Seek to end of imap pieces via log end pointer
    for (int j = 2; j < NUM_DIR_ENTRIES_PER_BLOCK; j++) {
        root_block[j].inum = -1;
    }
    write(fd, (char *)&root_block, sizeof(root_block));        

    // Create root directory inode
    struct inode root_dir;
    root_dir.type = MFS_DIRECTORY;
    root_dir.pointers[0] = root_data_ptr;
    for (int j = 1; j < NUM_POINTERS_PER_INODE; j++) {
        root_dir.pointers[j] = -1;
    }
    int root_dir_inode_ptr = lseek(fd, 0, SEEK_CUR);
    root_dir.size = 2*sizeof(MFS_DirEnt_t); // TODO
    for (int j = 1; j < NUM_POINTERS_PER_INODE; j++) {
        root_dir.pointers[j] = -1;
    }
    write(fd, (char *)&root_dir, sizeof(root_dir)); // Write inode
    
    // Update the imap piece
    struct imap_piece root_piece;
    root_piece.inode_ptrs[0] = root_dir_inode_ptr;
    for (int j = 1; j < NUM_INODES_PER_PIECE; j++) {
        root_piece.inode_ptrs[j] = -1;
    }
    int root_imap_piece_ptr = lseek(fd, 0, SEEK_CUR);
    write(fd, (char *)&root_piece, sizeof(root_piece)); // Write the imap piece

    // Update cr
    cr.imap_piece_ptrs[0] = root_imap_piece_ptr;
    cr.log_end_ptr = lseek(fd, 0, SEEK_CUR);

    // Initialize in memory imap
    imap[0] = root_dir;
    for (int j = 1; j < NUM_INODES; j++) {
        imap[j].size = -1;
    }

    // Write CR
    lseek(fd, 0, SEEK_SET);
    write(fd, (char *)&cr, sizeof(cr));
    
    fsync(fd); // Force to disk
}

void loadFS() {
    // Read in checkpoint region
    lseek(fd, 0, SEEK_SET);
    read(fd, (char *)&cr, sizeof(struct checkpoint_region));
    
    // Loop through imap pieces and set up in memory imap
    for (int i = 0; i < NUM_IMAP_PIECES; i++) {
        // Read the piece
        struct imap_piece piece;
        lseek(fd, cr.imap_piece_ptrs[i], SEEK_SET);
        read(fd, (char *)&piece, sizeof(struct imap_piece));
        for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
            struct inode node;
            lseek(fd, piece.inode_ptrs[j], SEEK_SET);
            read(fd, (char *)&node, sizeof(struct inode));

            int inode_num = i * NUM_INODES_PER_PIECE + j;
            imap[inode_num] = node;
            memset(&node, 0, sizeof(struct inode));
        }
        memset(&piece, 0, sizeof(struct imap_piece));
    }
}

// Run the server
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: prompt> server [portnum] [file-system-image]\n");
        exit(0);
    }

    // File system image file does not exist
    if (access(argv[2], F_OK) == 0) {
        fd = open(argv[2], O_RDWR); // Open image file
        loadFS();
    } else {
        fd = open(argv[2], O_RDWR | O_CREAT); // Open and create image file
        initNewFS();
    }

    sd = UDP_Open(atoi(argv[1]));
    assert (sd > -1);

    while (1) {
        struct request req;
        int bytesRead = UDP_Read(sd, &addr, (char *) &req, REQ_SIZE);
        
        switch (req.type) {
            case LOOKUP:
                fs_lookup(req.pinum, req.name);
                break;
            case STAT:
                fs_stat(req.inum);
                break;
            case WRITE:
                fs_write(req.inum, req.buffer, req.block);
                break;
            case READ:
                fs_read(req.inum, req.block);
                break;
            case CREAT:
                fs_create(req.pinum, req.file_type, req.name);
                break;
            case UNLINK:
                fs_unlink(req.pinum, req.name);
                break;
            case SHUTDOWN:
                fs_shutdown();
                break;
            default:
                break;
        }
    }

    return 0;
}
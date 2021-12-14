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

int fs_lookup(int pinum, char* name) {
    // Nate
    return 0;
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

int fs_write(int inum, char* buffer, int block) {
    // Nate
    return 0;
}

int fs_read(int inum, int block) {
    // Nate
    return 0;
}

/**
 * Makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY) 
 * in the parent directory specified by pinum of name name. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success (think about why).
 */
int fs_create(int pinum, int type, char* name) {
    // TODO USE NUM_DIR_ENTRIES_PER_BLOCK and new struct where possible
    // Name length checked in MFS_Creat
    // Check if pinum exists
    int piece_num = pinum / NUM_INODES_PER_PIECE;
    int idx = pinum / NUM_INODES_PER_PIECE;

    int rc = -1;
    struct response resp;
    if  (imap[pinum].size != -1) {
        // Pinum exists
        // Get p imap piece and p inode
        struct imap_piece ppiece = cr.imap_piece_ptrs[piece_num];
        struct inode pnode = imap[pinum];

        // Search data blocks for name existing
        for (int i = 0; i < NUM_POINTERS_PER_INODE; i++) {
            // Loop through this data block
            if (pnode.pointers[i] != -1) {
                lseek(fd, pnode.pointers[i], SEEK_SET); // Seek to the data block
                MFS_DirEnt_t block[NUM_DIR_ENTRIES_PER_BLOCK];
                read(fd, (char *)&block, sizeof(block));

                // Loop through entries
                for (int j = 0; j < NUM_ENTRIES_PER_BLOCK; j++) {
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

        // Send response if no more entries availble in parent
        if (pnode.size == MAX_FILE_SIZE) { 
            resp.rc = -1;
            UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
            return resp.rc;
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
                            pnode.size += sizeof(MFS_DirEnt_t); // TODO
                            imap[pinum] = pnode;
                            int pnode_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (Char *)&pnode, sizeof(pnode));

                            // Update and write ppiece
                            ppiece.inode_ptrs[idx] = pnode_ptr;
                            int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd (char *)&ppiece, sizeof(ppiece));

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
                    pnode.size += sizeof(MFS_DirEnt_t); // TODO Or sizeof whole data block
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
            new_node.size = 2 * sizeof(MFS_DirEnt_t);
            new_node.type = type;
            new_node.pointers[0] = data_block_ptr;
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
                            pnode.size += sizeof(MFS_DirEnt_t); // TODO
                            imap[pinum] = pnode;
                            int pnode_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd, (Char *)&pnode, sizeof(pnode));

                            // Update and write ppiece
                            ppiece.inode_ptrs[idx] = pnode_ptr;
                            int ppiece_ptr = lseek(fd, 0, SEEK_CUR);
                            write(fd (char *)&ppiece, sizeof(ppiece));

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
                    pnode.size += sizeof(MFS_DirEnt_t); // TODO Or sizeof whole data block
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

                    rc = 0
                }
            }
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
    if  imap[pinum].size != -1) {
        // Pinum exists
        // Get p imap piece and pinode
        struct imap_piece ppiece = cr.imap_piece_ptrs[ppiece_num];
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
                for (int j = 0; j < (NUM_DIR_ENTRIES_PER_BLOCK); j++) {
                    if (block[j].inum != -1) {
                        // Find name
                        if (strcmp(block[j].name, name) == 0) {
                            // Get inode to determine type
                            struct inode cur_node;
                            lseek(fd, imap[block[j].inum / NUM_INODES_PER_PIECE].inode_ptrs[block[j].inum % NUM_INODES_PER_PIECE], SEEK_SET); 
                            read(fd, (char *)&cur_node, sizeof(cur_node));

                            // Regular File Case
                            if (cur_node.type == MFS_REGULAR_FILE) {
                                // Just set the inum to be -1
                                block[j].inum = -1;

                                // Write data block
                                int data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                                write(fd, (char *)&block, sizeof(block));

                                // Update and write pinode
                                int pinode_ptr = lseek(fd, 0, SEEK_CUR);
                                pnode.pointers[i] = data_ptr;
                                pnode.size -= sizeof(MFS_DirEnt_t); // TODO HOW ARE WE TRACKING DIR SIZE
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
                            // Directory Case
                            else {
                                // Check if directory NOT empty
                                if (cur_node.size > 2*sizeof(MFS_DirEnt_t)) {
                                    rc = -1;
                                    break;
                                }

                                // Empty directory, remove just like a regular file
                                // Just set the inum to be -1
                                block[j].inum = -1;

                                // Write data block
                                int data_ptr = lseek(fd, cr.log_end_ptr, SEEK_SET);
                                write(fd, (char *)&block, sizeof(block));

                                // Update and write pinode
                                int pinode_ptr = lseek(fd, 0, SEEK_CUR);
                                pnode.pointers[i] = data_ptr;
                                pnode.size -= sizeof(MFS_DirEnt_t); // TODO HOW ARE WE TRACKING DIR SIZE
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
                            imap[block[j].inum].size = -1;
                            struct imap_piece cur_piece;
                            lseek(fd, cr.imap_piece_ptrs[block[j].inum / NUM_INODES_PER_PIECE], SEEK_SET);
                            read(fd, (char *)&cur_piece, sizeof(cur_piece));
                            cur_piece.inode_ptrs[block[j].inum % NUM_INODES_PER_PIECE] = -1;
                            write(fd, (char *)&cur_piece, sizeof(cur_piece));
                            cr.imap_piece_ptrs[block[j].inum / NUM_INODES_PER_PIECE] = cr.log_end_ptr;
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
        write(fd (char *)&cr, sizeof(cr));
    }

    fsync(fd);
    resp.rc = rc;
    UDP_Write(sd, &addr, (char *)&resp, RESP_SIZE);
    return rc;
}

int fs_shutdown() {
    // Nate
    return 0;
}

// Run the server
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: prompt> server [portnum] [file-system-image]\n");
        exit(0);
    }

    // Initialize / Setup FS

    // File system image file does not exist
    if (access(argv[2], F_OK) == 0) {
        fd = open(argv[2], O_RDWR); // Open image file

        // Read in checkpoint region
        lseek(fd, 0, SEEK_SET);
        read(fd, (char *)&cr, sizeof(struct checkpoint_region));
        
        // Loop through imap pieces and set up in memory imap
        for (int i = 0; i < NUM_IMAP_PIECES; i++;) {
            // Read the piece
            struct imap_piece piece;
            lseek(fd, cr.imap_piece_ptrs[i], SEEK_SET);
            read(fd, (char *)&piece, sizeof(piece));
            for (int j = 0; j < NUM_INODES_PER_PIECE; j++) {
                struct inode node;
                lseek(fd, piece[j], SEEK_SET);
                read(fd, (char *)&inode, sizeof(inode));

                int inode_num = i * NUM_INODES_PER_PIECE + j;
                imap[inode_num] = node;
            }
        }

        // Force to disk
        fsync(fd);
    } else {
        fd = open(argv[2], O_RDWR | O_CREAT); // Open and create image file

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

    sd = UDP_Open(atoi(argv[1]));
    assert (sd > -1);

    while (1) {
        struct request req;
        int rc = UDP_Read(sd, &addr, (char *) &req, REQ_SIZE);

        if (rc > 0) {
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
                    fs_create(req.pinum, req.type, req.name);
                    break;
                case UNLINK:
                    fs_unlink(req.pinum, req.name);
                    break;
                case SHUTDOWN:
                    rc = fs_shutdown();
                    break;
                default:
                    break;
            }
        }

        if (rc == -1)
            return rc;
    }

    return 0;
}
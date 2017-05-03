#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ftree.h"
#include "hash.h"

#define MAX_BACKLOG 5

void recurse(char *src_path, char *dest_path, int fd);

int check_hash(const char *hash1, const char *hash2) {
    for (long i = 0; i < HASH_SIZE; i++) {
        if (hash1[i] != hash2[i]) {
            return 1;
        }
    }
    return 0;
}

int fcopy_client(char *src_path, char *dest_path, char *host, int port){	
    // create socket 
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd == -1) {
		perror("client: socket");
        exit(1);
	}

	// Set the IP and port of the server to connect to
	struct sockaddr_in server;
	server.sin_family = AF_INET;
    server.sin_port = htons(port); 

    if (inet_pton(AF_INET, host, &server.sin_addr) < 1) {
        perror("client: inet_pton");
        close(sock_fd);
        exit(1);
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("client: connect");
        close(sock_fd);
        exit(1);
    }
    //printf("Done connecting!\n");

    char *base;
    base = (char *)malloc(strlen(dest_path) + 2 + strlen(src_path));
    strcpy(base, dest_path);
    strcat(base, "/");
    strcat(base, src_path);

    recurse(src_path, base, sock_fd);
    free(base);
    close(sock_fd);
    return(0);
}

void recurse(char *src_path, char *dest_path, int sock_fd){
    struct fileinfo *root;
    struct stat sb;

    DIR *d;
    struct dirent *dir;
    d = opendir(src_path);

    root = malloc(sizeof(struct fileinfo));
    if(lstat(src_path, &sb) == -1) {
        perror("lstat");
        exit(1);
    }

    // populate path field of struct
    char actualpath[MAXPATH];
    char *rootpath;
    rootpath = malloc(strlen(src_path) + 1 + strlen(actualpath));
    rootpath = realpath(src_path, actualpath);
    strcpy(root->path, rootpath);

    // populate mode field of struct
    root->mode = sb.st_mode;

    // populate size field of struct
    root->size = sb.st_size;

	//printf("%s %zu %o\n", root->path, root->size, root->mode & 0777);

	// Write dest to server
    if(write(sock_fd, dest_path, MAXPATH*sizeof(char)) == -1) {
    	fprintf(stderr, "write: failed at '%s'\n", dest_path);
    	close(sock_fd);
        exit(1);
    }

    // Write struct to server
    if(write(sock_fd, root->path, MAXPATH*sizeof(char)) == -1) {
    	fprintf(stderr, "write: failed at '%s'\n", root->path);
    	close(sock_fd);
        exit(1);
    }

    uint32_t mode = htonl(root->mode);
    if(write(sock_fd, &mode, sizeof(uint32_t)) == -1) {
    	fprintf(stderr, "write: failed at sending mode for '%s'\n", root->path);
    	close(sock_fd);
        exit(1);
    }

    if(write(sock_fd, root->hash, HASH_SIZE*sizeof(char)) == -1) {
    	fprintf(stderr, "write: failed at sending hash for '%s'\n", root->path);
    	close(sock_fd);
        exit(1);
    }
    uint32_t size = htonl(root->size);
    if(write(sock_fd, &size, sizeof(uint32_t)) == -1) {
    	fprintf(stderr, "write: failed at sending size for '%s'\n", root->path);
    	close(sock_fd);
        exit(1);
    }
	
	int msg;
    if(read(sock_fd, &msg, sizeof(int)) == -1) {
    	fprintf(stderr, "read: failed at reading message\n");
    	close(sock_fd);
        exit(1);
    }

    if(msg == MISMATCH) {

    } else if (msg == MATCH) {

    } else if (msg == MATCH_ERROR) {
    	fprintf(stderr, "Match Error: %s\n", root->path);
    	close(sock_fd);
        exit(1);
    }

    while((dir = readdir(d)) != NULL){ // go through directory contents
        if(dir->d_name[0] == '.'){ // skip files/directories starting with .
            continue;
        }
        char *fname; 
        fname = (char *)malloc(strlen(src_path) + 2 + strlen(dir->d_name));

        strcpy(fname, src_path);
        strcat(fname, "/");
        strcat(fname, dir->d_name);

        struct stat lb;
        //printf("%s\n", fname);

        if(lstat(fname, &lb) == -1) { // get information for fname, perform error check
            fprintf(stderr, "lstat: the name '%s' is not a file or directory\n", fname);
            exit(1);
        }
        if(S_ISLNK(lb.st_mode)) {
            continue;
        } else if(S_ISREG(lb.st_mode)) {
            FILE *fff;
            fff = malloc(sizeof(FILE *));
            fff = fopen(fname, "rb"); // error check
            if (fff == NULL) {
                fprintf(stderr, "fopen: '%s': permision denied\n", fname);
                exit(1);
            }

            struct fileinfo *child;
            child = malloc(sizeof(struct fileinfo));

            // populate path field of struct
            char cpath[MAXPATH];
            char *childpath;
            childpath = (char *)malloc(strlen(fname) + 1 + strlen(cpath));
            childpath = realpath(fname, cpath);
            strcpy(child->path, childpath);

            // populate mode field of struct
            child->mode = lb.st_mode;

            // populate size field of struct
            child->size = lb.st_size;

            // populate hash field of struct
            strcpy(child->hash, hash(fff));

			//printf("%s %zu %o\n", child->path, child->size, child->mode & 0777);
			
			char *dest = (char *)malloc(strlen(dest_path) + 2 + strlen(dir->d_name));
            strcpy(dest, dest_path);
    		strcat(dest, "/");
    		strcat(dest, dir->d_name);

            if(write(sock_fd, dest, MAXPATH*sizeof(char)) == -1) {
            	fprintf(stderr, "write: failed at '%s'\n", dest);
            	close(sock_fd);
        		exit(1);
            }

            if(write(sock_fd, child->path, MAXPATH*sizeof(char)) == -1) {
            	fprintf(stderr, "write: failed at writing path'%s'\n", child->path);
            	close(sock_fd);
        		exit(1);
            }

            uint32_t mode2 = htonl(child->mode);
            if(write(sock_fd, &mode2, sizeof(uint32_t)) == -1) {
            	fprintf(stderr, "write: failed at writing mode for '%s'\n", child->path);
            	close(sock_fd);
        		exit(1);
            }

            if(write(sock_fd, child->hash, HASH_SIZE*sizeof(char)) == -1) {
            	fprintf(stderr, "write: failed at writing hash for '%s'\n", child->path);
            	close(sock_fd);
        		exit(1);
            }

            uint32_t size2 = htonl(child->size);
            if(write(sock_fd, &size2, sizeof(uint32_t)) == -1) {
            	fprintf(stderr, "write: failed at writing size for'%s'\n", child->path);
            	close(sock_fd);
        		exit(1);
            }

			int msg;
            if(read(sock_fd, &msg, sizeof(int)) == -1) {
            	fprintf(stderr, "read: failed at getting message\n");
            	close(sock_fd);
        		exit(1);
            }

			if(msg == MISMATCH) {
				//printf("sending copy");
				FILE *ffff;
                //ffff = malloc(sizeof(FILE *));
                ffff = fopen(fname, "rb"); // error check
				int fd = fileno(ffff);
	            if (fd == -1) {
	                fprintf(stderr, "fopen: '%s': permision denied\n", fname);
	                exit(1);
	            }

				char buf[child->size];
				int r;
				while (1) {
					r = read(fd, buf, sizeof(buf));
					if (r == 0)
						break;
					if (r == -1) {
						fprintf(stderr, "read: failed to read '%s'\n", fname);
						exit(1);
					}
					if(write(sock_fd, buf, r) == -1) {
		            	fprintf(stderr, "write: failed at transferring data for '%s'\n", child->path);
		            	close(sock_fd);
    					exit(1);
			        }
				}

				close(fd);
				fclose(ffff);

				int recieved;
			    if(read(sock_fd, &recieved, sizeof(int)) == -1){
			    	fprintf(stderr, "read: failed at getting transmit message");
			    	close(sock_fd);
        			exit(1);
			    }
			    if(recieved == TRANSMIT_OK){
			    	continue;
			    } else if (recieved == TRANSMIT_ERROR){
			    	fprintf(stderr, "transmit error for file '%s'", child->path);
			    	close(sock_fd);
        			exit(1);
			    }

				
            } else if (msg == MATCH) {

            } else if (msg == MATCH_ERROR) {
            	fprintf(stderr, "Match Error: %s\n", child->path);
            	close(sock_fd);
        		exit(1);
            }

        } else if(S_ISDIR(lb.st_mode)) {
            char *dest = (char *)malloc(strlen(dest_path) + 2 + strlen(dir->d_name));
            strcpy(dest, dest_path);
    		strcat(dest, "/");
    		strcat(dest, dir->d_name);

    		char *src = (char *)malloc(strlen(src_path) + 2 + strlen(dir->d_name));
    		strcpy(src, src_path);
    		strcat(src, "/");
    		strcat(src, dir->d_name);

            recurse(src, dest, sock_fd);
        }
    }
}


void fcopy_server(int port){
    int MISMATCH_N = 1;
    int MATCH_N = 2;
    int MATCH_ERROR_N = 3;
    int TRANSMIT_OK_N = 4;
    int TRANSMIT_ERROR_N = 5;
	int on = 1;
	// create socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd == -1){
        perror("server: socket");
        exit(1);
    }

    // Make sure we can reuse the port immediately after the server terminates.
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        perror("setsockopt -- REUSEADDR");
    }

    // Set the IP and port we want to be connected to
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //printf("Listening on port: %d\n", port);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, HASH_SIZE);

    // Bind the selected port to the socket
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("server: bind"); // means port is in use
        close(sock_fd);
        exit(1);
    }

    // Create queue in kernel for new connection requests
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // Accept a new connection, new fd needed to communicate
    // first one is used to accept connections
    socklen_t socklen;
    struct sockaddr_in peer;

    while(1) {
        socklen = sizeof(peer);

        int client_fd = accept(sock_fd, (struct sockaddr *)&peer, &socklen);
        if (client_fd < 0) {
            perror("server: accept");
            //close(sock_fd);
            //exit(1);
        }

		// Recieve messages        

        char dest[MAXPATH];
        while(read(client_fd, dest, MAXPATH*sizeof(char)) > 0) {
        	struct fileinfo *root;
        	root = malloc(sizeof(struct fileinfo));
        	if(read(client_fd, root->path, MAXPATH*sizeof(char)) == -1) {
        		fprintf(stderr, "read: failed at reading '%s'\n", root->path);
        		close(client_fd);
        	}

        	uint32_t mode;
	        if(read(client_fd, &mode, sizeof(uint32_t)) == -1) {
	        	fprintf(stderr, "read: failed at reading mode for '%s'\n", root->path);
	        	close(client_fd);
	        }
	        root->mode = ntohl(mode);

	        if(read(client_fd, root->hash, HASH_SIZE*sizeof(char)) == -1) {
	        	fprintf(stderr, "read: failed at reading hash for '%s'\n", root->path);
	        	close(client_fd);
	        }

	        uint32_t size;
	        if(read(client_fd, &size, sizeof(uint32_t)) == -1) {
	        	fprintf(stderr, "read: failed at reading size for '%s'\n", root->path);
	        	close(client_fd);
	        }
	        root->size = ntohl(size);
	        //printf("%s %u %o\n", root->path, ntohl(size), root->mode & 0777);		

			if(S_ISREG(root->mode)){
		        FILE *f;
		        f = malloc(sizeof(FILE *));
		        f = fopen(dest, "rb");
		        if(!f){ // file does not exist on server
		        	if(write(client_fd, &MISMATCH_N, sizeof(int)) == -1) {
		        		fprintf(stderr, "write: failed at sending mismatch error\n");
		        		close(client_fd);
		        	}

		        	// reads the file's contents over the socket from the client
					int bytes_read = 0;
		        	int r;
		        	char buf;

		        	FILE *ffff;
		        	ffff = malloc(sizeof(FILE *));
		        	ffff = fopen(dest, "wb");
		        	if (ffff == NULL) {
		                if(write(client_fd, &MATCH_ERROR_N, sizeof(int)) == -1) {
	        				fprintf(stderr, "write: failed at sending match error\n");
	        				fclose(ffff);
	        				close(client_fd);
	        			}
		            }

					while(bytes_read < root->size) {
		        		r = read(client_fd, &buf, sizeof(char));

		        		if(r == -1) {
		        			if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
			        			fprintf(stderr, "write: failed at sending transmit ok\n");
			        			fclose(ffff);
			        			close(client_fd);
			        		}
		        		}
		        		if(r != 0) {
		        			if(fwrite(&buf, sizeof(char), 1, ffff) == -1) {
		        				if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
			        				fprintf(stderr, "write: failed at sending transmit ok\n");
			        				fclose(ffff);
			        				close(client_fd);
			        			}
			        		}
			        		bytes_read += r;
		        		}
		        	}

					if (chmod(dest, root->mode) == -1){
				        fprintf(stderr, "chmod: '%s' can't be changed\n", dest);
				        fclose(ffff);
				        close(client_fd);
			    	}

			    	fclose(ffff);

			    	if(write(client_fd, &TRANSMIT_OK_N, sizeof(int)) == -1) {
        				fprintf(stderr, "write: failed at sending transmit ok\n");
        				close(client_fd);
        			}
					

		        } else if (f) { // file exists on server, check if they're the same
		        	struct stat cb;

		            if(lstat(dest, &cb) == -1) { 
		                fprintf(stderr, "lstat: the name '%s' is not a file or directory\n", dest);
		                fclose(f);
		                close(client_fd);
		            }

		            if(cb.st_size == root->size) { // compare server file size to client file size
		            	//printf("SIZES SAME\n");
		                char *hashserver;
		                hashserver = malloc(9);
		                hashserver = hash(f);

		                char str1[9];
		                char str2[9];

		                memcpy(str1, hashserver, 9);
		                memcpy(str2, root->hash, 9);
		                free(hashserver);

		                if(memcmp(str1, str2, 8) != 0) { // if sizes are the same, compare their hashes
		                	//printf("MISMATCH\n");
		                    if(write(client_fd, &MISMATCH_N, sizeof(int)) == -1) {
		                    	fprintf(stderr, "write: failed at sending mismatch error\n");
		                    	fclose(f);
		                    	close(client_fd);
		                    }

		                    int bytes_read = 0;
				        	int r;
				        	char buf;

				        	FILE *ffff;
				        	ffff = malloc(sizeof(FILE *));
				        	ffff = fopen(dest, "wb");
				        	if (ffff == NULL) {
				                if(write(client_fd, &MATCH_ERROR_N, sizeof(int)) == -1) {
			        				fprintf(stderr, "write: failed at sending match error\n");
			        				fclose(ffff);
			        				close(client_fd);
			        			}
				            }

							while(bytes_read < root->size) {
				        		r = read(client_fd, &buf, sizeof(char));

				        		if(r == -1) {
				        			if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
					        			fprintf(stderr, "write: failed at sending transmit ok\n");
					        			fclose(ffff);
					        			close(client_fd);
					        		}
				        		}
				        		if(r != 0) {
				        			if(fwrite(&buf, sizeof(char), 1, ffff) == -1) {
				        				if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
					        				fprintf(stderr, "write: failed at sending transmit ok\n");
					        				fclose(ffff);
					        				close(client_fd);
					        			}
					        		}
					        		bytes_read += r;
				        		}
				        	}

							if (chmod(dest, root->mode) == -1){
						        fprintf(stderr, "chmod: '%s' can't be changed\n", dest);
						        fclose(ffff);
						        close(client_fd);
					    	}

					    	fclose(ffff);

					    	if(write(client_fd, &TRANSMIT_OK_N, sizeof(int)) == -1) {
		        				fprintf(stderr, "write: failed at sending transmit ok\n");
		        				close(client_fd);
		        			}
							

		                } else {
		                    if(write(client_fd, &MATCH_N, sizeof(int)) == -1) {
		                    	fprintf(stderr, "write: failed at sending match\n");
		                    	fclose(f);
		                    	close(client_fd);
		                    }
		                }
		            } else { // if sizes are diff, file does not exist on server
		            	//printf("SIZES DIFF");
		            	if(write(client_fd, &MISMATCH_N, sizeof(int)) == -1) {
		            		fprintf(stderr, "write: failed at sending mismatch error\n");
		            		fclose(f);
		            		close(client_fd);
		            	}

		            	int bytes_read = 0;
			        	int r;
			        	char buf;

			        	FILE *ffff;
			        	ffff = malloc(sizeof(FILE *));
			        	ffff = fopen(dest, "wb");
			        	if (ffff == NULL) {
			                if(write(client_fd, &MATCH_ERROR_N, sizeof(int)) == -1) {
		        				fprintf(stderr, "write: failed at sending match error\n");
		        				fclose(ffff);
		        				close(client_fd);
		        			}
			            }

						while(bytes_read < root->size) {
			        		r = read(client_fd, &buf, sizeof(char));

			        		if(r == -1) {
			        			if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
				        			fprintf(stderr, "write: failed at sending transmit ok\n");
				        			fclose(ffff);
				        			close(client_fd);
				        		}
			        		}
			        		if(r != 0) {
			        			if(fwrite(&buf, sizeof(char), 1, ffff) == -1) {
			        				if(write(client_fd, &TRANSMIT_ERROR_N, sizeof(int)) == -1) {
				        				fprintf(stderr, "write: failed at sending transmit ok\n");
				        				fclose(ffff);
				        				close(client_fd);
				        			}
				        		}
				        		bytes_read += r;
			        		}
			        	}

						if (chmod(dest, root->mode) == -1){
					        fprintf(stderr, "chmod: '%s' can't be changed\n", dest);
					        fclose(ffff);
					        close(client_fd);
				    	}

				    	fclose(ffff);

				    	if(write(client_fd, &TRANSMIT_OK_N, sizeof(int)) == -1) {
	        				fprintf(stderr, "write: failed at sending transmit ok\n");
	        				close(client_fd);
	        			}
		            }
		            fclose(f);
		        }
	        } else if (S_ISDIR(root->mode)) {
	        	umask(0);
        		mkdir(dest, root->mode);
     
    			if(write(client_fd, &MATCH_N, sizeof(int)) == -1) {
    				fprintf(stderr, "write: failed at sending match \n");
    				close(client_fd);
    			}
        	
	        }
		}
	}
}
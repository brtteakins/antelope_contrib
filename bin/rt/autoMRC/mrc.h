/***********************************************************************
 *
 *  mrc/mrc.h
 *
 *  Include file of constants, macros, function declarations, etc.
 *
 *
 *  Author: Marina Harkins-Glushko
 *  	    UCSD, IGPP
 *	    glushko@ucsd.edu
 ***********************************************************************/
#ifndef mrc_h_included
#define mrc_h_included
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdio.h>      
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include "db.h"
#include "coords.h"
#include "arrays.h"
#include "Pkt.h"

#define MAX_OFF  100000  /* Max LTA value which good data should not exceed */

typedef struct Orbpipe_packet {
	double time ;
	int pktid ; 
	char srcname[ORBSRCNAME_SIZE] ;
	char *packet ; 
	int nbytes, bufsize; 
} Orbpipe_packet ;

typedef struct Orbpipe {
    Tbl	*tbl ; 
    int maxpkts ;
    double last ;
    Orbpipe_packet rpkt ;
} Orbpipe ;

extern Tbl *MailAdd;
extern Arr *MailSent;

extern Arr *Dasid;
extern Arr *PChan;

typedef struct Ch {
    double time;
    int nsamp;
    long lta;
} Ch;

typedef struct ChPipe {
    double time;
    int nsamp;
    int maxtbl;
    Ch crnt_chan;
    Tbl *tbl;
} ChPipe;

typedef struct ChRec {
    double time;
    long pktlta;
    long lta;
    int nsamp;
    int dasid;
    int mrcnum;
    char srcid[ORBSRCNAME_SIZE];
    ChPipe *chpipe;
} ChRec;

typedef struct Das {
    double time;
    char srcid[ORBSRCNAME_SIZE];
    Orbpipe *apipe;
} Das;


extern int MaxOff;
extern int Ls;
extern char *logname;
extern FILE *fplog;

extern struct sockaddr_in myadd_in;           
extern struct sockaddr_in peer_in;

#ifdef __STDC__
#define PL_(x) x
#else
#define PL_(x) ( )
#endif /* __STDC__ */
 
extern void usage PL_(( void ));
extern int main PL_(( int argc, char *argv[] ));
extern int open_dc PL_(( char *ports ));
 
#undef PL_

#endif

/***************************************************************************
 * statefile.c:
 *
 * Routines to save and recover SeedLink sequence numbers to/from a file.
 *
 * Written by Chad Trabant, ORFEUS/EC-Project MEREDIAN
 *
 * modified: 2003.276
 ***************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "libslink.h"


/***************************************************************************
 * sl_savestate():
 * Save the all the current the sequence numbers and time stamps into the
 * given state file.
 *
 * Returns:
 * -1 : error
 *  0 : completed successfully
 ***************************************************************************/
int
sl_savestate (SLCD * slconn, const char *statefile)
{
  SLstream *curstream;
  FILE *statefp;

  curstream = slconn->streams;

  /* Open the state file */
  if ((statefp = fopen (statefile, "w")) == NULL)
    {
      sl_log_r (slconn, 1, 0, "cannot open state file for writing\n");
      return -1;
    }

  sl_log_r (slconn, 0, 2, "saving connection state to state file\n");

  /* Traverse stream chain and write sequence numbers */
  while (curstream != NULL)
    {
      fprintf (statefp, "%s %s %d %s\n",
	       curstream->net, curstream->sta,
	       curstream->seqnum, curstream->timestamp);

      curstream = curstream->next;
    }

  if (fclose (statefp))
    {
      sl_log_r (slconn, 1, 0, "closing state file, %s\n", strerror (errno));
      return -1;
    }

  return 0;
}


/***************************************************************************
 * sl_recoverstate():
 * Recover the state file and put the sequence numbers and time stamps into
 * the pre-existing stream chain entries.
 *
 * Returns:
 * -1 : error
 *  0 : completed successfully
 *  1 : file could not be opened (probably not found)
 ***************************************************************************/
int
sl_recoverstate (SLCD * slconn, const char *statefile)
{
  SLstream *curstream;
  FILE *statefp;
  char net[3];
  char sta[6];
  char timestamp[20];
  char line[100];
  int seqnum;
  int fields;
  int count;

  net[0] = '\0';
  sta[0] = '\0';
  timestamp[0] = '\0';

  /* Open the state file */
  if ((statefp = fopen (statefile, "r")) == NULL)
    {
      if (errno == ENOENT)
	{
	  sl_log_r (slconn, 0, 0, "could not open state file: %s\n", statefile);
	  return 1;
	}
      else
	{
	  sl_log_r (slconn, 1, 0, "opening state file, %s\n", strerror (errno));
	  return -1;
	}
    }

  sl_log_r (slconn, 0, 1, "recovering connection state from state file\n");

  count = 1;

  while ( (fgets (line, sizeof(line), statefp)) !=  NULL)
    {
      fields = sscanf (line, "%2s %5s %d %19[0-9,]\n",
		       net, sta, &seqnum, timestamp);

      if ( fields < 0 )
        continue;
      
      if ( fields < 3 )
	{
	  sl_log_r (slconn, 1, 0, "error parsing line %d of state file\n", count); 
	}
      
      /* Search for a matching NET and STA in the stream chain */
      curstream = slconn->streams;
      while (curstream != NULL)
	{
	  if (!strcmp (net, curstream->net) &&
	      !strcmp (sta, curstream->sta))
	    {
	      curstream->seqnum = seqnum;

	      if ( fields == 4 )
		strncpy (curstream->timestamp, timestamp, 20);

	      break;
	    }
	  curstream = curstream->next;
	}

      count++;
    }

  if ( fclose (statefp) )
    {
      sl_log_r (slconn, 1, 0, "closing state file, %s\n", strerror (errno));
      return -1;
    }

  return 0;
}
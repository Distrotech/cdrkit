/* 
 * Copyright 2006 Eduard Bloch 
 *
 * This code emulates the interface of the original defaults.c file. However,
 * it improves its behaviour and deals with corner cases: prepended and
 * trailing spaces on variable and value, no requirement for using TABs
 * anymore. No requirements to insert dummy values like -1 or "".
 *
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <mconfig.h>
#include <stdlib.h>
#include <stdio.h>
#include "defaults.h"
#include <ctype.h>
#include <string.h>

#define CFGPATH "/etc/default/wodim"
/* The better way would be exporting the meta functions to getnum.h or so */
extern int	getnum		(char *arg, long *valp);

enum parstate {
   KEYBEGINSEARCH,
   KEYCOMPARE,
   EQSIGNSEARCH,
   VALBEGINSEARCH,
   LASTCHARSEARCH
};
#define GETVAL_BUF_LEN 256
#define isUspace(x) isspace( (int) (unsigned char) x)

/*
 * Warning, uses static line buffer, not reentrant. NULL returned if the key isn't found.
 */
static char *get_value(FILE *srcfile, char *key) {
   static char linebuf[GETVAL_BUF_LEN];

   if(!srcfile)
      return NULL;

   rewind(srcfile);
next_line:
   while(fgets(linebuf, sizeof(linebuf)-1, srcfile)) {
      int i;
      int keybeg;
      int s=KEYBEGINSEARCH;
      char *ret=NULL;
      int lastchar=0;

      /* simple state machine, char position moved by the states (or not),
       * state change is done by the state (or not) */
      for( i=0 ; i<sizeof(linebuf) ; ) {
         /* printf("key: %s, %s, s: %d\n", key,  linebuf, s); */
         switch(s) {
            case(KEYBEGINSEARCH):
               {
                  if(isUspace(linebuf[i]))
                     i++;
                  else if(linebuf[i] == '#' || linebuf[i]=='\0')
                     goto next_line;
                  else {
                     s=KEYCOMPARE;
                     keybeg=i;
                  }
               }
               break;
            case(KEYCOMPARE): /* compare the key */
               {
                  if(key[i-keybeg]=='\0') 
                     /* end of key, next state decides what to do on this position */
                     s=EQSIGNSEARCH;
                  else {
                     if(linebuf[i-keybeg]!=key[i-keybeg])
                        goto next_line;
                     else
                        i++;
                  }
               }
               break;
            case(EQSIGNSEARCH): /* skip whitespace, stop on =, break on anything else */
               {
                  if(isUspace(linebuf[i]))
                     i++;
                  else if(linebuf[i]=='=') {
                        s=VALBEGINSEARCH;
                        i++;
                     }
                  else
                     goto next_line;
               }
               break;
            case(VALBEGINSEARCH):
               {
                  if(isUspace(linebuf[i]))
                     i++;
                  else {
                     /* possible at EOF */
                     if(linebuf[i] == '\0')
                        return NULL;

                     lastchar=i-1; /* lastchar can be a space, see below */
                     ret= & linebuf[i];
                     s=LASTCHARSEARCH;
                  }
               }
               break;
            case(LASTCHARSEARCH):
               {
                  if(linebuf[i]) {
                     if(!isUspace(linebuf[i]))
                        lastchar=i;
                  }
                  else { /* got string end, terminate after the last seen char */
                     if(linebuf+lastchar < ret) /* no non-space found */
                        return NULL;
                     linebuf[lastchar+1]='\0';
                     return ret;
                  }
                  i++;
               }
               break;
         }
      }
   }
   return NULL;
}

void
cdr_defaults(char **p_dev_name, int *p_speed, long *p_fifosize, char **p_drv_opts) {
   FILE *stream;
   char *t; /* tmp */
   int wc=0;
   char loc[256], sSpeed[11], sFs[11], sOpts[81];
   char *devcand=NULL;

   stream=fopen(CFGPATH, "r");

   if(p_dev_name && *p_dev_name)
      devcand=*p_dev_name;
   else if(NULL!=(t=getenv("CDR_DEVICE")))
      devcand=t;
   else if(NULL!=(t=get_value(stream, "CDR_DEVICE")))
      devcand=strdup(t); // needs to use it as a key later, same stat. memory

   if(devcand && NULL != (t=get_value(stream,devcand))) {
      /* extract them now, may be used later */
      wc=sscanf(t, "%255s %10s %10s %80s", loc, sSpeed, sFs, sOpts);
   }

   if(p_dev_name) {
      if(wc>0)
         *p_dev_name = strdup(loc);
      else if(devcand) // small mem. leak possible, does not matter, checks for that would require more code size than we loose
         *p_dev_name=strdup(devcand);
   }
   if(p_speed) { /* sth. to write back */
      char *bad;
      int cfg_speed=-1;

      /* that value may be used twice */
      if(NULL!=(t=get_value(stream, "CDR_SPEED"))) {
         cfg_speed=strtol(t,&bad,10);
         if(*bad || cfg_speed<-1) {
            fprintf(stderr, "Bad default CDR_SPEED setting (%s).\n", t);
            exit(EXIT_FAILURE);
         }
      }

      if(*p_speed>0) { 
         /* ok, already set by the program arguments */
      }
      else if(NULL!=(t=getenv("CDR_SPEED"))) {
         *p_speed=strtol(t,&bad,10);
         if(*bad || *p_speed<-1) {
            fprintf(stderr, "Bad CDR_SPEED environment (%s).\n", t);
            exit(EXIT_FAILURE);
         }
      }
      else if(wc>1 && *sSpeed) {
         *p_speed=strtol(sSpeed, &bad, 10);
         if(*bad || *p_speed<-1) {
            fprintf(stderr, "Bad speed (%s) in the config, drive description.\n", sSpeed);
            exit(EXIT_FAILURE);
         }
         if(*p_speed==-1) 
            /* that's autodetect, use the config default as last ressort */
            *p_speed=cfg_speed;
      }
      else 
         *p_speed=cfg_speed;
   }
   if(p_fifosize) { /* sth. to write back */
      if(*p_fifosize>0) { 
         /* ok, already set by the user */
      }
      else if(NULL!=(t=getenv("CDR_FIFOSIZE"))) {
         if(getnum(t, p_fifosize)!=1 || *p_fifosize<-1) {
            fprintf(stderr, "Bad CDR_FIFOSIZE environment (%s).\n", t);
            exit(EXIT_FAILURE);
         }
      }
      else if(wc>2 && *sFs) {
         if(getnum(sFs, p_fifosize)!=1 || *p_fifosize<-1) {
            fprintf(stderr, "Bad fifo size (%s) in the config, device description.\n", sSpeed);
            exit(EXIT_FAILURE);
         }
      }
      else if(NULL!=(t=get_value(stream, "CDR_FIFOSIZE"))) {
         if(getnum(t, p_fifosize)!=1 || *p_fifosize<-1) {
            fprintf(stderr, "Bad speed default setting (%s).\n", t);
            exit(EXIT_FAILURE);
         }
      }
      /* undocumented option. Most likely to prevent killing Schily's
       * underpowered machines (see docs) by allocating too much memory after
       * doing a mistake in the config. */
      if(NULL!=(t=get_value(stream, "CDR_MAXFIFOSIZE"))) {
         long max;
         if(getnum(t, &max)!=1 || *p_fifosize<-1) {
            fprintf(stderr, "Bad CDR_MAXFIFOSIZE setting (%s).\n", t);
            exit(EXIT_FAILURE);
         }
         if(*p_fifosize>max)
            *p_fifosize=max;
      }
   }

   if(p_drv_opts && !*p_drv_opts && wc>3 && strcmp(sOpts, "\"\""))
      *p_drv_opts=strdup(sOpts);

   if(stream != (FILE*)NULL)
      fclose(stream);

}

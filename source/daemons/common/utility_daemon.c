/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://www.gridengine.sunsource.net/license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "sge_conf.h"
#include "sgermon.h"
#include "cull.h"
#include "sge_stringL.h"
#include "utility_daemon.h"
#include "sge_log.h"
#include "msg_common.h"
#include "msg_daemons_common.h"
#include "sge_dirent.h"

/****
 **** sge_get_dirents
 ****
 **** Returns the entries of the specified directory
 **** (without . and ..) as lList of type ST_Type.
 **** The lList has to be freed from the caller.
 **** On any error, NULL is returned.
 ****/
lList *sge_get_dirents(
char *path 
) {
   lList *entries = NULL;
   DIR *cwd;
   SGE_STRUCT_DIRENT *dent;

   DENTER(TOP_LAYER, "sge_get_dirents");

   cwd = opendir(path);

   if (cwd == (DIR *) 0) {
      ERROR((SGE_EVENT, MSG_FILE_CANTOPENDIRECTORYX_SS, path, strerror(errno)));
      return (NULL);
   }

   while ((dent = SGE_READDIR(cwd)) != NULL) {
      if (!dent->d_name)
         continue;              
      if (!dent->d_name[0])
         continue;             
      if (strcmp(dent->d_name, "..") == 0 || strcmp(dent->d_name, ".") == 0)
         continue;
      lAddElemStr(&entries, STR, dent->d_name, ST_Type);
   }
   closedir(cwd);

   DEXIT;
   return (entries);
}

#ifndef _SGE_SHARETREE_QMASTER_H_
#define _SGE_SHARETREE_QMASTER_H_
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

int sge_add_sharetree(lListElem *ep, lList **lpp, lList **alpp, char *ruser, char *rhost);
int sge_mod_sharetree(lListElem *ep, lList **lpp, lList **alpp, char *ruser, char *rhost);
int sge_del_sharetree(lList **lpp, lList **alpp, char *ruser, char *rhost);

int update_sharetree(lList **alpp, lList *dst, lList *src);

lListElem *getNode(lList *share_tree, char *name, int node_type, int recurse);

#endif /*  _SGE_SHARETREE_QMASTER_H_ */


#ifndef __SGE_PE_QMASTER_H
#define __SGE_PE_QMASTER_H
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



#include "sge_c_gdi.h"

/* funtions called from within gdi framework in qmaster */
int pe_mod(lList **alpp, lListElem *new_pe, lListElem *pe, int add, char *ruser, char *rhost, gdi_object_t *object, int sub_command);

int pe_spool(lList **alpp, lListElem *pep, gdi_object_t *object);

int pe_success(lListElem *ep, lListElem *old_ep, gdi_object_t *object);

/* funtions called via gdi and inside the qmaster */
int sge_del_pe(lListElem *, lList **, char *, char *);

/* find a specific pe */
lListElem *sge_locate_pe(char *pe_name);

lListElem *sge_match_pe(char *wildcard);

/* to do at qmasters startup */
void debit_all_jobs_from_pes(lList *pe_list);

void debit_job_from_pe(lListElem *pep, int slots, u_long32 job_id);

void reverse_job_from_pe(lListElem *pep, int slots, u_long32 job_id);

int validate_pe(int startup, lListElem *pep, lList **alpp);

#endif /* __SGE_PE_QMASTER_H */


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
#include <stdio.h>
#include <string.h>
#include <pwd.h>

#include "cull.h"
#include "sgermon.h"
#include "sge_peopen.h"
#include "sge_gdi_intern.h"
#include "sge_log.h"
#include "setup_path.h"
#include "sge_copy_append.h"
#include "sge_jobL.h"
#include "sge_string.h"
#include "sge_answerL.h"
#include "sge_afsutil.h"
#include "sge_switch_user.h"
#include "execution_states.h"
#include "sge_me.h"
#include "sge_arch.h"
#include "sge_gdi.h"
#include "qm_name.h"
#include "sge_exit.h"
#include "msg_gdilib.h"
#include "sge_feature.h"
#include "sge_getpwnam.h"
#include "dispatcher.h"
#include "sge_security.h"

#ifdef CRYPTO
#include <openssl/evp.h>
#endif

#define SGE_SEC_BUFSIZE 1024

#define ENCODE_TO_STRING   1
#define DECODE_FROM_STRING 0

static int sge_encrypt(char *intext, int inlen, char *outbuf, int outsize);
static int sge_decrypt(char *intext, int inlen, char *outbuf, int *outsize);
static int change_encoding(char *cbuf, int* csize, unsigned char* ubuf, int* usize, int mode);

/****** sge_security/sge_security_initialize() ***************************
*
*  NAME
*     sge_security_initialize -- initialize sge security
*
*  SYNOPSIS
*     int sge_security_initialize(char *name);
*
*  FUNCTION
*     Initialize sge security by initializing the underlying security
*     mechanism and setup the corresponding data structures
*
*  INPUTS
*     name - name of enrolling program
*
*  RETURN
*     0  in case of success, something different otherwise 
*
*  EXAMPLE
*
*  NOTES
*
*  BUGS
*
*  SEE ALSO
*
**************************************************************************/
int sge_security_initialize(char *name)
{
   static int initialized = 0;

   DENTER(TOP_LAYER, "sge_security_initialize");
   if (!initialized) {
#ifdef SECURE
      if(sec_init(name)){
/*          CRITICAL((SGE_EVENT, MSG_GDI_INITSECURITYDATAFAILED )); */
         DEXIT;
         return -1;
      }
#endif

#ifdef KERBEROS
      if(krb_init(name)){
         CRITICAL((SGE_EVENT,MSG_GDI_INITKERBEROSSECURITYDATAFAILED ));
         DEXIT;
         return -1;
      }
#endif   
      initialized = 1; 
   }

   DEXIT;
   return 0;
}



/****** set_sec_cred() ***************************************
*
*  NAME
*     set_sec_cred -- get credit for security system
*
*  SYNOPSIS
*     int set_sec_cred(lListElem *job);
*
*  FUNCTION
*     Tries to get credit for a security system (DCE or KERBEROS),
*     sets the accordant information in the job structure
*     If an error occurs the return value is unequal 0
*
*  INPUTS
*     job - the job structure
*
*  RETURN
*     0  in case of success, something different otherwise 
*
*  EXAMPLE
*
*  NOTES
*     Hope, the above description is correct - don't know the DCE/KERBEROS
*     code.
*
*  BUGS
*
*  SEE ALSO
*
****************************************************************************
*/
int set_sec_cred(lListElem *job)
{

   pid_t command_pid;
   FILE *fp_in, *fp_out, *fp_err;
   char *str;
   int ret = 0;
   char binary[1024];
   char cmd[2048];
   char line[1024];


   DENTER(TOP_LAYER, "set_sec_cred");
   
   if (feature_is_enabled(FEATURE_AFS_SECURITY)) {
      sprintf(binary, "%s/util/get_token_cmd", path.sge_root);

      if (get_token_cmd(binary, NULL) != 0) {
         fprintf(stderr, MSG_QSH_QSUBFAILED);
         SGE_EXIT(1);
      }   
      
      command_pid = peopen("/bin/sh", 0, binary, NULL, NULL, &fp_in, &fp_out, &fp_err);

      if (command_pid == -1) {
         fprintf(stderr, MSG_QSUB_CANTSTARTCOMMANDXTOGETTOKENQSUBFAILED_S, binary);
         SGE_EXIT(1);
      }

      str = sge_bin2string(fp_out, 0);
      
      ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);
      
      lSetString(job, JB_tgt, str);
   }
      
   /*
    * DCE / KERBEROS security stuff
    *
    *  This same basic code is in qsh.c and qmon_submit.c
    *  It should really be moved to a common place. It would
    *  be nice if there was a generic job submittal function.
    */

   if (feature_is_enabled(FEATURE_DCE_SECURITY) ||
       feature_is_enabled(FEATURE_KERBEROS_SECURITY)) {
      sprintf(binary, "%s/utilbin/%s/get_cred", path.sge_root, sge_arch());

      if (get_token_cmd(binary, NULL) != 0) {
         fprintf(stderr, MSG_QSH_QSUBFAILED);
         SGE_EXIT(1);
      }   

      sprintf(cmd, "%s %s%s%s", binary, "sge", "@", sge_get_master(0));
      
      command_pid = peopen("/bin/sh", 0, cmd, NULL, NULL, &fp_in, &fp_out, &fp_err);

      if (command_pid == -1) {
         fprintf(stderr, MSG_QSH_CANTSTARTCOMMANDXTOGETCREDENTIALSQSUBFAILED_S, binary);
         SGE_EXIT(1);
      }

      str = sge_bin2string(fp_out, 0);

      while (!feof(fp_err)) {
         if (fgets(line, sizeof(line), fp_err))
            fprintf(stderr, "get_cred stderr: %s", line);
      }

      ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

      if (ret) {
         fprintf(stderr, MSG_QSH_CANTGETCREDENTIALS);
      }
      
      lSetString(job, JB_cred, str);
   }
   DEXIT;
   return ret;
}   

#if 0
      
      /*
      ** AFS specific things
      */
      if (feature_is_enabled(FEATURE_AFS_SECUIRITY)) {
         pid_t command_pid;
         FILE *fp_in, *fp_out, *fp_err;
         char *cp;
         int ret;
         char binary[1024];

         sprintf(binary, "%s/util/get_token_cmd", path.sge_root);

         if (get_token_cmd(binary, buf))
            goto error;

         command_pid = peopen("/bin/sh", 0, binary, NULL, NULL, &fp_in, &fp_out, &fp_err);

         if (command_pid == -1) {
            DPRINTF(("can't start command \"%s\" to get token\n",  binary));
            sprintf(buf, "can't start command \"%s\" to get token\n",  binary);
            goto error;
         }

         cp = sge_bin2string(fp_out, 0);

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         lSetString(lFirst(lp), JB_tgt, cp);
      }

      /*
      ** DCE / KERBEROS security stuff
      **
      **  This same basic code is in qsh.c and qsub.c. It really should
      **  be put in a common place.
      */

      if (feature_is_enabled(FEATURE_DCE_SECURITY) ||
          feature_is_enabled(FEATURE_KERBEROS_SECURITY)) {
         pid_t command_pid;
         FILE *fp_in, *fp_out, *fp_err;
         char *str;
         char binary[1024], cmd[2048];
         int ret;
         char line[1024];

         sprintf(binary, "%s/utilbin/%s/get_cred", path.sge_root, sge_arch());

         if (get_token_cmd(binary, buf) != 0)
            goto error;

         sprintf(cmd, "%s %s%s%s", binary, "sge", "@", sge_get_master(0));
      
         command_pid = peopen("/bin/sh", 0, cmd, NULL, NULL, &fp_in, &fp_out, &fp_err);

         if (command_pid == -1) {
            DPRINTF((buf, "can't start command \"%s\" to get credentials "
                     "- qsub failed\n", binary));
            sprintf(buf, "can't start command \"%s\" to get credentials "
                    "- qsub failed\n", binary);
            goto error;
         }

         str = sge_bin2string(fp_out, 0);

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               fprintf(stderr, "get_cred stderr: %s", line);
         }

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         if (ret) {
            DPRINTF(("warning: could not get credentials\n"));
            sprintf(buf, "warning: could not get credentials\n");
            goto error;
         }
         
         lSetString(lFirst(lp), JB_cred, str);
      }

#endif


void cache_sec_cred(lListElem *jep, char *rhost)
{
   DENTER(TOP_LAYER, "cache_sec_cred");

   /* 
    * Execute command to get DCE or Kerberos credentials.
    * 
    * This needs to be made asynchronous.
    *
    */

   if (feature_is_enabled(FEATURE_DCE_SECURITY) ||
       feature_is_enabled(FEATURE_KERBEROS_SECURITY)) {

      pid_t command_pid=-1;
      FILE *fp_in, *fp_out, *fp_err;
      char *str;
      char binary[1024], cmd[2048], ccname[256];
      int ret;
      char *env[2];

      /* set up credentials cache for this job */
      sprintf(ccname, "KRB5CCNAME=FILE:/tmp/krb5cc_qmaster_" u32,
              lGetUlong(jep, JB_job_number));
      env[0] = ccname;
      env[1] = NULL;

      sprintf(binary, "%s/utilbin/%s/get_cred", path.sge_root, sge_arch());

      if (get_token_cmd(binary, NULL) == 0) {
         char line[1024];

         sprintf(cmd, "%s %s%s%s", binary, "sge", "@", rhost);

         switch2start_user();
         command_pid = peopen("/bin/sh", 0, cmd, NULL, env, &fp_in, &fp_out, &fp_err);
         switch2admin_user();

         if (command_pid == -1) {
            ERROR((SGE_EVENT, MSG_SEC_NOSTARTCMD4GETCRED_SU, 
                   binary, u32c(lGetUlong(jep, JB_job_number))));
         }

         str = sge_bin2string(fp_out, 0);

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR((SGE_EVENT, MSG_QSH_GET_CREDSTDERR_S, line));
         }

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         lSetString(jep, JB_cred, str);

         if (ret) {
            ERROR((SGE_EVENT, MSG_SEC_NOCRED_USSI, 
                   u32c(lGetUlong(jep, JB_job_number)), rhost, binary, ret));
         }
      } else {
         ERROR((SGE_EVENT, MSG_SEC_NOCREDNOBIN_US,  
                u32c(lGetUlong(jep, JB_job_number)), binary));
      }
   }
   DEXIT;
}   

void delete_credentials(lListElem *jep)
{

   DENTER(TOP_LAYER, "delete_credentials");

   /* 
    * Execute command to delete the client's DCE or Kerberos credentials.
    */
   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
        lGetString(jep, JB_cred)) {

      pid_t command_pid=-1;
      FILE *fp_in, *fp_out, *fp_err;
      char binary[1024], cmd[2048], ccname[256], ccfile[256], ccenv[256];
      int ret=0;
      char *env[2];
      char tmpstr[1024];

      /* set up credentials cache for this job */
      sprintf(ccfile, "/tmp/krb5cc_qmaster_" u32,
              lGetUlong(jep, JB_job_number));
      sprintf(ccenv, "FILE:%s", ccfile);
      sprintf(ccname, "KRB5CCNAME=%s", ccenv);
      env[0] = ccname;
      env[1] = NULL;

      sprintf(binary, "%s/utilbin/%s/delete_cred", path.sge_root, sge_arch());

      if (get_token_cmd(binary, NULL) == 0) {
         char line[1024];

         sprintf(cmd, "%s -s %s", binary, "sge");

         switch2start_user();
         command_pid = peopen("/bin/sh", 0, cmd, NULL, env, &fp_in, &fp_out, &fp_err);
         switch2admin_user();

         if (command_pid == -1) {
            strcpy(tmpstr, SGE_EVENT);
            ERROR((SGE_EVENT, MSG_SEC_STARTDELCREDCMD_SU,
                   binary, u32c(lGetUlong(jep, JB_job_number))));
            strcpy(SGE_EVENT, tmpstr);
         }

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err)) {
               strcpy(tmpstr, SGE_EVENT);
               ERROR((SGE_EVENT, MSG_SEC_DELCREDSTDERR_S, line));
               strcpy(SGE_EVENT, tmpstr);
            }
         }

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         if (ret != 0) {
            strcpy(tmpstr, SGE_EVENT);
            ERROR((SGE_EVENT, MSG_SEC_DELCREDRETCODE_USI,
                   u32c(lGetUlong(jep, JB_job_number)), binary, ret));
            strcpy(SGE_EVENT, tmpstr);
         }

      } else {
         strcpy(tmpstr, SGE_EVENT);
         ERROR((SGE_EVENT, MSG_SEC_DELCREDNOBIN_US,  
                u32c(lGetUlong(jep, JB_job_number)), binary));
         strcpy(SGE_EVENT, tmpstr);
      }
   }

   DEXIT;
}



/* 
 * Execute command to store the client's DCE or Kerberos credentials.
 * This also creates a forwardable credential for the user.
 */
int store_sec_cred(sge_gdi_request *request, lListElem *jep, int do_authentication, lList** alpp)
{

   DENTER(TOP_LAYER, "store_sec_cred");

   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
       (do_authentication || lGetString(jep, JB_cred))) {

      pid_t command_pid;
      FILE *fp_in, *fp_out, *fp_err;
      char line[1024], binary[1024], cmd[2048], ccname[256];
      int ret;
      char *env[2];

      if (do_authentication && lGetString(jep, JB_cred) == NULL) {
         ERROR((SGE_EVENT, MSG_SEC_NOAUTH_U, u32c(lGetUlong(jep, JB_job_number))));
         sge_add_answer(alpp, SGE_EVENT, STATUS_EUNKNOWN, 0);
         DEXIT;
         return -1;
      }

      /* set up credentials cache for this job */
      sprintf(ccname, "KRB5CCNAME=FILE:/tmp/krb5cc_qmaster_" u32,
              lGetUlong(jep, JB_job_number));
      env[0] = ccname;
      env[1] = NULL;

      sprintf(binary, "%s/utilbin/%s/put_cred", path.sge_root, sge_arch());

      if (get_token_cmd(binary, NULL) == 0) {
         sprintf(cmd, "%s -s %s -u %s", binary, "sge", lGetString(jep, JB_owner));

         switch2start_user();
         command_pid = peopen("/bin/sh", 0, cmd, NULL, env, &fp_in, &fp_out, &fp_err);
         switch2admin_user();

         if (command_pid == -1) {
            ERROR((SGE_EVENT, MSG_SEC_NOSTARTCMD4GETCRED_SU,
                   binary, u32c(lGetUlong(jep, JB_job_number))));
         }

         sge_string2bin(fp_in, lGetString(jep, JB_cred));

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR((SGE_EVENT, MSG_SEC_PUTCREDSTDERR_S, line));
         }

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         if (ret) {
            ERROR((SGE_EVENT, MSG_SEC_NOSTORECRED_USI,
                   u32c(lGetUlong(jep, JB_job_number)), binary, ret));
         }

         /*
          * handle authentication failure
          */

         if (do_authentication && (ret != 0)) {
            ERROR((SGE_EVENT, MSG_SEC_NOAUTH_U, u32c(lGetUlong(jep, JB_job_number))));
            sge_add_answer(alpp, SGE_EVENT, STATUS_EUNKNOWN, 0);
            DEXIT;
            return -1;
         }

      } else {
         ERROR((SGE_EVENT, MSG_SEC_NOSTORECREDNOBIN_US, 
                u32c(lGetUlong(jep, JB_job_number)), binary));
      }
   }
#ifdef KERBEROS

   /* get client TGT and store in job entry */

   {
      krb5_error_code rc;
      krb5_creds ** tgt_creds = NULL;
      krb5_data outbuf;

      outbuf.length = 0;

      if (krb_get_tgt(request->host, request->commproc, request->id,
		      request->request_id, &tgt_creds) == 0) {
      
	 if ((rc = krb_encrypt_tgt_creds(tgt_creds, &outbuf))) {
	    ERROR((SGE_EVENT, MSG_SEC_KRBENCRYPTTGT_SSIS, 
            request->host, request->commproc, request->id, error_message(rc)));
	 }

	 if (rc == 0)
	    lSetString(jep, JB_tgt,
                       krb_bin2str(outbuf.data, outbuf.length, NULL));

	 if (outbuf.length)
	    krb5_xfree(outbuf.data);

         /* get rid of the TGT credentials */
         krb_put_tgt(request->host, request->commproc, request->id,
		     request->request_id, NULL);

      }
   }

#endif

   return 0;
}   





int store_sec_cred2(lListElem *jelem, int do_authentication, int *general, char* err_str)
{
   int ret = 0;
   char *cred;
   
   DENTER(TOP_LAYER, "store_sec_cred2");

   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
       (cred = lGetString(jelem, JB_cred)) && cred[0]) {

      pid_t command_pid;
      FILE *fp_in, *fp_out, *fp_err;
      char binary[1024], cmd[2048], ccname[256], ccfile[256], ccenv[256],
           jobstr[64];
      int ret;
      char *env[3];
      lListElem *vep;

      /* set up credentials cache for this job */
      sprintf(ccfile, "/tmp/krb5cc_%s_" u32, "sge", lGetUlong(jelem, JB_job_number));
      sprintf(ccenv, "FILE:%s", ccfile);
      sprintf(ccname, "KRB5CCNAME=%s", ccenv);
      sprintf(jobstr, "JOB_ID="u32, lGetUlong(jelem, JB_job_number));
      env[0] = ccname;
      env[1] = jobstr;
      env[2] = NULL;
      vep = lAddSubStr(jelem, VA_variable, "KRB5CCNAME", JB_env_list, VA_Type);
      lSetString(vep, VA_value, ccenv);

      sprintf(binary, "%s/utilbin/%s/put_cred", path.sge_root,
              sge_arch());

      if (get_token_cmd(binary, NULL) == 0) {
         char line[1024];

         sprintf(cmd, "%s -s %s -u %s -b %s", binary, "sge",
                 lGetString(jelem, JB_owner), lGetString(jelem, JB_owner));

         switch2start_user();
         command_pid = peopen("/bin/sh", 0, cmd, NULL, env, &fp_in, &fp_out, &fp_err);
         switch2admin_user();

         if (command_pid == -1) {
            ERROR((SGE_EVENT, MSG_SEC_NOSTARTCMD4GETCRED_SU, binary, u32c(lGetUlong(jelem, JB_job_number))));
         }

         sge_string2bin(fp_in, lGetString(jelem, JB_cred));

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR((SGE_EVENT, MSG_SEC_PUTCREDSTDERR_S, line));
         }

         ret = peclose(command_pid, fp_in, fp_out, fp_err, NULL);

         if (ret) {
            ERROR((SGE_EVENT, MSG_SEC_NOSTORECRED_USI, u32c(lGetUlong(jelem, JB_job_number)), binary, ret));
         }

         /*
          * handle authentication failure
          */                                                  
                                                              
         if (do_authentication && (ret != 0)) {               
            ERROR((SGE_EVENT, MSG_SEC_KRBAUTHFAILURE,
                   u32c(lGetUlong(jelem, JB_job_number))));         
            sprintf(err_str, MSG_SEC_KRBAUTHFAILUREONHOST,
                    u32c(lGetUlong(jelem, JB_job_number)),
                    me.unqualified_hostname);                 
            *general = GFSTATE_JOB;                            
         }                                                    
      } 
      else {
         ERROR((SGE_EVENT, MSG_SEC_NOSTORECREDNOBIN_US, u32c(lGetUlong(jelem, JB_job_number)), binary));
      }
   }
   DEXIT;
   return ret;
}

#ifdef KERBEROS
int kerb_job(
lListElem *jelem,
struct dispatch_entry *de 
) {
   /* get TGT and store in job entry and in user's credentials cache */
   krb5_error_code rc;
   krb5_creds ** tgt_creds = NULL;
   krb5_data outbuf;

   DENTER(TOP_LAYER, "kerb_job");

   outbuf.length = 0;

   if (krb_get_tgt(de->host, de->commproc, de->id, lGetUlong(jelem, JB_job_number), &tgt_creds) == 0) {
      struct passwd *pw;

      if ((rc = krb_encrypt_tgt_creds(tgt_creds, &outbuf))) {
         ERROR((SGE_EVENT, MSG_SEC_KRBENCRYPTTGTUSER_SUS, lGetString(jelem, JB_owner),
                u32c(lGetUlong(jelem, JB_job_number)), error_message(rc)));
      }

      if (rc == 0)
         lSetString(jelem, JB_tgt, krb_bin2str(outbuf.data, outbuf.length, NULL));

      if (outbuf.length)
         krb5_xfree(outbuf.data);

      pw = sge_getpwnam(lGetString(jelem, JB_owner));

      if (pw) {
         if (krb_store_forwarded_tgt(pw->pw_uid,
               lGetUlong(jelem, JB_job_number),
               tgt_creds) == 0) {
            char ccname[40];
            lListElem *vep;

            krb_get_ccname(lGetUlong(jelem, JB_job_number), ccname);
            vep = lAddSubStr(jelem, VA_variable, "KRB5CCNAME", JB_env_list, VA_Type);
            lSetString(vep, VA_value, ccname);
         }

      } else {
         ERROR((SGE_EVENT, MSG_SEC_NOUID_SU, lGetString(jelem, JB_owner), u32c(lGetUlong(jelem, JB_job_number))));
      }

      /* clear TGT out of client entry (this frees the TGT credentials) */
      krb_put_tgt(de->host, de->commproc, de->id, lGetUlong(jelem, JB_job_number), NULL);
   }
   
   DEXIT;
   return 0;
}
#endif


/* get TGT from job entry and store in client connection */
void tgt2cc(lListElem *jep, char *rhost, char* target)
{

#ifdef KERBEROS
   krb5_error_code rc;
   krb5_creds ** tgt_creds = NULL;
   krb5_data inbuf;
   char *tgtstr = NULL;
   u_long32 jid = 0;
   
   DENTER(TOP_LAYER, "tgt2cc");
   inbuf.length = 0;
   jid = lGetUlong(jep, JB_job_number);
   
   if ((tgtstr = lGetString(jep, JB_tgt))) { 
      inbuf.data = krb_str2bin(tgtstr, NULL, &inbuf.length);
      if (inbuf.length) {
         if ((rc = krb_decrypt_tgt_creds(&inbuf, &tgt_creds))) {
            ERROR((SGE_EVENT, MSG_SEC_KRBDECRYPTTGT_US, u32c(jid),
                   error_message(rc)));
         }
      }
      if (rc == 0)
         if (krb_put_tgt(rhost, target, 0, jid, tgt_creds) == 0) {
            krb_set_tgt_id(jid);
 
            tgt_creds = NULL;
         }

      if (inbuf.length)
         krb5_xfree(inbuf.data);

      if (tgt_creds)
         krb5_free_creds(krb_context(), *tgt_creds);
   }

   DEXIT;
#endif

}


void tgtcclr(lListElem *jep, char *rhost, char* target)
{
#ifdef KERBEROS

   /* clear client TGT */
   krb_put_tgt(rhost, target, 0, lGetUlong(jep, JB_job_number), NULL);
   krb_set_tgt_id(0);

#endif
}


/*
** authentication information
*/
int sge_set_auth_info(sge_gdi_request *request, uid_t uid, char *user, 
                        gid_t gid, char *group)
{
   char buffer[SGE_SEC_BUFSIZE];
   char obuffer[3*SGE_SEC_BUFSIZE];

   DENTER(TOP_LAYER, "sge_set_auth_info");

   sprintf(buffer, "%d %d %s %s", uid, gid, user, group);
   if (!sge_encrypt(buffer, sizeof(buffer), obuffer, sizeof(obuffer))) {
      DEXIT;
      return -1;
   }   

   request->auth_info = sge_strdup(NULL, obuffer);

   DEXIT;
   return 0;
}

int sge_get_auth_info(sge_gdi_request *request, uid_t *uid, char *user, 
                        gid_t *gid, char *group)
{
   char dbuffer[2*SGE_SEC_BUFSIZE];
   int dlen = 0;

   DENTER(TOP_LAYER, "sge_get_auth_info");

   if (!sge_decrypt(request->auth_info, strlen(request->auth_info), dbuffer, &dlen)) {
      DEXIT;
      return -1;
   }   

   if (sscanf(dbuffer, "%d %d %s %s", uid, gid, user, group) != 4) {
      DEXIT;
      return -1;
   }   
   
   DEXIT;
   return 0;
}


#ifndef CRYPTO
/*
** dummy encrypt/decrypt functions
*/
static int sge_encrypt(char *intext, int inlen, char *outbuf, int outsize)
{
   int len;

   DENTER(TOP_LAYER, "sge_encrypt");

/*    DPRINTF(("======== intext:\n"SFN"\n=========\n", intext)); */

   len = strlen(intext);
   if (!change_encoding(outbuf, &outsize, (unsigned char*) intext, &len, ENCODE_TO_STRING)) {
      DEXIT;
      return FALSE;
   }   

/*    DPRINTF(("======== outbuf:\n"SFN"\n=========\n", outbuf)); */

   DEXIT;
   return TRUE;
}

static int sge_decrypt(char *intext, int inlen, char *outbuf, int* outsize)
{
   unsigned char decbuf[2*SGE_SEC_BUFSIZE];
   int declen = sizeof(decbuf);

   DENTER(TOP_LAYER, "sge_decrypt");

   if (!change_encoding(intext, &inlen, decbuf, &declen, DECODE_FROM_STRING)) {
      return FALSE;
   }   
   decbuf[declen] = '\0';

   strcpy(outbuf, (char*)decbuf);

/*    DPRINTF(("======== outbuf:\n"SFN"\n=========\n", outbuf)); */

   DEXIT;
   return TRUE;
}

#else

static int sge_encrypt(char *intext, int inlen, char *outbuf, int outsize)
{

   int enclen, tmplen;
   unsigned char encbuf[2*SGE_SEC_BUFSIZE];

   unsigned char key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
   unsigned char iv[] = {1,2,3,4,5,6,7,8};
   EVP_CIPHER_CTX ctx;

   DENTER(TOP_LAYER, "sge_encrypt");

/*    DPRINTF(("======== intext:\n"SFN"\n=========\n", intext)); */

   if (!EVP_EncryptInit(&ctx, /*EVP_enc_null() EVP_bf_cbc()*/EVP_cast5_ofb(), key, iv)) {
      printf("EVP_EncryptInit failure !!!!!!!\n");
      DEXIT;
      return FALSE;
   }   

   if (!EVP_EncryptUpdate(&ctx, encbuf, &enclen, (unsigned char*) intext, inlen)) {
      DEXIT;
      return FALSE;
   }

   if (!EVP_EncryptFinal(&ctx, encbuf + enclen, &tmplen)) {
      DEXIT;
      return FALSE;
   }
   enclen += tmplen;
   EVP_CIPHER_CTX_cleanup(&ctx);

   if (!change_encoding(outbuf, &outsize, encbuf, &enclen, ENCODE_TO_STRING)) {
      DEXIT;
      return FALSE;
   }   

/*    DPRINTF(("======== outbuf:\n"SFN"\n=========\n", outbuf)); */

   DEXIT;
   return TRUE;
}

static int sge_decrypt(char *intext, int inlen, char *outbuf, int* outsize)
{

   int outlen, tmplen;
   unsigned char decbuf[2*SGE_SEC_BUFSIZE];
   int declen = sizeof(decbuf);

   unsigned char key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
   unsigned char iv[] = {1,2,3,4,5,6,7,8};
   EVP_CIPHER_CTX ctx;

   DENTER(TOP_LAYER, "sge_decrypt");

   if (!change_encoding(intext, &inlen, decbuf, &declen, DECODE_FROM_STRING)) {
      DEXIT;
      return FALSE;
   }   

   if (!EVP_DecryptInit(&ctx, /* EVP_enc_null() EVP_bf_cbc()*/EVP_cast5_ofb(), key, iv)) {
      DEXIT;
      return FALSE;
   }
   
   if (!EVP_DecryptUpdate(&ctx, (unsigned char*)outbuf, &outlen, decbuf, declen)) {
      DEXIT;
      return FALSE;
   }

   if (!EVP_DecryptFinal(&ctx, (unsigned char*)outbuf + outlen, &tmplen)) {
      DEXIT;
      return FALSE;
   }
   EVP_CIPHER_CTX_cleanup(&ctx);

   *outsize = outlen+tmplen;

/*    DPRINTF(("======== outbuf:\n"SFN"\n=========\n", outbuf)); */

   DEXIT;
   return TRUE;
}

#endif


#define LOQUAD(i) (((i)&0x0F))
#define HIQUAD(i) (((i)&0xF0)>>4)
#define SETBYTE(hi, lo)  ((((hi)<<4)&0xF0) | (0x0F & (lo)))

static int change_encoding(char *cbuf, int* csize, unsigned char* ubuf, int* usize, int mode)
{
   static char alphabet[16] = {"*b~de,gh&j�lrn=p"};

   DENTER(TOP_LAYER, "change_encoding");

   if (mode == ENCODE_TO_STRING) {
      /*
      ** encode to string
      */
      int i, j;
      int enclen = *usize;
      if ((*csize) < (2*enclen+1)) {
         DEXIT;
         return FALSE;
      }

      for (i=0,j=0; i<enclen; i++) {
         cbuf[j++] = alphabet[HIQUAD(ubuf[i])];
         cbuf[j++] = alphabet[LOQUAD(ubuf[i])];
      }
      cbuf[j] = '\0';
   }

   if (mode == DECODE_FROM_STRING) {
      /*
      ** decode from string
      */
      char *p;
      int declen;
      if ((*usize) < (*csize)) {
         DEXIT;
         return FALSE;
      }
      for (p=cbuf, declen=0; *p; p++, declen++) {
         int hi, lo, j;
         for (j=0; j<16; j++) {
            if (*p == alphabet[j]) 
               break;
         }
         hi = j;
         p++;
         for (j=0; j<16; j++) {
            if (*p == alphabet[j]) 
               break;
         }
         lo = j;
         ubuf[declen] = (unsigned char) SETBYTE(hi, lo);
      }   
      *usize = declen;
   }
      
   DEXIT;   
   return TRUE;
}   

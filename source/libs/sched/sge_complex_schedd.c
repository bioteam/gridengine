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
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
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
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#ifdef WIN32NATIVE
#	include "win32nativetypes.h"
#endif

#include "sge_sched.h"
#include "sgermon.h"
#include "sge_log.h"
#include "sge_parse_num_par.h"
#include "sge_answer.h"
#include "sge_language.h"
#include "sge_string.h"
#include "sge_hostname.h"
#include "sge_schedd_conf.h"
#include "sge_qinstance.h"
#include "sge_host.h"
#include "sge_object.h"
#include "sge_ulong.h"
#include "sge_centry.h"
#include "sge_load.h"

#include "msg_common.h"
#include "msg_schedd.h"

#include "sge_complex_schedd.h"
#include "sge_resource_utilization.h"

static int resource_cmp(u_long32 relop, double req_all_slots, double src_dl);

static int string_cmp(u_long32 type, u_long32 relop, const char *request,
                      const char *offer);

static lList *get_attribute_list_by_names(lListElem *global, lListElem *host, lListElem *queue, 
         const lList *centry_list, const char** attrnames, int max_names);

static void build_name_filter(const char **filter, lList *list, int t_name, int *pos);

static bool is_attr_prior2(lListElem *upper_el, double lower_value, int t_value, int t_dominant );

static lList *get_attribute_list(lListElem *global, lListElem *host, lListElem *queue, const lList *centry_list);


void monitor_dominance(
char *str,
u_long32 mask 
) {
   switch (mask & DOMINANT_LAYER_MASK) {
   case DOMINANT_LAYER_GLOBAL:   
      *str++ = 'g';
      break;
   case DOMINANT_LAYER_HOST:   
      *str++ = 'h';
      break;
   case DOMINANT_LAYER_QUEUE:   
      *str++ = 'q';
      break;
   default:
      *str++ = '?';
      break;
   }

   switch (mask & DOMINANT_TYPE_MASK) {
   case DOMINANT_TYPE_VALUE:   
      *str++ = 'v';
      break;
   case DOMINANT_TYPE_FIXED:   
      *str++ = 'f';
      break;
   case DOMINANT_TYPE_LOAD:   
      *str++ = 'l';
      break;
   case DOMINANT_TYPE_CLOAD:   
      *str++ = 'L';
      break;
   case DOMINANT_TYPE_CONSUMABLE:   
      *str++ = 'c';
      break;
   default:
      *str++ = '?';
      break;
   }

   *str++ = '\0';
}

/****** sge_select_queue/get_attribute() ***************************************
*  NAME
*     get_attribute() -- looks for an attribut, but only for one level (for host, global, or queue)  
*
*  SYNOPSIS
*     static lListElem* get_attribute(const char *attrname, lList *config_attr, 
*     lList *actual_attr, lList *load_attr, lList *centry_list, lListElem 
*     *queue, lListElem *rep, u_long32 layer, double lc_factor, dstring *reason) 
*
*  FUNCTION
*     Extracts the attribut specified with 'attrname' and finds the 
*     more important one, if it is defined multiple times on the same 
*     level. It only cares about one level.
*     If the attribute is a consumable, one can specify a point in time and a duration.
*     This will get the caller the min amount of that resource during the time frame.
*
*  INPUTS
*     const char *attrname - attribute name one is looking for 
*     lList *config_attr   - user defined attributes (CE_Type)
*     lList *actual_attr   - current usage of consumables (RUE_Type)
*     lList *load_attr     - load attributes 
*     lList *centry_list   - the system wide attribute configuration 
*     lListElem *queue     - the current queue, or null, if one works on hosts 
*     u_long32 layer       - the current layer 
*     double lc_factor     - the load correction value 
*     dstring *reason      - space for error messages or NULL 
*     bool zero_utilization - ???
*     u_long32 start_time  - begin of the time interval, one asks for the resource
*     u_long32 duration    - the duration the interval
*
*  RESULT
*     static lListElem* - the element one was looking for or NULL
*
*******************************************************************************/
lListElem* get_attribute(const char *attrname, lList *config_attr, lList *actual_attr, lList *load_attr, 
   const lList *centry_list, lListElem *queue, u_long32 layer, double lc_factor, dstring *reason,
   bool zero_utilization, u_long32 start_time, u_long32 duration)
{
   lListElem *actual_el=NULL;
   lListElem *load_el=NULL;
   lListElem *cplx_el=NULL;

   DENTER(BASIS_LAYER, "get_attribute");

   /* resource_attr is a complex_entry (CE_Type) */
   if (config_attr) {
      lListElem *temp = lGetElemStr(config_attr, CE_name, attrname);

      if(temp){ 

         cplx_el = lCopyElem(lGetElemStr(centry_list, CE_name, attrname));
         if(!cplx_el){
            /* error */
            DEXIT;
            return NULL;
         }
         lSetUlong(cplx_el, CE_dominant, layer | DOMINANT_TYPE_FIXED);
         lSetUlong(cplx_el, CE_pj_dominant, DOMINANT_TYPE_VALUE);  /* default, no value set */ 
         lSetDouble(cplx_el, CE_doubleval, lGetDouble(temp,CE_doubleval) ); 
         lSetString(cplx_el, CE_stringval, lGetString(temp,CE_stringval) ); 
      }
   }

   if(cplx_el && lGetBool(cplx_el, CE_consumable)) {
      lSetUlong(cplx_el, CE_pj_dominant, layer | DOMINANT_TYPE_CONSUMABLE );
      lSetUlong(cplx_el, CE_dominant,DOMINANT_TYPE_VALUE );
      /* treat also consumables as fixed attributes when assuming an empty queuing system */
      if (sconf_get_qs_state() == QS_STATE_FULL) {
         if (actual_attr && (actual_el = lGetElemStr(actual_attr, RUE_name, attrname))){
            dstring ds;
            char as_str[20];
            double utilized = zero_utilization ? 0 : utilization_max(actual_el, start_time, duration);

            switch (lGetUlong(cplx_el, CE_relop)) {
               case CMPLXGE_OP:
               case CMPLXGT_OP:
                     lSetDouble(cplx_el, CE_pj_doubleval, utilized); 
               break;

               case CMPLXEQ_OP:
               case CMPLXLT_OP:
               case CMPLXLE_OP:
               case CMPLXNE_OP:
               default:
                     lSetDouble(cplx_el, CE_pj_doubleval, lGetDouble(cplx_el,CE_doubleval) - utilized); 
                  break;
            }
            sge_dstring_init(&ds, as_str, sizeof(as_str));
            sge_dstring_sprintf(&ds, "%8.3f", (float)lGetDouble(cplx_el, CE_pj_doubleval));
            lSetString(cplx_el,CE_pj_stringval, as_str);
         } else{
            sge_dstring_sprintf(reason, MSG_ATTRIB_ACTUALELEMENTTOATTRIBXMISSING_S, attrname);
            cplx_el = lFreeElem(cplx_el);
            DEXIT;
            return NULL;
         }
      } else{
         lSetDouble(cplx_el, CE_pj_doubleval, lGetDouble(cplx_el, CE_doubleval)); 
         lSetString(cplx_el,CE_pj_stringval, lGetString(cplx_el, CE_stringval));
      }
   }

   /** check for a load value */
   if ( load_attr && 
        (sconf_get_qs_state()==QS_STATE_FULL || sge_is_static_load_value(attrname)) &&
        (load_el = lGetElemStr(load_attr, HL_name, attrname)) &&
        (!is_attr_prior(cplx_el, cplx_el))
      ) {
         lListElem *ep_nproc=NULL;
         int nproc=1;

         if (!cplx_el){
            cplx_el = lCopyElem(lGetElemStr(centry_list, CE_name, attrname));
               if(!cplx_el){
                  /* error */
                  return NULL;
               }         
            lSetUlong(cplx_el, CE_dominant, DOMINANT_TYPE_VALUE);
            lSetUlong(cplx_el, CE_pj_dominant, DOMINANT_TYPE_VALUE);
         }

         if ((ep_nproc = lGetElemStr(load_attr, HL_name, LOAD_ATTR_NUM_PROC))) {
            const char *cp = lGetString(ep_nproc, HL_value);
            if (cp)
               nproc = MAX(1, atoi(lGetString(ep_nproc, HL_value)));
         }

         {
            const char *load_value=NULL;
            u_long32 type;
            double dval;

            load_value = lGetString(load_el, HL_value);

            /* are we working on string values? if though, than it is easy */
            if ( (type = lGetUlong(cplx_el, CE_valtype)) == TYPE_STR || type == TYPE_CSTR || type == TYPE_HOST || type == TYPE_RESTR) {
               lSetString(cplx_el, CE_stringval, load_value);
               lSetUlong(cplx_el, CE_dominant, layer | DOMINANT_TYPE_LOAD);
            }
            else { /* working on numerical values */
               lListElem *job_load;
               char err_str[256];
               char sval[100];
               u_long32 dom_type = DOMINANT_TYPE_LOAD;
 
               job_load=lGetElemStr(sconf_get_job_load_adjustments(), CE_name, attrname);
               if (parse_ulong_val(&dval, NULL, type, load_value, NULL, 0)) {

               strcpy(sval, load_value);
               /* --------------------------------
                  look for 'name' in our load_adjustments list
               */
               if (job_load) {
                  const char *s;
                  double load_correction;

                  s = lGetString(job_load, CE_stringval);
                  if (!parse_ulong_val(&load_correction, NULL, type, s, err_str, 255)) {
                     ERROR((SGE_EVENT, MSG_SCHEDD_LOADADJUSTMENTSVALUEXNOTNUMERIC_S , attrname));
                  }
                  else if (lc_factor) {
                     double old_dval;
                     u_long32 relop;
                     if (!strncmp(attrname, "np_", 3) && nproc != 1 ) {
                        DPRINTF(("fillComplexFromHost: dividing lc_factor for \"%s\" with value %f by %d to %f\n",
                                 attrname, lc_factor, nproc, lc_factor / nproc));
                        lc_factor /= nproc;
                     }
                     load_correction *= lc_factor;

                     /* it depends on relop in complex config whether load_correction is pos/neg */
                     if ( (relop = lGetUlong(cplx_el, CE_relop)) == CMPLXGE_OP || relop == CMPLXGT_OP){
                        old_dval = dval;
                        dval += load_correction;
                     }   
                     else{
                        old_dval = dval;
                        dval -= load_correction;
                     }

                     sprintf(sval, "%8.3f", dval);
                     DPRINTF(("%s: uc: %f c(%f): %f\n", attrname, old_dval, lc_factor, dval));
                     dom_type = DOMINANT_TYPE_CLOAD;
                  }
               }

               /* we can have a user, who wants to override the incomming load value. This is no
                  problem for consumables, but for fixed values. A custom fixed value is a per
                  slot value (stored in CE_doubleval) and a load value is a per job value (stored
                  in CE_doubleval). 
   
                  This code changes a fixed custom value from a per slot to a per job value!!
               */
               if ( !(lGetUlong(cplx_el, CE_dominant) == DOMINANT_TYPE_VALUE) && 
                     (lGetUlong(cplx_el, CE_pj_dominant) == DOMINANT_TYPE_VALUE)){
                  lSetDouble(cplx_el, CE_pj_doubleval, lGetDouble(cplx_el, CE_doubleval));
                  lSetString(cplx_el, CE_pj_stringval, lGetString(cplx_el, CE_stringval));
                  lSetUlong(cplx_el, CE_dominant, DOMINANT_TYPE_VALUE);
                  lSetUlong(cplx_el, CE_pj_dominant, layer | DOMINANT_TYPE_FIXED);
               } 
 
               if (!is_attr_prior2(cplx_el, dval, CE_pj_doubleval, CE_pj_dominant)){
                  lSetString(cplx_el, CE_pj_stringval, load_value);
                  lSetUlong(cplx_el, CE_pj_dominant, layer | dom_type);
                  lSetDouble(cplx_el, CE_pj_doubleval, dval );
               }
            } /* end numerical load value */
         }/* end block */
      }
   }

   /* we are working on queue level, so we have to check for queue resource values */
   if (queue){
      bool created=false;
      if(!cplx_el){
         cplx_el = lCopyElem(lGetElemStr(centry_list, CE_name, attrname));
         if(!cplx_el){
            /* error */
            return NULL;
         }         
         lSetUlong(cplx_el, CE_dominant, DOMINANT_TYPE_VALUE);
         lSetUlong(cplx_el, CE_pj_dominant, DOMINANT_TYPE_VALUE);
         created = true;
      }
      if (!get_queue_resource(cplx_el, queue, attrname) && created)
         cplx_el = lFreeElem(cplx_el);
   }
   DEXIT;
   return cplx_el;
}


/****** sge_select_queue/get_queue_resource() ***************************************
*  NAME
*     get_queue_resource() -- extracts attribut information from the queue 
*
*  SYNOPSIS
*    static lListElem* get_queue_resource(lListElem *queue, lList *centry_list, const char *attrname)
* 
*  FUNCTION
*     All fixed queue attributes are directly coded into the queue structure. These have to extraced
*     and formed into a CE structure. That is, what this function does. It takes a name for an attribut
*     and returns a full CE structure, if the attribut is set in the queue. Otherwise it returns NULL.
*
*  INPUTS
*     lListElem *queue_elem - 
*     lListElm  *queue      -
*     const char *attrname  - name of the attribute.
*  RESULT
*     bool -  
*
*
*******************************************************************************/
bool get_queue_resource(lListElem *queue_elem, const lListElem *queue, const char *attrname){
   double dval=0.0;
   const char *value=NULL;
   char as_str[100];
   int type;
   int pos = 0;

      DENTER(BASIS_LAYER, "get_queue_resource");

      if(!queue_elem){
         /* error */
         return false;
      }

      { /* test, if its a valid queue resource */
         bool found = false;
         for(; pos < max_queue_resources; pos++)
            if(strcmp(attrname, queue_resource[pos].name) == 0){
               found=true;
               break;
            }
               
         if(!found){
            DPRINTF(("is not a system queue attribute: %s\n", attrname));
            DEXIT;
            return false;            
         }
      }

      /* read stuff from queue and set to new elements */
      type = queue_resource[pos].type;
      switch(type) {
      case TYPE_INT:
         dval = (double)lGetUlong(queue, queue_resource[pos].field);
         sprintf(as_str, u32, lGetUlong(queue, queue_resource[pos].field));
         break;

      case TYPE_TIM:
      case TYPE_MEM:
      case TYPE_DOUBLE:
         if ((value = lGetString(queue, queue_resource[pos].field))) {
            parse_ulong_val(&dval, NULL, type, value, NULL, 0); 
         } 
         break;

      case TYPE_BOO:
         dval = (double)lGetBool(queue, queue_resource[pos].field);
         sprintf(as_str, "%d", (int)lGetBool(queue, queue_resource[pos].field));
         break;

      case TYPE_STR: 
      case TYPE_CSTR:
      case TYPE_RESTR:
         value = lGetString(queue, queue_resource[pos].field);
         break;
      case TYPE_HOST:
         value = lGetHost(queue, queue_resource[pos].field);
         break;
      }
     
 
      if(value || as_str){
         if (!is_attr_prior2(queue_elem, dval, CE_doubleval, CE_dominant)){
            lSetUlong(queue_elem,CE_dominant , DOMINANT_LAYER_QUEUE|DOMINANT_TYPE_FIXED);
            lSetDouble(queue_elem,CE_doubleval , dval);

            if(value){
               lSetString(queue_elem, CE_stringval, value);
            } 
            else{
               lSetString(queue_elem,CE_stringval , as_str);
            }
         }
      }
      else{ 
         queue_elem = lFreeElem(queue_elem);
         DPRINTF(("the sytem queue element %s has no value!\n", attrname));
         /* error */
      }
      DEXIT;
      return true;
}


/****** sge_select_queue/is_attr_prior() ***************************************
*  NAME
*     is_attr_prior() -- compares two attribut instances with each other 
*
*  SYNOPSIS
*     static bool is_attr_prior(lListElem *upper_el, lListElem *lower_el) 
*
*  FUNCTION
*     checks if the first given attribut instance has a higher priority than
*     second instance.
*     if the first is NULL, it returns false
*     if the second or the second and first is NULL, it returns true
*     if the "==" or "!=" operators are used, it is true
*     if both are the same, it returns false. 
*     otherwise it computes the minimum or maximum between the values. 
*
*  INPUTS
*     lListElem *upper_el - attribut, which shouldb e overridden by the second one. 
*     lListElem *lower_el - attribut, which want to override the first one. 
*
*  RESULT
*     static bool - true, when the first attribut has a higher priority.
*
*******************************************************************************/
bool is_attr_prior(lListElem *upper_el, lListElem *lower_el){
   u_long32 relop;
   bool ret;
   double upper_value;
   double lower_value;
   u_long32 dom;
   u_long32 used_dom;
   u_long32 used_dom_val;
   u_long32 used_dom_str;

   u_long32 unused_dom_val;
   u_long32 unused_dom_str;
   u_long32 unused_dom;

   DENTER(BASIS_LAYER, "is_attr_prior");

   /* the order is important must not be changed */   
   if(!upper_el){
      DEXIT;
      return false;
   }
   if(!lower_el){
      DEXIT;
      return true;
   }

   relop = lGetUlong(upper_el, CE_relop);
   if ((relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
      DEXIT;
      return true;
  }

   /* if both elements are the same, than I can not say which one is more important */
   if(upper_el == lower_el)
      return false;

   if ((dom = lGetUlong(upper_el, CE_pj_dominant)) == 0 || (dom & DOMINANT_TYPE_VALUE) ){
      used_dom_val = CE_doubleval;
      used_dom_str = CE_stringval;      
      used_dom = CE_dominant;

      unused_dom_val = CE_pj_doubleval;
      unused_dom_str= CE_pj_stringval;
      unused_dom = CE_pj_dominant;
   }
   else {
      used_dom_val = CE_pj_doubleval;
      used_dom_str = CE_pj_stringval;      
      used_dom = CE_pj_dominant;

      unused_dom_val = CE_doubleval;
      unused_dom_str = CE_stringval;
      unused_dom = CE_dominant;
   }

   if ((dom = lGetUlong(lower_el, used_dom)) == 0 || (dom & DOMINANT_TYPE_VALUE) ){
      lSetDouble(lower_el, used_dom_val, lGetDouble(lower_el, unused_dom_val));
      lSetString(lower_el, used_dom_str, lGetString(lower_el, unused_dom_str));
      lSetUlong(lower_el, used_dom, lGetUlong(lower_el, unused_dom));
      lSetUlong(lower_el, unused_dom, DOMINANT_TYPE_VALUE);
   }

   upper_value = lGetDouble(upper_el, used_dom_val);
   lower_value = lGetDouble(lower_el, used_dom_val);

   if (relop == CMPLXGE_OP || relop == CMPLXGT_OP ){
      ret = upper_value >= lower_value; 
   }
   else{
      ret =  upper_value <= lower_value; 
   }

   DEXIT;
   return ret;
}

/****** sge_complex_schedd/is_attr_prior2() ************************************
*  NAME
*     is_attr_prior2() -- checks, if the set value in the structure has a higher priority
*     than the new one 
*
*  SYNOPSIS
*     static bool is_attr_prior2(lListElem *upper_el, double lower_value, int 
*     t_value, int t_dominant) 
*
*  FUNCTION
*     Computes the priority between a given structure and its values and a new value. This
*     is done on some basic rules. If the value is not set (dominant ==  DOMINANT_TYPE_VALUE)
*     or which relational opperator is used. If this is not enough, the two values are compared
*     and based on the opperator, it returns a true or false:
*     if no value is set in the structure: false
*     if the relops are == or != : true
*     if the relops are >= or > : true, when the new value is smaller than the old one
*     if the relops are <= or < : true, when the new value is bigger than the old one
*
*  INPUTS
*     lListElem *upper_el - target structure 
*     double lower_value  - new value 
*     int t_value         - which field to use (CE_doubleval or CE_pj_doubleval) 
*     int t_dominant      - which dominant field to use (CE_dominant, CE_pj_dominant) 
*
*  RESULT
*     static bool - true, if the value in the structure has the higher priority 
*
*******************************************************************************/
static bool is_attr_prior2(lListElem *upper_el, double lower_value, int t_value, int t_dominant ){
   u_long32 relop;
   u_long32 dom;
   bool ret;
   double upper_value;

   DENTER(BASIS_LAYER, "is_attr_prior2");

   if ((dom = lGetUlong(upper_el, t_dominant)) == 0 || (dom & DOMINANT_TYPE_VALUE) ){
      DEXIT; 
      return false;
   }

   relop = lGetUlong(upper_el, CE_relop);
   if ((relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
      DEXIT;
      return true;
   }

   upper_value = lGetDouble(upper_el, t_value); 

   if (relop == CMPLXGE_OP || relop == CMPLXGT_OP ){
      ret = upper_value >= lower_value;
   }
   else{
      ret = upper_value <= lower_value;
   }
   DEXIT;
   return ret;
}

/* provide a list of attributes containing all global attributes */
int global_complexes2scheduler(
lList **new_centry_list,
lListElem *global_host,
lList *centry_list
) {
   DENTER(TOP_LAYER, "global_complexes2scheduler");

   if(*new_centry_list)
      *new_centry_list = lFreeList(*new_centry_list);

   *new_centry_list = get_attribute_list(global_host, NULL, NULL, centry_list);

   DEXIT;
   return 0;
}


/* provide a list of attributes containing all attributes for the given host */
int host_complexes2scheduler(
lList **new_centry_list,
lListElem *host,
lList *exechost_list,
lList *centry_list
) {
   DENTER(TOP_LAYER, "host_comlexes2scheduler");

   if (!host) {
      DPRINTF(("!!missing host!!\n"));
   }
   /* build global complex and add it to result */
   if ( *new_centry_list) 
      *new_centry_list = lFreeList(*new_centry_list);

   *new_centry_list = get_attribute_list(host_list_locate(exechost_list, "global"), host, NULL, centry_list);

   DEXIT;
   return 0;
}

/**********************************************************************
 This is the function used by the scheduler to get a complete list of 
 complexes for a given queue.
 All templates are filled by actual values.
 With this list the scheduler can decide whether a request matches this
 queue.

 **********************************************************************/
int queue_complexes2scheduler(
lList **new_centry_list,
lListElem *queue,
const lList *exechost_list,
const lList *centry_list 
) {
   DENTER(BASIS_LAYER, "queue_complexes2scheduler");

   if (*new_centry_list) 
      *new_centry_list = lFreeList(*new_centry_list);

   *new_centry_list = get_attribute_list(host_list_locate(exechost_list, "global"), 
                                         queue ? host_list_locate(exechost_list, lGetHost(queue, QU_qhostname)) : NULL, 
                                         queue, centry_list);
   DEXIT;
   return 0;
}

/****** sge_complex_schedd/get_attribute_list_by_names() ***********************
*  NAME
*     get_attribute_list_by_names() -- generates a list of attributes from the given names 
*
*  SYNOPSIS
*     static lList* get_attribute_list_by_names(lListElem *global, lListElem 
*     *host, lListElem *queue, lList *centry_list, const char** attrnames, int 
*     max_names) 
*
*  FUNCTION
*     Assembles a list of attributes for a given queue, host, global, which contains all 
*     the specified elements. The general sort order is, global, host, queue. If an 
*     element could not be found, it will not exist. If no elements exist, the function
*     will return NULL 
*
*  INPUTS
*     lListElem *global      - global host 
*     lListElem *host        - host (or NULL, if only global resources are asked for ) 
*     lListElem *queue       - queue (or NULL, if only global / host resources are asked for) 
*     lList *centry_list     - the system wide attribut config list 
*     const char** attrnames - an array of attribute names 
*     int max_names          - nr of attribute names 
*
*  RESULT
*     static lList* - a CULL list of elements or NULL
*
*******************************************************************************/
static lList *get_attribute_list_by_names(lListElem *global, lListElem *host, 
                                          lListElem *queue, const lList *centry_list,
                                          const char** attrnames, int max_names){
   lListElem *attr;
   lList * list = NULL;
   int i;

   for (i=0; i<max_names; i++){
      attr = get_attribute_by_name(global, host, queue, attrnames[i], centry_list, DISPATCH_TIME_NOW, 0);
      if (attr) {
         if (!list )
            list = lCreateList("attr", CE_Type);
         lAppendElem(list, attr);
      }
   }

   return list;
}



/****** sge_complex_schedd/get_attribute_list() ********************************
*  NAME
*     get_attribute_list() -- generates a list for all defined elements in a queue, host, global 
*
*  SYNOPSIS
*     static lList* get_attribute_list(lListElem *global, lListElem *host, 
*     lListElem *queue, lList *centry_list) 
*
*  FUNCTION
*     Generates a list for all attributes defined at the given queue, host, global. 
*
*  INPUTS
*     lListElem *global  - global host 
*     lListElem *host    - host (or NULL, if only global attributes are important) 
*     lListElem *queue   - queue (or NULL if only host/global attributes are important) 
*     lList *centry_list - system wide attribute config list 
*
*  RESULT
*     static lList* - list of attributes or NULL, if no attributes exist.
*
*******************************************************************************/
static lList *get_attribute_list(lListElem *global, lListElem *host, lListElem *queue, const lList *centry_list){
   int pos = 0;
   const char **filter=NULL; 
   lList *list=NULL;

   int size = lGetNumberOfElem(centry_list) + max_queue_resources ; 
   
   DENTER(BASIS_LAYER, "get_attribute_list");
   
   if (global)
      size += lGetNumberOfElem(lGetList(global, EH_load_list));
   if (host)
      size +=  lGetNumberOfElem( lGetList(host, EH_load_list));

   filter = malloc(size * sizeof(char*)); 
   memset(filter, 0, size * sizeof(char*));

   if (global){
      build_name_filter(filter, lGetList(global, EH_load_list), HL_name, &pos);
      build_name_filter(filter, lGetList(global, EH_consumable_config_list), CE_name, &pos);
   }    

   if (host){
      build_name_filter(filter, lGetList(host, EH_load_list), HL_name, &pos);
      build_name_filter(filter, lGetList(host, EH_consumable_config_list), CE_name, &pos);
   } 

   if (queue){ 
      int x=0;  
      for (; x < max_queue_resources; x++){
         bool add = true; 
         int i;

         if (pos >= size){
            ERROR((SGE_EVENT,MSG_COMPLEX_MISSING)); 
            break;
         }

         for(i=0 ; i<pos; i++){ /* prefent adding duplicates */
            if(strcmp(filter[i], queue_resource[x].name) == 0){
               add = false;
               break;
            }
         }
         if(add)
           filter[pos++] = queue_resource[x].name;
      }

      build_name_filter(filter, lGetList(queue, QU_consumable_config_list), CE_name, &pos);
   }


   list = get_attribute_list_by_names(global, host, queue, centry_list, filter, pos);

   free(filter);
   filter = NULL;
   
   DEXIT;
   return list; 

}

/****** sge_complex_schedd/build_name_filter() *********************************
*  NAME
*     build_name_filter() -- fills in an array with complex nams, which can be used
*                            as a filter. 
*
*  SYNOPSIS
*     void build_name_filter(const char **filter, lList *list, int t_name, int 
*     *pos) 
*
*  FUNCTION
*     Takes an array of a given size and fills in complex names. 
*
*  INPUTS
*     const char **filter     - target for the filter strings. It has to be of sufficant size. 
*     lList *list             - a list of complexes, from which the names are extracted 
*     int t_name              - specifies the field which is used as a name 
*     int *pos                - current position in which the new strings are added. After a 
*                               run it points to the next empty spot in the array. 
*
*  NOTES
*     ??? 
*
*******************************************************************************/
static void build_name_filter(const char **filter, lList *list, int t_name, int *pos){
   lListElem *current = NULL;
   
   if(!list)
      return;

   for_each( current,list){
         const char* name = lGetString(current, t_name);
         bool add = true; 
         int i;

         for(i=0 ; i<(*pos); i++){
            if(strcmp(filter[i], name) == 0){
               add = false;
               break;
            }
         }
         
         if(add)
            filter[(*pos)++] = name; 
      }
}

/* wrapper for strcmp() of all string types */ 
/* s1 is the pattern */
/* s2 the string that should be matched against the pattern */
int string_base_cmp(u_long32 type, const char *s1, const char *s2)
{
   int match=0;

   switch(type){
      case TYPE_STR: match = strcmp(s1, s2);
         break;
      case TYPE_CSTR:  match = strcasecmp(s1, s2);
         break;
      case TYPE_HOST:  match = sge_hostcmp(s1, s2);
         break;
      case TYPE_RESTR:  {
                           char *s = NULL;
                           struct saved_vars_s *context=NULL;
                           for (s=sge_strtok_r(s1, "|", &context); s; s=sge_strtok_r(NULL, "|", &context)) {
                              if ((match = fnmatch(s, s2, 0)) == 0) {
                                 break;
                              }   
                           }
                        }
         break;
      default: match = -1;
   }

   return match;
}

/* compare string type attributes under consideration of relop */
static int string_cmp( u_long32 type, u_long32 relop, const char *request,
const char *offer) {
   int match;

   switch(relop) {
   case CMPLXEQ_OP:
      match = (string_base_cmp(type, request, offer) == 0);
      break;
   case CMPLXLE_OP :
      match = (string_base_cmp(type, request, offer) <= 0);
      break;
   case CMPLXLT_OP :
      match = (string_base_cmp(type, request, offer) < 0);
      break;
   case CMPLXGT_OP :
      match = (string_base_cmp(type, request, offer) > 0);
      break;
   case CMPLXGE_OP :
      match = (string_base_cmp(type, request, offer) >= 0);
      break;
   case CMPLXNE_OP :
      match = (string_base_cmp(type, request, offer) != 0);
      break;
   default:
      match = 0; /* default -> no match */
   }

   return match;
}

static int resource_cmp(
u_long32 relop,
double req,
double src_dl
) {
   int match;

   DENTER(CULL_LAYER, "resource_cmp");

   switch(relop) {
   case CMPLXEQ_OP :
      match = ( req==src_dl);
      break;
   case CMPLXLE_OP :
      match = ( req<=src_dl);
      break;
   case CMPLXLT_OP :
      match = ( req<src_dl);
      break;
   case CMPLXGT_OP :
      match = ( req>src_dl);
      break;
   case CMPLXGE_OP :
      match = ( req>=src_dl);
      break;
   case CMPLXNE_OP :
      match = ( req!=src_dl);
      break;
   default:
      match = 0; /* default -> no match */
   }

   DPRINTF((" %f %s %f -> match = %d\n", req, map_op2str(relop), src_dl, match));

   DEXIT;
   return match;
}

/*********************************************************************
 compare two complex entries (attributes)
 the type is given by the first complex
 return 1 if matched anything else 0 if not
 *********************************************************************/
int compare_complexes(
int slots,
lListElem *req_cplx,
lListElem *src_cplx,
char *availability_text,
int is_threshold,
int force_existence 
) {
   u_long32 type, relop, used_relop = 0;
   double req_dl, src_dl;
   int match, m1, m2;
   const char *s;
   const char *name;
   const char *offer;
   char dom_str[5];
   char availability_text1[2048];
   char availability_text2[2048]; 
   dstring resource_string = DSTRING_INIT;

   DENTER(TOP_LAYER,"compare_complexes");

   name = lGetString(src_cplx, CE_name); 
   type = lGetUlong(src_cplx, CE_valtype);
   relop = lGetUlong(src_cplx, CE_relop);


   if (is_threshold) {
      switch (relop) {
      case CMPLXLE_OP:
         used_relop = CMPLXGT_OP;
         break;
      case CMPLXGT_OP:
         used_relop = CMPLXLE_OP;
         break;
      case CMPLXLT_OP:
         used_relop = CMPLXGE_OP;
         break;
      case CMPLXGE_OP:
         used_relop = CMPLXLT_OP;
         break;
      case CMPLXNE_OP:
      case CMPLXEQ_OP:
      default:
         used_relop = relop;
         break;
      }
   } else {
      used_relop = relop ;
   }
   switch (type) {
      const char *request;
      double req_all_slots;

   case TYPE_STR:
   case TYPE_CSTR:
   case TYPE_HOST:
   case TYPE_RESTR:
      request = lGetString(req_cplx, CE_stringval);

      offer = lGetString(src_cplx, CE_stringval);
      monitor_dominance(dom_str, lGetUlong(src_cplx, CE_dominant));
#if 0
      DPRINTF(("%s(\"%s\", \"%s\")\n", type==TYPE_STR?"strcmp":"strcasecmp",
            request, offer)); 
#endif
      match = string_cmp(type, used_relop, request, offer);
      sprintf(availability_text, "%s:%s=%s", dom_str, name, offer);
#if 0
      DPRINTF(("-l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
            name, request, dom_str, name, map_op2str(relop),
            offer, match?"ok":"no match"));
#endif
      DEXIT;
      return match;

   case TYPE_INT:
   case TYPE_TIM:
   case TYPE_MEM:
   case TYPE_BOO:
   case TYPE_DOUBLE:
      s=lGetString(req_cplx, CE_stringval); 
      if (!parse_ulong_val(&req_dl, NULL, type, s, NULL, 0)) {
#if 0
         DPRINTF(("%s is not of type %s\n", s, map_type2str(type)));
#endif
         req_dl = 0;
      }   

      if (is_threshold)
         m1 = m2 = 0; /* nothing exceeded per default */
      else
         m1 = m2 = 1; /* matched per default */
      /* is there a per job limit */
      if (!(lGetUlong(src_cplx, CE_pj_dominant) & (DOMINANT_TYPE_VALUE))) {
         /* Actually request matching for utilization attributes 
            fails if request is higher than what we get as per job 
            limit also for attributes marked as consumable.
        
            In future we could say that the actual value (being a load 
            value or a consumable resource value) plus the jobs request 
            may not exceed the maximal utilization from (global) host 
            or queue - but only if the attributes marked as consumable. */
         src_dl = lGetDouble(src_cplx, CE_pj_doubleval);
         
         req_all_slots = req_dl*slots;

         m1 = resource_cmp(used_relop, req_all_slots, src_dl);

         monitor_dominance(dom_str, lGetUlong(src_cplx, CE_pj_dominant));

         switch (type) {
         case TYPE_BOO:
            sge_dstring_copy_string( &resource_string, src_dl?"true":"false");
/*            sprintf(availability_text1, "%s:%s=%s", dom_str, name, src_dl?"true":"false");*/
#if 0
            DPRINTF(("-l %s=%f, Q: %s:%s%s%f, Comparison: %s\n",
                     name, req_dl?"true":"false", dom_str, name,
                     map_op2str(used_relop),
                     sge_dstring_get_string(&resource_string), m1?"ok":"no match"));
#endif
            break;
         case TYPE_MEM:
            double_print_memory_to_dstring(src_dl, &resource_string);
#if 0
            { 
               dstring request_string = DSTRING_INIT;

               double_print_memory_to_dstring(req_dl, &request_string);
               DPRINTF(("%d times of -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                        slots, name, sge_dstring_get_string(&request_string),
                        dom_str, name, map_op2str(used_relop),
                        sge_dstring_get_string(&resource_string), 
                        m1?"ok":"no match"));
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         case TYPE_TIM:
            double_print_time_to_dstring(src_dl, &resource_string);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_time_to_dstring(req_dl, &request_string);
               DPRINTF(("%d times of -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                        slots, name, sge_dstring_get_string(&request_string),
                        dom_str, name, map_op2str(used_relop),
                        sge_dstring_get_string(&resource_string), 
                        m1?"ok":"no match"));
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         default:
            double_print_to_dstring(src_dl, &resource_string);
            break;
         } 
         sprintf(availability_text1, "%s:%s=%s", dom_str, name, sge_dstring_get_string(&resource_string));
      }

      /* is there a per slot limit */
      if (!(lGetUlong(src_cplx, CE_dominant) & (DOMINANT_TYPE_VALUE)) ||     /* per slot set || */
              ((lGetUlong(src_cplx, CE_dominant) & (DOMINANT_TYPE_VALUE)) && /* (per slot not set && */
           (lGetUlong(src_cplx, CE_pj_dominant) & (DOMINANT_TYPE_VALUE)) &&  /* and per job not set) */
            force_existence)) {  

         src_dl = lGetDouble(src_cplx, CE_doubleval);
         req_all_slots = req_dl;
         m2 = resource_cmp(used_relop, req_all_slots, src_dl);

         monitor_dominance(dom_str, lGetUlong(src_cplx, CE_dominant));

         switch (type) {
         case TYPE_BOO:
            sge_dstring_copy_string( &resource_string, src_dl?"true":"false");
#if 0
            DPRINTF(("-l %s=%f, Q: %s:%s%s%f, Comparison: %s\n",
                     name, req_dl?"true":"false", dom_str, name, 
                     map_op2str(used_relop),
                     src_dl?"true":"false", m2?"ok":"no match"));
#endif
            break;
         case TYPE_MEM:
            double_print_memory_to_dstring(src_dl, &resource_string);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_memory_to_dstring(req_dl, &request_string);
               DPRINTF(("per slot -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                        name, sge_dstring_get_string(&request_string),
                        dom_str, name, map_op2str(used_relop),
                        sge_dstring_get_string(&resource_string), 
                        m2?"ok":"no match"));
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         case TYPE_TIM:
            double_print_time_to_dstring(src_dl, &resource_string);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_time_to_dstring(req_dl, &request_string);
               DPRINTF(("per slot -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                        name, sge_dstring_get_string(&request_string),
                        dom_str, name, map_op2str(used_relop),
                        sge_dstring_get_string(&resource_string), 
                        m2?"ok":"no match"));
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         default:
            double_print_to_dstring(src_dl, &resource_string);
            break;
         } 
         sprintf(availability_text2, "%s:%s=%s", dom_str, name, sge_dstring_get_string(&resource_string));
      }
      sge_dstring_free(&resource_string);
      if (is_threshold)
         match = m1 || m2;
      else {
         match = m1 && m2;
         if (!m1) {
            strcpy(availability_text, availability_text1);
         } else if (!m2) {
            strcpy(availability_text, availability_text2);
         } else {
            strcpy(availability_text, "");
         }
      }
      DEXIT;
      return match;

   default:  /* should never reach this -> undefined type */
      *availability_text = '\0';
      break;
   }
   DEXIT;
   return 0;
}

/****** sge_select_queue/get_attribute_by_Name() *******************************
*  NAME
*     get_attribute_by_Name() -- returns an attribut by name 
*
*  SYNOPSIS
*     void lListElem* get_attribute_by_Name(lListElem* global, lListElem *host, 
*     lListElem *queue, const char* attrname, lList *centry_list, char * 
*     reason, int reason_size) 
*
*  FUNCTION
*     It looks into the different configurations on host, global and queue and returns
*     the attribute, which was asked for. It the attribut is defined multiple times, only
*     the valid one is returned. 
*
*  INPUTS
*     lListElem* global    - the global host 
*     lListElem *host      - a given host can be null, than only the  global host is important
*     lListElem *queue     - a queue on the given host, can be null, than only the host and global ist important 
*     const char* attrname - the attribut name one is looking ofr 
*     lList *centry_list   - the system wide attribut config list 
*     char *reason         - memory for the error message 
*     int reason_size      - the max length of an error message 
*
*  RESULT
*     void lListElem* - the element one is looking for (a copy) or NULL.
*
*******************************************************************************/
lListElem *get_attribute_by_name(lListElem* global, lListElem *host, lListElem *queue, 
    const char* attrname, const lList *centry_list, u_long32 start_time, u_long32 duration){
   lListElem *global_el=NULL;
   lListElem *host_el=NULL;
   lListElem *queue_el=NULL;
   lListElem *ret_el = NULL;
   u_long32 ulc_factor = 0;
   double lc_factor = 0;
   lList *load_attr = NULL;
   lList *config_attr = NULL;
   lList *actual_attr = NULL; 

   DENTER(BASIS_LAYER, "get_attribute_by_name");

   if(global){
      load_attr = lGetList(global, EH_load_list);  
      config_attr = lGetList(global, EH_consumable_config_list);
      actual_attr = lGetList(global, EH_resource_utilization);

      /* is there a multiplier for load correction (may be not in qstat, qmon etc) */
      if (lGetPosViaElem(global, EH_load_correction_factor) >= 0) {
         if ((ulc_factor=lGetUlong(global, EH_load_correction_factor)))
            lc_factor = ((double)ulc_factor)/100;
      }
      global_el = get_attribute(attrname, config_attr, actual_attr, load_attr, 
                                centry_list, NULL, DOMINANT_LAYER_GLOBAL, 
                                lc_factor, NULL, false, start_time, duration);
      ret_el = global_el;
   } 

   if(host){
      load_attr = lGetList(host, EH_load_list); 
      config_attr = lGetList(host, EH_consumable_config_list);
      actual_attr = lGetList(host, EH_resource_utilization);

      /* is there a multiplier for load correction (may be not in qstat, qmon etc) */
      if (lGetPosViaElem(host, EH_load_correction_factor) >= 0) {
         if ((ulc_factor=lGetUlong(host, EH_load_correction_factor)))
            lc_factor = ((double)ulc_factor)/100;
      }
      host_el = get_attribute(attrname, config_attr, actual_attr, load_attr, centry_list, NULL, DOMINANT_LAYER_HOST, 
                              lc_factor, NULL, false, start_time, duration);
      if(!global_el && host_el)
         ret_el = host_el;
      else if(global_el && host_el){
         if(is_attr_prior(global_el, host_el)){
            host_el = lFreeElem(host_el);
         }
         else{
            global_el = lFreeElem(global_el);
            ret_el = host_el;
         }
      }
   }

   if(queue){
      config_attr = lGetList(queue, QU_consumable_config_list);
      actual_attr = lGetList(queue, QU_resource_utilization);
      
      queue_el = get_attribute(attrname, config_attr, actual_attr, NULL, centry_list, queue, DOMINANT_LAYER_QUEUE, 
                              0.0, NULL, false, start_time, duration);

      if(!ret_el)
         ret_el = queue_el;
      else if(ret_el && queue_el){
         if(is_attr_prior(ret_el, queue_el)){
            queue_el = lFreeElem(queue_el);
         }
         else{
            ret_el = lFreeElem(ret_el);
            ret_el = queue_el;
         }
      }
   }
   DEXIT;
   return ret_el;
}



#ifdef TEST
/* 
for testing purposes compile (on linux) with:
gcc -Wall -DLINUX -DTEST -o complex complex.c ../LINUX/sge_parse_num_par.o ../LINUX/log.o ../LINUX/utility.o  ../LINUX/pack.o ../LINUX/free.o ../LINUX/io.o ../LINUX/libcull.a ../LINUX/librmon.a
*/

int main(int argc, char *argv[], char *envp[])
{
   lListElem *l;
  
#ifdef __SGE_COMPILE_WITH_GETTEXT__   
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(NULL,NULL);  
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */
 

   if (argc!=3) {
      fprintf(stdout, "usage: complex read_file write_file\n");
      exit(1);
   }

   l = read_cmplx(argv[1], "cmplx_name", NULL);

   write_cmplx(1, argv[2], l, NULL, NULL);

   return 0;
}
#endif


int 
ensure_attrib_available(lList **alpp, lListElem *ep, int nm) 
{
   lListElem *attr;

   DENTER(TOP_LAYER, "ensure_attrib_available");
   for_each (attr, lGetList(ep, nm)) {
      const char *name = lGetString(attr, CE_name);
      lListElem *centry = centry_list_locate(Master_CEntry_List, name);

      if (centry == NULL) {
         ERROR((SGE_EVENT, MSG_GDI_NO_ATTRIBUTE_S, name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_EUNKNOWN;
      }
   }
   DEXIT;
   return 0;
}

int attr_mod_threshold(
lList **alpp,
lListElem *qep,
lListElem *new_ep,
int nm,
int primary_key,
int sub_command,
char *attr_name,
char *object_name 
) {
   int ret;

   DENTER(TOP_LAYER, "attr_mod_threshold");

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm)>=0) {
      lListElem *tmp_elem;

      DPRINTF(("got new %s\n", attr_name));

      tmp_elem = lCopyElem(new_ep); 
      attr_mod_sub_list(alpp, tmp_elem, nm, primary_key, qep,
         sub_command, attr_name, object_name, 0); 

      ret=centry_list_fill_request(lGetList(tmp_elem, nm), Master_CEntry_List, true, false, false);
      if (ret) {
         /* error message gets written by centry_list_fill_request into SGE_EVENT */
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         lFreeElem(tmp_elem);
         DEXIT;
         return STATUS_EUNKNOWN;
      }

      lSetList(new_ep, nm, lCopyList("", lGetList(tmp_elem, nm)));
      lFreeElem(tmp_elem);

      /* check whether this attrib is available due to complex configuration */
      if (ensure_attrib_available(alpp, new_ep, nm)) {
         DEXIT;
         return STATUS_EUNKNOWN;
      }
   }

   DEXIT;
   return 0;
}


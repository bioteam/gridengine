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
#include <Xmt/Xmt.h>
#include <Xmt/InputField.h>
#include <Xmt/Dialogs.h>

#include "sge_gdi.h"
#include "sge_answerL.h"
#include "sge_complexL.h"
#include "sge_hostL.h"
#include "sge_stringL.h"

#include "qmon_rmon.h"
#include "qmon_matrix.h"
#include "qmon_load.h"
#include "qmon_comm.h"
#include "qmon_message.h"
#include "qmon_util.h"

typedef struct _tFFM {
   Widget matrix;
   Widget name;
   Widget value;
} tFFM;


/*-------------------------------------------------------------------------*/
static void qmonLoadAddEntry(Widget matrix, String name);

/*-------------------------------------------------------------------------*/
void qmonLoadSelectEntry(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   XbaeMatrixSelectCellCallbackStruct *cbs = 
            (XbaeMatrixSelectCellCallbackStruct*) cad;
   String str;

   DENTER(GUI_LAYER, "qmonLoadSelectEntry");

   if (cbs->num_params && !strcmp(cbs->params[0], "begin")) {
      /* name */
      str = XbaeMatrixGetCell(w, cbs->row, 0);

      /* value */
      str = XbaeMatrixGetCell(w, cbs->row, 1);
   }
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonLoadAddEntry(
Widget matrix,
String name 
) {
   int rows = 0;
   int row;
   String str;
   String new_row[2];
#if 0
   int num_columns = 2;
   int i;
   lList *cl = NULL;
   lList *ce = NULL;
   lListElem *cep = NULL;
#endif

   DENTER(GUI_LAYER, "qmonLoadAddEntry");

   /*
   ** check input 
   */
   if (is_empty_word(name)) {
      qmonMessageShow(matrix, True, "Name required !\n");
      DEXIT;
      return;
   }

#if 0
   /*
   ** is it a valid load name ?
   */
   cl = qmonMirrorList(SGE_COMPLEX_LIST);
   cep = lGetElemStr(cl, CX_name, "host");
   
   if (cep && name && name[0] != '\0') {
      ce = lGetList(cep, CX_entries);
      cep = lGetElemStr(ce, CE_name, name);
      /*
      ** don't get out of the field
      */
      if (!cep) {
         qmonMessageShow(w, True, "No valid load parameter name !\n");
         DEXIT;
         return;
      }
   }
#endif

   /*
   ** add to attribute matrix, search if item already exists
   */
   rows = XbaeMatrixNumRows(matrix);

   for (row=0; row<rows; row++) {
      /* get name str */
      str = XbaeMatrixGetCell(matrix, row, 0);
      if (!str || (str && !strcmp(str, name)) ||
            (str && is_empty_word(str))) 
         break;
   }
   
/*    if (row <= rows) */
   if (row < rows)
      XbaeMatrixSetCell(matrix, row, 0, name);
   else {
      new_row[0] = name;
      new_row[1] = NULL;
      XbaeMatrixAddRows(matrix, row, new_row, NULL, NULL, 1);
   }

   /* refresh the matrix */
   XbaeMatrixDeselectAll(matrix);
   XbaeMatrixRefresh(matrix);

   /* set value field to edit mode */
   XbaeMatrixEditCell(matrix, row, 1);
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
void ShowLoadNames(
Widget w,
lList *entries 
) {
   lListElem *ep = NULL;
   char stringval[BUFSIZ];
   Boolean status = False;
   String *strs = NULL;
   int n, i;

   DENTER(GUI_LAYER, "ShowLoadNames");

   if (entries) {
      n = lGetNumberOfElem(entries);
      if (n>0) {
         lPSortList(entries, "%I+", CE_name);
         strs = (String*)XtMalloc(sizeof(String)*n);
         for (ep=lFirst(entries), i=0; i<n; ep=lNext(ep), i++) {
           /*
           ** build the strings to display 
           */
           String name = lGetString(ep, CE_name);
/*            String type = lGetUlong(ep, CE_consumable) ? "C" : "F"; */
/*            sprintf(stringval, "%s: %s", type, name ? name : "");  */
           strcpy(stringval, name ? name : ""); 
           strs[i] = XtNewString(stringval);
         }
      }

      strcpy(stringval, "");
      status = XmtAskForItem(w, NULL, "@{Select a load parameter}",
                     "@{Available attributes}", strs, n, False,
                     stringval, sizeof(stringval), NULL);
      /*
      ** don't free referenced strings, they are in the ce list
      */
      StringTableFree(strs, n); 
   }
   else
      XmtDisplayInformationMsg(w, NULL, "@{No load attributes available}", 
                                 NULL, NULL);

   if (status) {
      qmonLoadAddEntry(w, stringval);
   }

   DEXIT;
}


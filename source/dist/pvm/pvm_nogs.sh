#!/bin/sh
# 
#
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://www.gridengine.sunsource.net/license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
##########################################################################
#___INFO__MARK_END__

# sample pvm job 
#
# This script starts a pvm sample with master-slave 
# communication. No group communication is needed.
#
# our name 
#$ -N PVM_NOGS
# pe request
#$ -pe pvm 16-1
#$ -S /bin/sh
#$ -v COMMD_PORT,DISPLAY
# ---------------------------

echo "Got $NSLOTS slots."

/bin/echo Here I am on a $ARC called `hostname`.

trace=
if [ "$1" = "-debug" ]; then
   case "$ARC" in 
   sun4) trace=/usr/bin/trace ;;
   solaris) trace=/usr/bin/truss ;;
   linux) trace=/usr/bin/strace ;;
   *) echo cannot run in debug mode; exit 1 ;;
   esac
   echo using $trace to trace system calls
fi

if [ x$COD_CKPT_ENV = x ]; then
   /bin/echo Running without checkpoint environment
   bin_name=master
else
   /bin/echo Running under checkpoint environment $COD_CKPT_ENV
   bin_name=master_ckpt
fi 

# master starts slave n-1 times 
NSLOTS=`expr $NSLOTS - 1`
echo I will start $NSLOTS slaves.

$trace $CODINE_ROOT/pvm/bin/$ARC/$bin_name $NSLOTS

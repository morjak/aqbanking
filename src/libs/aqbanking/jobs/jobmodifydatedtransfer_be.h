/***************************************************************************
 $RCSfile$
 -------------------
 cvs         : $Id$
 begin       : Mon Mar 01 2004
 copyright   : (C) 2004 by Martin Preuss
 email       : martin@libchipcard.de

 ***************************************************************************
 *          Please see toplevel file COPYING for license details           *
 ***************************************************************************/


#ifndef AQBANKING_JOBMODIFYDATEDTRANSFER_BE_H
#define AQBANKING_JOBMODIFYDATEDTRANSFER_BE_H


#include <aqbanking/jobmodifydatedtransfer.h>


AQBANKING_API
void AB_JobModifyDatedTransfer_SetFieldLimits(AB_JOB *j,
                                              AB_TRANSACTION_LIMITS *limits);

#endif


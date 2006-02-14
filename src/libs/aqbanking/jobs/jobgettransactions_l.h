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


#ifndef AQBANKING_JOBGETTRANSACTIONS_L_H
#define AQBANKING_JOBGETTRANSACTIONS_L_H


#include <aqbanking/job.h>
#include <aqbanking/jobgettransactions.h>

#ifdef __cplusplus
extern "C" {
#endif

AB_JOB *AB_JobGetTransactions_fromDb(AB_ACCOUNT *a, GWEN_DB_NODE *db);
int AB_JobGetTransactions_toDb(const AB_JOB *j, GWEN_DB_NODE *db);

void AB_JobGetTransactions_SetTransactions(AB_JOB *j,
                                           AB_TRANSACTION_LIST2 *tl);
void AB_JobGetTransactions_SetAccountStatusList(AB_JOB *j,
                                                AB_ACCOUNT_STATUS_LIST2 *tl);

#ifdef __cplusplus
}
#endif


#endif /* AQBANKING_JOBGETTRANSACTIONS_L_H */


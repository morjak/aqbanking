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


#ifndef AQBANKING_PROVIDER_H
#define AQBANKING_PROVIDER_H


#include <gwenhywfar/misc.h>
#include <gwenhywfar/misc2.h>
#include <gwenhywfar/inherit.h>
#include <gwenhywfar/xml.h>
#include <gwenhywfar/bufferedio.h>
#include <aqbanking/error.h> /* for AQBANKING_API */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AB_PROVIDER AB_PROVIDER;
GWEN_INHERIT_FUNCTION_LIB_DEFS(AB_PROVIDER, AQBANKING_API)
GWEN_LIST_FUNCTION_LIB_DEFS(AB_PROVIDER, AB_Provider, AQBANKING_API)
/* Do not terminate these lines with semicolon because they are
   macros, not functions, and ISO C89 does not allow a semicolon
   there. */

typedef struct AB_PROVIDER_DESCRIPTION AB_PROVIDER_DESCRIPTION;
GWEN_INHERIT_FUNCTION_LIB_DEFS(AB_PROVIDER_DESCRIPTION, AQBANKING_API)
GWEN_LIST_FUNCTION_LIB_DEFS(AB_PROVIDER_DESCRIPTION, AB_ProviderDescription, AQBANKING_API)
GWEN_LIST2_FUNCTION_LIB_DEFS(AB_PROVIDER_DESCRIPTION, AB_ProviderDescription, AQBANKING_API)

#ifdef __cplusplus
}
#endif


#include <aqbanking/banking.h>
#include <aqbanking/error.h>
#include <aqbanking/job.h>
#include <aqbanking/account.h>
#include <aqbanking/transaction.h>


#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup AB_PROVIDER AB_PROVIDER (Online Banking Backends)
 * @ingroup AB_C_INTERFACE
 *
 * @brief This group represents backends.
 */
/*@{*/


typedef AB_PROVIDER* (*AB_PROVIDER_FACTORY_FN)(AB_BANKING *ab,
                                               GWEN_DB_NODE *db);

/** @name Prototypes For Virtual Functions
 *
 */
/*@{*/
typedef int (*AB_PROVIDER_INIT_FN)(AB_PROVIDER *pro, GWEN_DB_NODE *dbData);
typedef int (*AB_PROVIDER_FINI_FN)(AB_PROVIDER *pro, GWEN_DB_NODE *dbData);

typedef int (*AB_PROVIDER_UPDATEJOB_FN)(AB_PROVIDER *pro, AB_JOB *j);
typedef int (*AB_PROVIDER_ADDJOB_FN)(AB_PROVIDER *pro, AB_JOB *j);
typedef int (*AB_PROVIDER_EXECUTE_FN)(AB_PROVIDER *pro);
typedef AB_ACCOUNT_LIST2* (*AB_PROVIDER_GETACCOUNTLIST_FN)(AB_PROVIDER *pro);
typedef int (*AB_PROVIDER_UPDATEACCOUNT_FN)(AB_PROVIDER *pro, AB_ACCOUNT *a);
typedef int (*AB_PROVIDER_ADDACCOUNT_FN)(AB_PROVIDER *pro, AB_ACCOUNT *a);
typedef int (*AB_PROVIDER_IMPORTTRANSACTIONS_FN)(AB_PROVIDER *pro,
                                                 AB_TRANSACTION_LIST2 *tl,
                                                 GWEN_BUFFEREDIO *bio);

/*@}*/




AQBANKING_API
AB_PROVIDER *AB_Provider_new(AB_BANKING *ab,
                             const char *name);
AQBANKING_API
void AB_Provider_free(AB_PROVIDER *pro);

AQBANKING_API
const char *AB_Provider_GetName(const AB_PROVIDER *pro);
AQBANKING_API
AB_BANKING *AB_Provider_GetBanking(const AB_PROVIDER *pro);


AQBANKING_API
int AB_Provider_IsInit(const AB_PROVIDER *pro);


/** @name Setters For Virtual Functions
 *
 */
/*@{*/
AQBANKING_API
void AB_Provider_SetInitFn(AB_PROVIDER *pro, AB_PROVIDER_INIT_FN f);
AQBANKING_API
void AB_Provider_SetFiniFn(AB_PROVIDER *pro, AB_PROVIDER_FINI_FN f);

AQBANKING_API
void AB_Provider_SetUpdateJobFn(AB_PROVIDER *pro, AB_PROVIDER_UPDATEJOB_FN f);
AQBANKING_API
void AB_Provider_SetAddJobFn(AB_PROVIDER *pro, AB_PROVIDER_ADDJOB_FN f);
AQBANKING_API
void AB_Provider_SetExecuteFn(AB_PROVIDER *pro, AB_PROVIDER_EXECUTE_FN f);
AQBANKING_API
void AB_Provider_SetGetAccountListFn(AB_PROVIDER *pro,
                                     AB_PROVIDER_GETACCOUNTLIST_FN f);
AQBANKING_API
void AB_Provider_SetUpdateAccountFn(AB_PROVIDER *pro,
                                    AB_PROVIDER_UPDATEACCOUNT_FN f);
AQBANKING_API
void AB_Provider_SetAddAccountFn(AB_PROVIDER *pro,
                                 AB_PROVIDER_ADDACCOUNT_FN f);
AQBANKING_API
void AB_Provider_SetImportTransactionsFn(AB_PROVIDER *pro,
                                         AB_PROVIDER_IMPORTTRANSACTIONS_FN f);
/*@}*/


/*@}*/ /* defgroup */

#ifdef __cplusplus
}
#endif




#endif /* AQBANKING_PROVIDER_H */










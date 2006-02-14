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

#ifndef AH_MESSAGE_H
#define AH_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif
typedef struct AH_MSG AH_MSG;
#ifdef __cplusplus
}
#endif

#include <aqhbci/dialog.h>
#include <gwenhywfar/misc.h>
#include <gwenhywfar/keyspec.h>
#include <gwenhywfar/xml.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

GWEN_LIST_FUNCTION_DEFS(AH_MSG, AH_Msg);


/** @name Constructor And Destructor
 *
 */
/*@{*/
AH_MSG *AH_Msg_new(AH_DIALOG *dlg);
void AH_Msg_free(AH_MSG *hmsg);

/*@}*/


/** @name Cryptographic Stuff
 *
 */
/*@{*/
GWEN_KEYSPEC_LIST *AH_Msg_GetSigners(const AH_MSG *hmsg);
int AH_Msg_AddSigner(AH_MSG *hmsg, const GWEN_KEYSPEC *ks);
unsigned int AH_Msg_GetSignerCount(AH_MSG *hmsg);
int AH_Msg_IsSignedBy(const AH_MSG *hmsg, const char *s);
const GWEN_KEYSPEC *AH_Msg_GetCrypter(const AH_MSG *hmsg);
void AH_Msg_SetCrypter(AH_MSG *hmsg, const GWEN_KEYSPEC *ks);
/*@}*/


/** @name Informational Functions
 *
 */
/*@{*/

GWEN_BUFFER *AH_Msg_GetBuffer(AH_MSG *hmsg);
GWEN_BUFFER *AH_Msg_TakeBuffer(AH_MSG *hmsg);
void AH_Msg_SetBuffer(AH_MSG *hmsg, GWEN_BUFFER *bf);

unsigned int AH_Msg_GetMsgNum(const AH_MSG *hmsg);

unsigned int AH_Msg_GetMsgRef(const AH_MSG *hmsg);
void AH_Msg_SetMsgRef(AH_MSG *hmsg,
                           unsigned int i);
unsigned int AH_Msg_GetNodes(AH_MSG *hmsg);

AH_DIALOG *AH_Msg_GetDialog(const AH_MSG *hmsg);

int AH_Msg_EnableInsert(AH_MSG *hmsg);
int AH_Msg_HasWarnings(const AH_MSG *hmsg);
void AH_Msg_SetHasWarnings(AH_MSG *hmsg, int i);
int AH_Msg_HasErrors(const AH_MSG *hmsg);
void AH_Msg_SetHasErrors(AH_MSG *hmsg, int i);

int AH_Msg_GetResultCode(const AH_MSG *hmsg);
void AH_Msg_SetResultCode(AH_MSG *hmsg, int i);

const char *AH_Msg_GetResultText(const AH_MSG *hmsg);
void AH_Msg_SetResultText(AH_MSG *hmsg, const char *s);

const char *AH_Msg_GetResultParam(const AH_MSG *hmsg);
void AH_Msg_SetResultParam(AH_MSG *hmsg, const char *s);

unsigned int AH_Msg_GetHbciVersion(const AH_MSG *hmsg);
void AH_Msg_SetHbciVersion(AH_MSG *hmsg, unsigned int i);

const char *AH_Msg_GetTan(const AH_MSG *hmsg);

int AH_Msg_GetNeedTan(const AH_MSG *hmsg);
void AH_Msg_SetNeedTan(AH_MSG *hmsg, int i);

int AH_Msg_NoSysId(const AH_MSG *hmsg);
void AH_Msg_SetNoSysId(AH_MSG *hmsg, int i);

/*@}*/


/** @name Encoding, Decoding And Completing a Message
 *
 */
/*@{*/
unsigned int AH_Msg_GetCurrentSegmentNumber(AH_MSG *hmsg);

unsigned int AH_Msg_AddNode(AH_MSG *hmsg,
                            GWEN_XMLNODE *node,
                            GWEN_DB_NODE *data);

unsigned int AH_Msg_InsertNode(AH_MSG *hmsg,
                                    GWEN_XMLNODE *node,
                                    GWEN_DB_NODE *data);

int AH_Msg_EncodeMsg(AH_MSG *hmsg);

/**
 * @param hmsg message object
 * @param gr DB node to receive the decoded message
 * @param flags flags for @ref GWEN_MsgEngine_ParseMessage
 */
int AH_Msg_DecodeMsg(AH_MSG *hmsg,
                     GWEN_DB_NODE *gr,
                     unsigned int flags);

/*@}*/


/** @name Debugging Functions
 *
 */
/*@{*/
void AH_Msg__Dump(const AH_MSG *hmsg,
                  FILE *f,
                  unsigned int indent);

#define AH_Msg_Dump(hmsg, f, indent) \
  {fprintf(stderr, "Dumping message from "__FILE__" %d\n", __LINE__);\
  AH_Msg__Dump(hmsg, f, indent);}

/*@}*/


#ifdef __cplusplus
}
#endif
















#endif /* AH_MESSAGE_H */




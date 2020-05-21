/***************************************************************************
    begin       : Mon Mar 01 2004
    copyright   : (C) 2018 by Martin Preuss
    email       : martin@libchipcard.de

 ***************************************************************************
 *          Please see toplevel file COPYING for license details           *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "job_p.h"
#include "aqhbci_l.h"
#include "hbci_l.h"
#include "aqhbci/banking/user_l.h"
#include "aqhbci/banking/account_l.h"
#include "aqhbci/banking/provider_l.h"
#include "aqhbci/banking/provider.h"

#include <aqbanking/backendsupport/account.h>

#include <gwenhywfar/debug.h>
#include <gwenhywfar/misc.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/text.h>
#include <gwenhywfar/stringlist.h>
#include <gwenhywfar/cryptkeyrsa.h>

#include <assert.h>



GWEN_LIST_FUNCTIONS(AH_JOB, AH_Job);
GWEN_LIST2_FUNCTIONS(AH_JOB, AH_Job);
GWEN_INHERIT_FUNCTIONS(AH_JOB);




static GWEN_DB_NODE *_getHighestMatchingSepaProfile(AH_JOB *j, const GWEN_STRINGLIST *descriptors,
                                                    const char *sepaType);
static void _removeNonMatchingSepaProfiles(GWEN_DB_NODE *dbProfiles, const char *sepaTypePattern,
                                           const GWEN_STRINGLIST *descriptors);
static void _sortSepaProfilesDescending(GWEN_DB_NODE *dbProfiles);
static int _sortSepaProfilesXML_cb(const void *a, const void *b);






void AH_Job_free(AH_JOB *j)
{
  if (j) {
    assert(j->usage);
    if (--(j->usage)==0) {
      GWEN_StringList_free(j->challengeParams);
      GWEN_StringList_free(j->log);
      GWEN_StringList_free(j->signers);
      GWEN_StringList_free(j->sepaDescriptors);
      free(j->responseName);
      free(j->code);
      free(j->name);
      free(j->dialogId);
      free(j->expectedSigner);
      free(j->expectedCrypter);
      free(j->usedTan);
      GWEN_MsgEngine_free(j->msgEngine);
      GWEN_DB_Group_free(j->jobParams);
      GWEN_DB_Group_free(j->jobArguments);
      GWEN_DB_Group_free(j->jobResponses);
      GWEN_DB_Group_free(j->sepaProfile);
      AH_Result_List_free(j->msgResults);
      AH_Result_List_free(j->segResults);
      AB_Message_List_free(j->messages);

      AB_Transaction_List_free(j->transferList);
      AB_Transaction_List2_free(j->commandList);

      GWEN_LIST_FINI(AH_JOB, j);
      GWEN_INHERIT_FINI(AH_JOB, j);
      GWEN_FREE_OBJECT(j);
    }
  }
}



int AH_Job_SampleBpdVersions(const char *name,
                             AB_USER *u,
                             GWEN_DB_NODE *dbResult)
{
  GWEN_XMLNODE *node;
  const char *paramName;
  GWEN_DB_NODE *bpdgrp;
  const AH_BPD *bpd;
  GWEN_MSGENGINE *e;

  assert(name);
  assert(u);

  /* get job descriptions */
  e=AH_User_GetMsgEngine(u);
  assert(e);

  bpd=AH_User_GetBpd(u);

  if (AH_User_GetHbciVersion(u)==0)
    GWEN_MsgEngine_SetProtocolVersion(e, 210);
  else
    GWEN_MsgEngine_SetProtocolVersion(e, AH_User_GetHbciVersion(u));

  GWEN_MsgEngine_SetMode(e, AH_CryptMode_toString(AH_User_GetCryptMode(u)));

  /* first select any version, we simply need to know the BPD job name */
  node=GWEN_MsgEngine_FindNodeByProperty(e,
                                         "JOB",
                                         "id",
                                         0,
                                         name);
  if (!node) {
    DBG_INFO(AQHBCI_LOGDOMAIN,
             "Job \"%s\" not supported by local XML files", name);
    return GWEN_ERROR_NOT_FOUND;
  }

  /* get some properties */
  paramName=GWEN_XMLNode_GetProperty(node, "params", "");

  if (bpd) {
    bpdgrp=AH_Bpd_GetBpdJobs(bpd, AH_User_GetHbciVersion(u));
    assert(bpdgrp);
  }
  else
    bpdgrp=NULL;

  if (paramName && *paramName) {
    GWEN_DB_NODE *jobBPD;

    DBG_INFO(AQHBCI_LOGDOMAIN, "Job \"%s\" needs BPD job \"%s\"",
             name, paramName);

    if (!bpd) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "No BPD");
      return GWEN_ERROR_BAD_DATA;
    }

    /* get BPD job */
    jobBPD=GWEN_DB_GetGroup(bpdgrp,
                            GWEN_PATH_FLAGS_NAMEMUSTEXIST,
                            paramName);
    if (jobBPD) {
      /* children are one group per version */
      jobBPD=GWEN_DB_GetFirstGroup(jobBPD);
    }

    /* check for jobs for which we have a BPD */
    while (jobBPD) {
      int version;

      /* get version from BPD */
      version=atoi(GWEN_DB_GroupName(jobBPD));
      /* now get the correct version of the JOB */
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "Checking Job %s (%d)", name, version);
      node=GWEN_MsgEngine_FindNodeByProperty(e,
                                             "JOB",
                                             "id",
                                             version,
                                             name);
      if (node) {
        GWEN_DB_NODE *cpy;

        cpy=GWEN_DB_Group_dup(jobBPD);
        GWEN_DB_AddGroup(dbResult, cpy);
      }
      jobBPD=GWEN_DB_GetNextGroup(jobBPD);
    } /* while */
  } /* if paramName */
  else {
    DBG_ERROR(AQHBCI_LOGDOMAIN, "Job has no BPDs");
    return 0;
  }

  return 0;
}



int AH_Job_GetMaxVersionUpUntil(const char *name, AB_USER *u, int maxVersion)
{
  GWEN_DB_NODE *db;
  int rv;

  db=GWEN_DB_Group_new("bpd");
  rv=AH_Job_SampleBpdVersions(name, u, db);
  if (rv<0) {
    DBG_INFO(AQHBCI_LOGDOMAIN, "here (%d)", rv);
    GWEN_DB_Group_free(db);
    return rv;
  }
  else {
    GWEN_DB_NODE *dbT;
    int m=-1;

    /* determine maximum version */
    dbT=GWEN_DB_GetFirstGroup(db);
    while (dbT) {
      int v;

      v=atoi(GWEN_DB_GroupName(dbT));
      if (v>0 && v>m && v<=maxVersion)
        m=v;
      dbT=GWEN_DB_GetNextGroup(dbT);
    }
    GWEN_DB_Group_free(db);
    DBG_DEBUG(AQHBCI_LOGDOMAIN, "Max version of [%s] up until %d: %d",
              name, maxVersion, m);
    return m;
  }
}



AB_MESSAGE_LIST *AH_Job_GetMessages(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->messages;
}



int AH_Job_GetChallengeClass(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->challengeClass;
}



int AH_Job_GetSegmentVersion(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->segmentVersion;
}



void AH_Job_SetChallengeClass(AH_JOB *j, int i)
{
  assert(j);
  assert(j->usage);
  j->challengeClass=i;
}



void AH_Job_Attach(AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  j->usage++;
}



int AH_Job_PrepareNextMessage(AH_JOB *j)
{
  assert(j);
  assert(j->usage);

  if (j->nextMsgFn) {
    int rv;

    rv=j->nextMsgFn(j);
    if (rv==0) {
      /* callback flagged that no message follows */
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "Job says: No more messages");
      j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
      return 0;
    }
    else if (rv!=1) {
      DBG_INFO(AQHBCI_LOGDOMAIN, "Job says: Error");
      j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
      return rv;
    }
  }

  if (j->status==AH_JobStatusUnknown ||
      j->status==AH_JobStatusError) {
    DBG_INFO(AQHBCI_LOGDOMAIN, "At least one message had errors, aborting job");
    j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
    return 0;
  }

  if (j->status==AH_JobStatusToDo) {
    DBG_NOTICE(AQHBCI_LOGDOMAIN,
               "Hmm, job has never been sent, so we do nothing here");
    j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
    return 0;
  }

  if (j->flags & AH_JOB_FLAGS_HASATTACHPOINT) {
    DBG_NOTICE(AQHBCI_LOGDOMAIN,
               "Job has an attachpoint, so yes, we need more messages");
    j->flags|=AH_JOB_FLAGS_HASMOREMSGS;
    AH_Job_Log(j, GWEN_LoggerLevel_Debug,
               "Job has an attachpoint");
    return 1;
  }

  if (!(j->flags & AH_JOB_FLAGS_MULTIMSG)) {
    DBG_NOTICE(AQHBCI_LOGDOMAIN, "Not a Multi-message job, so we don't need more messages");
    j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
    return 0;
  }

  assert(j->msgNode);
  j->msgNode=GWEN_XMLNode_FindNextTag(j->msgNode, "MESSAGE", 0, 0);
  if (j->msgNode) {
    /* there is another message, so set flags accordingly */
    DBG_NOTICE(AQHBCI_LOGDOMAIN, "Multi-message job, still more messages");
    AH_Job_Log(j, GWEN_LoggerLevel_Debug,
               "Job has more messages");

    /* sample some flags for the next message */
    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "sign", "1"))!=0) {
      if (j->minSigs==0)
        j->minSigs=1;
      j->flags|=(AH_JOB_FLAGS_NEEDSIGN | AH_JOB_FLAGS_SIGN);
    }
    else {
      j->flags&=~(AH_JOB_FLAGS_NEEDSIGN | AH_JOB_FLAGS_SIGN);
    }
    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "crypt", "1"))!=0)
      j->flags|=(AH_JOB_FLAGS_NEEDCRYPT| AH_JOB_FLAGS_CRYPT);
    else
      j->flags&=~(AH_JOB_FLAGS_NEEDCRYPT| AH_JOB_FLAGS_CRYPT);

    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "nosysid", "0"))!=0)
      j->flags|=AH_JOB_FLAGS_NOSYSID;
    else
      j->flags&=~AH_JOB_FLAGS_NOSYSID;

    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "signseqone", "0"))!=0)
      j->flags|=AH_JOB_FLAGS_SIGNSEQONE;
    else
      j->flags&=~AH_JOB_FLAGS_SIGNSEQONE;

    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "noitan", "0"))!=0) {
      j->flags|=AH_JOB_FLAGS_NOITAN;
    }
    else
      j->flags&=~AH_JOB_FLAGS_NOITAN;

    if (atoi(GWEN_XMLNode_GetProperty(j->msgNode, "ignerrors", "0"))!=0)
      j->flags|=AH_JOB_FLAGS_IGNORE_ERROR;
    else
      j->flags&=~AH_JOB_FLAGS_IGNORE_ERROR;

    j->flags|=AH_JOB_FLAGS_HASMOREMSGS;
    return 1;
  }
  else {
    DBG_NOTICE(AQHBCI_LOGDOMAIN, "Job \"%s\" is finished", j->name);
    AH_Job_Log(j, GWEN_LoggerLevel_Debug,
               "Job has no more messages");
    j->flags&=~AH_JOB_FLAGS_HASMOREMSGS;
    return 0;
  }
}



uint32_t AH_Job_GetId(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->id;
}



void AH_Job_SetId(AH_JOB *j, uint32_t i)
{
  assert(j);
  assert(j->usage);
  j->id=i;
}



const char *AH_Job_GetName(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->name;
}



const char *AH_Job_GetCode(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->code;
}



void AH_Job_SetCode(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);
  if (j->code)
    free(j->code);
  if (s)
    j->code=strdup(s);
  else
    j->code=NULL;
}



const char *AH_Job_GetResponseName(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->responseName;
}



void AH_Job_SetResponseName(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);
  if (j->responseName)
    free(j->responseName);
  if (s)
    j->responseName=strdup(s);
  else
    j->responseName=NULL;
}



int AH_Job_GetMinSignatures(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->minSigs;
}



int AH_Job_GetSecurityProfile(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->secProfile;
}



int AH_Job_GetSecurityClass(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->secClass;
}



int AH_Job_GetJobsPerMsg(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->jobsPerMsg;
}



uint32_t AH_Job_GetFlags(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->flags;
}



void AH_Job_SetFlags(AH_JOB *j, uint32_t f)
{
  assert(j);
  assert(j->usage);
  DBG_DEBUG(AQHBCI_LOGDOMAIN, "Changing flags of job \"%s\" from %08x to %08x",
            j->name, j->flags, f);
  j->flags=f;
}



void AH_Job_AddFlags(AH_JOB *j, uint32_t f)
{
  assert(j);
  assert(j->usage);
  DBG_DEBUG(AQHBCI_LOGDOMAIN,
            "Changing flags of job \"%s\" from %08x to %08x",
            j->name, j->flags, j->flags|f);
  j->flags|=f;
}



void AH_Job_SubFlags(AH_JOB *j, uint32_t f)
{
  assert(j);
  assert(j->usage);
  DBG_DEBUG(AQHBCI_LOGDOMAIN,
            "Changing flags of job \"%s\" from %08x to %08x",
            j->name, j->flags, j->flags&~f);
  j->flags&=~f;
}



GWEN_DB_NODE *AH_Job_GetParams(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->jobParams;
}



GWEN_DB_NODE *AH_Job_GetArguments(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->jobArguments;
}



GWEN_DB_NODE *AH_Job_GetResponses(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->jobResponses;
}



uint32_t AH_Job_GetFirstSegment(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->firstSegment;
}



void AH_Job_SetFirstSegment(AH_JOB *j, uint32_t i)
{
  assert(j);
  assert(j->usage);
  j->firstSegment=i;
}



uint32_t AH_Job_GetLastSegment(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->lastSegment;
}



void AH_Job_SetLastSegment(AH_JOB *j, uint32_t i)
{
  assert(j);
  assert(j->usage);
  j->lastSegment=i;
}



int AH_Job_HasSegment(const AH_JOB *j, int seg)
{
  assert(j);
  assert(j->usage);
  DBG_DEBUG(AQHBCI_LOGDOMAIN, "Job \"%s\" checked for %d: first=%d, last=%d",
            j->name, seg,  j->firstSegment, j->lastSegment);
  return (seg<=j->lastSegment && seg>=j->firstSegment);
}



void AH_Job_AddResponse(AH_JOB *j, GWEN_DB_NODE *db)
{
  assert(j);
  assert(j->usage);
  GWEN_DB_AddGroup(j->jobResponses, db);
}



AH_JOB_STATUS AH_Job_GetStatus(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->status;
}



void AH_Job_SetStatus(AH_JOB *j, AH_JOB_STATUS st)
{
  assert(j);
  assert(j->usage);
  if (j->status!=st) {
    GWEN_BUFFER *lbuf;

    lbuf=GWEN_Buffer_new(0, 64, 0, 1);
    DBG_INFO(AQHBCI_LOGDOMAIN,
             "Changing status of job \"%s\" from \"%s\" (%d) to \"%s\" (%d)",
             j->name,
             AH_Job_StatusName(j->status), j->status,
             AH_Job_StatusName(st), st);
    GWEN_Buffer_AppendString(lbuf, "Status changed from \"");
    GWEN_Buffer_AppendString(lbuf, AH_Job_StatusName(j->status));
    GWEN_Buffer_AppendString(lbuf, "\" to \"");
    GWEN_Buffer_AppendString(lbuf, AH_Job_StatusName(st));
    GWEN_Buffer_AppendString(lbuf, "\"");

    AH_Job_Log(j, GWEN_LoggerLevel_Info, GWEN_Buffer_GetStart(lbuf));
    GWEN_Buffer_free(lbuf);
    j->status=st;

    /* set status to original command */
    if (j->commandList) {
      AB_TRANSACTION_LIST2_ITERATOR *jit;

      jit=AB_Transaction_List2_First(j->commandList);
      if (jit) {
        AB_TRANSACTION *t;
        AB_TRANSACTION_STATUS ts=AB_Transaction_StatusUnknown;

        switch (st) {
        case AH_JobStatusUnknown:
          ts=AB_Transaction_StatusUnknown;
          break;
        case AH_JobStatusToDo:
          ts=AB_Transaction_StatusEnqueued;
          break;
        case AH_JobStatusEnqueued:
          ts=AB_Transaction_StatusEnqueued;
          break;
        case AH_JobStatusEncoded:
          ts=AB_Transaction_StatusSending;
          break;
        case AH_JobStatusSent:
          ts=AB_Transaction_StatusSending;
          break;
        case AH_JobStatusAnswered:
          ts=AB_Transaction_StatusSending;
          break;
        case AH_JobStatusError:
          ts=AB_Transaction_StatusError;
          break;

        case AH_JobStatusAll:
          ts=AB_Transaction_StatusUnknown;
          break;
        }

        t=AB_Transaction_List2Iterator_Data(jit);
        while (t) {
          AB_Transaction_SetStatus(t, ts);
          t=AB_Transaction_List2Iterator_Next(jit);
        }
        AB_Transaction_List2Iterator_free(jit);
      } /* if (jit) */
    } /* if (j->commandList) */
  }
}



void AH_Job_AddSigner(AH_JOB *j, const char *s)
{
  GWEN_BUFFER *lbuf;

  assert(j);
  assert(j->usage);
  assert(s);

  lbuf=GWEN_Buffer_new(0, 128, 0, 1);
  if (!GWEN_StringList_AppendString(j->signers, s, 0, 1)) {
    DBG_INFO(AQHBCI_LOGDOMAIN, "Signer \"%s\" already in list", s);
    GWEN_Buffer_AppendString(lbuf, "Signer \"");
    GWEN_Text_EscapeToBufferTolerant(s, lbuf);
    GWEN_Buffer_AppendString(lbuf, "\" already in list");
    AH_Job_Log(j, GWEN_LoggerLevel_Warning,
               GWEN_Buffer_GetStart(lbuf));
  }
  else {
    GWEN_Buffer_AppendString(lbuf, "Signer \"");
    GWEN_Text_EscapeToBufferTolerant(s, lbuf);
    GWEN_Buffer_AppendString(lbuf, "\" added");
    AH_Job_Log(j, GWEN_LoggerLevel_Info,
               GWEN_Buffer_GetStart(lbuf));
  }
  GWEN_Buffer_free(lbuf);
  j->flags|=AH_JOB_FLAGS_SIGN;
}



int AH_Job_AddSigners(AH_JOB *j, const GWEN_STRINGLIST *sl)
{
  int sCount=0;

  if (sl) {
    GWEN_STRINGLISTENTRY *se;

    se=GWEN_StringList_FirstEntry(sl);
    while (se) {
      AH_Job_AddSigner(j, GWEN_StringListEntry_Data(se));
      sCount++;
      se=GWEN_StringListEntry_Next(se);
    } /* while */
  }

  return sCount;
}



AB_USER *AH_Job_GetUser(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->user;
}



const GWEN_STRINGLIST *AH_Job_GetSigners(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->signers;
}



GWEN_XMLNODE *AH_Job_GetXmlNode(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  if (j->flags & AH_JOB_FLAGS_MULTIMSG) {
    DBG_DEBUG(AQHBCI_LOGDOMAIN,
              "Multi message node, returning current message node");
    return j->msgNode;
  }
  return j->xmlNode;
}



unsigned int AH_Job_GetMsgNum(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->msgNum;
}



const char *AH_Job_GetDialogId(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->dialogId;
}



void AH_Job_SetMsgNum(AH_JOB *j, unsigned int i)
{
  assert(j);
  assert(j->usage);
  j->msgNum=i;
}



void AH_Job_SetDialogId(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);
  assert(s);

  free(j->dialogId);
  j->dialogId=strdup(s);
}



const char *AH_Job_StatusName(AH_JOB_STATUS st)
{
  switch (st) {
  case AH_JobStatusUnknown:
    return "unknown";
  case AH_JobStatusToDo:
    return "todo";
  case AH_JobStatusEnqueued:
    return "enqueued";
  case AH_JobStatusEncoded:
    return "encoded";
  case AH_JobStatusSent:
    return "sent";
  case AH_JobStatusAnswered:
    return "answered";
  case AH_JobStatusError:
    return "error";

  case AH_JobStatusAll:
    return "any";
  default:
    return "?";
  }
}


int AH_Job_HasWarnings(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return (j->flags & AH_JOB_FLAGS_HASWARNINGS);
}



int AH_Job_HasErrors(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return
    (j->status==AH_JobStatusError) ||
    (j->flags & AH_JOB_FLAGS_HASERRORS);
}



void AH_Job_SampleResults(AH_JOB *j)
{
  GWEN_DB_NODE *dbCurr;

  assert(j);
  assert(j->usage);

  dbCurr=GWEN_DB_GetFirstGroup(j->jobResponses);
  while (dbCurr) {
    GWEN_DB_NODE *dbResults;

    dbResults=GWEN_DB_GetGroup(dbCurr, GWEN_PATH_FLAGS_NAMEMUSTEXIST,
                               "data/SegResult");
    if (dbResults) {
      GWEN_DB_NODE *dbRes;

      dbRes=GWEN_DB_GetFirstGroup(dbResults);
      while (dbRes) {
        if (strcasecmp(GWEN_DB_GroupName(dbRes), "result")==0) {
          AH_RESULT *r;
          int code;
          const char *text;

          code=GWEN_DB_GetIntValue(dbRes, "resultcode", 0, 0);
          text=GWEN_DB_GetCharValue(dbRes, "text", 0, 0);
          if (code) {
            GWEN_BUFFER *lbuf;
            char numbuf[32];
            GWEN_LOGGER_LEVEL ll;

            if (code>=9000)
              ll=GWEN_LoggerLevel_Error;
            else if (code>=3000 && code!=3920)
              ll=GWEN_LoggerLevel_Warning;
            else
              ll=GWEN_LoggerLevel_Info;
            lbuf=GWEN_Buffer_new(0, 128, 0, 1);
            GWEN_Buffer_AppendString(lbuf, "SegResult: ");
            snprintf(numbuf, sizeof(numbuf), "%d", code);
            GWEN_Buffer_AppendString(lbuf, numbuf);
            if (text) {
              GWEN_Buffer_AppendString(lbuf, "(");
              GWEN_Buffer_AppendString(lbuf, text);
              GWEN_Buffer_AppendString(lbuf, ")");
            }
            AH_Job_Log(j, ll,
                       GWEN_Buffer_GetStart(lbuf));
            GWEN_Buffer_free(lbuf);
          }

          /* found a result */
          r=AH_Result_new(code,
                          text,
                          GWEN_DB_GetCharValue(dbRes, "ref", 0, 0),
                          GWEN_DB_GetCharValue(dbRes, "param", 0, 0),
                          0);
          AH_Result_List_Add(r, j->segResults);

          DBG_DEBUG(AQHBCI_LOGDOMAIN, "Segment result:");
          if (GWEN_Logger_GetLevel(0)>=GWEN_LoggerLevel_Debug)
            AH_Result_Dump(r, stderr, 4);

          /* check result */
          if (code>=9000)
            j->flags|=AH_JOB_FLAGS_HASERRORS;
          else if (code>=3000 && code<4000)
            j->flags|=AH_JOB_FLAGS_HASWARNINGS;
        } /* if result */
        dbRes=GWEN_DB_GetNextGroup(dbRes);
      } /* while */
    } /* if segResult */
    else {
      dbResults=GWEN_DB_GetGroup(dbCurr, GWEN_PATH_FLAGS_NAMEMUSTEXIST,
                                 "data/MsgResult");
      if (dbResults) {
        GWEN_DB_NODE *dbRes;

        dbRes=GWEN_DB_GetFirstGroup(dbResults);
        while (dbRes) {
          if (strcasecmp(GWEN_DB_GroupName(dbRes), "result")==0) {
            AH_RESULT *r;
            int code;
            const char *text;

            code=GWEN_DB_GetIntValue(dbRes, "resultcode", 0, 0);
            text=GWEN_DB_GetCharValue(dbRes, "text", 0, 0);
            if (code) {
              GWEN_BUFFER *lbuf;
              char numbuf[32];
              GWEN_LOGGER_LEVEL ll;

              if (code>=9000)
                ll=GWEN_LoggerLevel_Error;
              else if (code>=3000)
                ll=GWEN_LoggerLevel_Warning;
              else
                ll=GWEN_LoggerLevel_Info;
              lbuf=GWEN_Buffer_new(0, 128, 0, 1);
              GWEN_Buffer_AppendString(lbuf, "MsgResult: ");
              snprintf(numbuf, sizeof(numbuf), "%d", code);
              GWEN_Buffer_AppendString(lbuf, numbuf);
              if (text) {
                GWEN_Buffer_AppendString(lbuf, "(");
                GWEN_Buffer_AppendString(lbuf, text);
                GWEN_Buffer_AppendString(lbuf, ")");
              }
              AH_Job_Log(j, ll,
                         GWEN_Buffer_GetStart(lbuf));
              GWEN_Buffer_free(lbuf);
            }

            /* found a result */
            r=AH_Result_new(code,
                            text,
                            GWEN_DB_GetCharValue(dbRes, "ref", 0, 0),
                            GWEN_DB_GetCharValue(dbRes, "param", 0, 0),
                            1);
            AH_Result_List_Add(r, j->msgResults);
            DBG_DEBUG(AQHBCI_LOGDOMAIN, "Message result:");
            if (GWEN_Logger_GetLevel(0)>=GWEN_LoggerLevel_Debug)
              AH_Result_Dump(r, stderr, 4);

            /* check result */
            if (code>=9000) {
              /* FIXME: Maybe disable here, let only the segment results
               * influence the error flags */
              j->flags|=AH_JOB_FLAGS_HASERRORS;
            }
            else if (code>=3000 && code<4000)
              j->flags|=AH_JOB_FLAGS_HASWARNINGS;
          } /* if result */
          dbRes=GWEN_DB_GetNextGroup(dbRes);
        } /* while */
      } /* if msgResult */
    }
    dbCurr=GWEN_DB_GetNextGroup(dbCurr);
  } /* while */

}



const char *AH_Job_GetDescription(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  if (j->description)
    return j->description;
  return j->name;
}



void AH_Job_Dump(const AH_JOB *j, FILE *f, unsigned int insert)
{
  uint32_t k;

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Job:\n");

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Name          : %s\n", j->name);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Code          : %s\n", (j->code)?(j->code):"(empty)");

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "SegVer        : %d\n", j->segmentVersion);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "FirstSegment  : %d\n", j->firstSegment);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "LasttSegment  : %d\n", j->lastSegment);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "ChallengeClass: %d\n", j->challengeClass);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "MinSigs       : %d\n", j->minSigs);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "SecProfile    : %d\n", j->secProfile);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "SecClass      : %d\n", j->secClass);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "JobsPerMsg    : %d\n", j->jobsPerMsg);


  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Status        : %s (%d)\n", AH_Job_StatusName(j->status), j->status);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Msgnum        : %d\n", j->msgNum);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "DialogId      : %s\n", j->dialogId);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Owner         : %s\n", AB_User_GetCustomerId(j->user));

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "MaxTransfers  : %d\n", j->maxTransfers);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "TransferCount : %d\n", j->transferCount);

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "SupportedCmd  : %s\n", AB_Transaction_Command_toString(j->supportedCommand));

  for (k=0; k<insert; k++)
    fprintf(f, " ");
  fprintf(f, "Flags: %08x ( ", j->flags);
  if (j->flags & AH_JOB_FLAGS_IGNOREACCOUNTS)
    fprintf(f, "IGNOREACCOUNTS ");
  if (j->flags & AH_JOB_FLAGS_SIGNSEQONE)
    fprintf(f, "SIGNSEQONE ");
  if (j->flags & AH_JOB_FLAGS_IGNORE_ERROR)
    fprintf(f, "IGNORE_ERROR ");
  if (j->flags & AH_JOB_FLAGS_NOITAN)
    fprintf(f, "NOITAN ");
  if (j->flags & AH_JOB_FLAGS_TANUSED)
    fprintf(f, "TANUSED ");
  if (j->flags & AH_JOB_FLAGS_NOSYSID)
    fprintf(f, "NOSYSID ");
  if (j->flags & AH_JOB_FLAGS_NEEDCRYPT)
    fprintf(f, "NEEDCRYPT ");
  if (j->flags & AH_JOB_FLAGS_NEEDSIGN)
    fprintf(f, "NEEDSIGN ");
  if (j->flags & AH_JOB_FLAGS_ATTACHABLE)
    fprintf(f, "ATTACHABLE ");
  if (j->flags & AH_JOB_FLAGS_SINGLE)
    fprintf(f, "SINGLE ");
  if (j->flags & AH_JOB_FLAGS_DLGJOB)
    fprintf(f, "DLGJOB ");
  if (j->flags & AH_JOB_FLAGS_CRYPT)
    fprintf(f, "CRYPT ");
  if (j->flags & AH_JOB_FLAGS_SIGN)
    fprintf(f, "SIGN ");
  if (j->flags & AH_JOB_FLAGS_MULTIMSG)
    fprintf(f, "MULTIMSG ");
  if (j->flags & AH_JOB_FLAGS_HASATTACHPOINT)
    fprintf(f, "HASATTACHPOINT ");
  if (j->flags & AH_JOB_FLAGS_HASMOREMSGS)
    fprintf(f, "HASMOREMSGS ");
  if (j->flags & AH_JOB_FLAGS_HASWARNINGS)
    fprintf(f, "HASWARNINGS ");
  if (j->flags & AH_JOB_FLAGS_HASERRORS)
    fprintf(f, "HASERRORS ");
  if (j->flags & AH_JOB_FLAGS_PROCESSED)
    fprintf(f, "PROCESSED ");
  if (j->flags & AH_JOB_FLAGS_COMMITTED)
    fprintf(f, "COMMITTED ");
  if (j->flags & AH_JOB_FLAGS_NEEDTAN)
    fprintf(f, "NEEDTAN ");
  if (j->flags & AH_JOB_FLAGS_OUTBOX)
    fprintf(f, "OUTBOX ");
  fprintf(f, ")\n");


  if (j->jobResponses) {
    for (k=0; k<insert; k++)
      fprintf(f, " ");
    fprintf(f, "Response Data:\n");
    GWEN_DB_Dump(j->jobResponses, insert+2);
  }
}






int AH_Job_HasItanResult(const AH_JOB *j)
{

  return AH_Job_HasResultWithCode(j, 3920);
}



int AH_Job_HasResultWithCode(const AH_JOB *j, int wantedCode)
{
  GWEN_DB_NODE *dbCurr;

  assert(j);
  assert(j->usage);

  dbCurr=GWEN_DB_GetFirstGroup(j->jobResponses);
  while (dbCurr) {
    GWEN_DB_NODE *dbRd;

    dbRd=GWEN_DB_GetGroup(dbCurr, GWEN_PATH_FLAGS_NAMEMUSTEXIST, "data");
    if (dbRd) {
      dbRd=GWEN_DB_GetFirstGroup(dbRd);
    }
    if (dbRd) {
      const char *sGroupName;

      sGroupName=GWEN_DB_GroupName(dbRd);

      if (sGroupName && *sGroupName &&
          ((strcasecmp(sGroupName, "SegResult")==0) ||
           (strcasecmp(sGroupName, "MsgResult")==0))) {
        GWEN_DB_NODE *dbRes;

        dbRes=GWEN_DB_GetFirstGroup(dbRd);
        while (dbRes) {
          if (strcasecmp(GWEN_DB_GroupName(dbRes), "result")==0) {
            int code;

            code=GWEN_DB_GetIntValue(dbRes, "resultcode", 0, 0);
            DBG_DEBUG(AQHBCI_LOGDOMAIN, "Checking result code %d against %d", code, wantedCode);
            if (code==wantedCode) {
              return 1;
            }
          } /* if result */
          dbRes=GWEN_DB_GetNextGroup(dbRes);
        } /* while */
      }
    } /* if response data found */
    dbCurr=GWEN_DB_GetNextGroup(dbCurr);
  } /* while */

  return 0; /* no iTAN response */
}



AH_JOB *AH_Job__freeAll_cb(AH_JOB *j, void *userData)
{
  assert(j);
  assert(j->usage);
  AH_Job_free(j);
  return 0;
}



void AH_Job_List2_FreeAll(AH_JOB_LIST2 *jl)
{
  AH_Job_List2_ForEach(jl, AH_Job__freeAll_cb, 0);
  AH_Job_List2_free(jl);
}



AH_HBCI *AH_Job_GetHbci(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);

  return AH_User_GetHbci(j->user);
}



AB_BANKING *AH_Job_GetBankingApi(const AH_JOB *j)
{
  AH_HBCI *hbci;

  assert(j);
  assert(j->usage);
  hbci=AH_Job_GetHbci(j);
  assert(hbci);
  return AH_HBCI_GetBankingApi(hbci);
}



AH_RESULT_LIST *AH_Job_GetSegResults(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->segResults;
}



AH_RESULT_LIST *AH_Job_GetMsgResults(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->msgResults;
}



const char *AH_Job_GetExpectedSigner(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->expectedSigner;
}



void AH_Job_SetExpectedSigner(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);
  free(j->expectedSigner);
  if (s)
    j->expectedSigner=strdup(s);
  else
    j->expectedSigner=0;
}



const char *AH_Job_GetExpectedCrypter(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->expectedCrypter;
}



void AH_Job_SetExpectedCrypter(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);
  free(j->expectedCrypter);
  if (s)
    j->expectedCrypter=strdup(s);
  else
    j->expectedCrypter=0;
}



int AH_Job_CheckEncryption(AH_JOB *j, GWEN_DB_NODE *dbRsp)
{
  if (AH_User_GetCryptMode(j->user)==AH_CryptMode_Pintan) {
    DBG_DEBUG(AQHBCI_LOGDOMAIN, "Not checking security in PIN/TAN mode");
  }
  else {
    GWEN_DB_NODE *dbSecurity;
    const char *s;

    assert(j);
    assert(j->usage);
    assert(dbRsp);
    dbSecurity=GWEN_DB_GetGroup(dbRsp, GWEN_PATH_FLAGS_NAMEMUSTEXIST, "security");
    if (!dbSecurity) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "No security settings, should not happen");
      GWEN_Gui_ProgressLog(0,
                           GWEN_LoggerLevel_Error,
                           I18N("Response without security info (internal)"));
      return AB_ERROR_SECURITY;
    }

    s=GWEN_DB_GetCharValue(dbSecurity, "crypter", 0, 0);
    if (s) {
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "Response encrypted with key [%s]", s);

      if (*s=='!' || *s=='?') {
        DBG_ERROR(AQHBCI_LOGDOMAIN, "Encrypted with invalid key (%s)", s);
        GWEN_Gui_ProgressLog(0,
                             GWEN_LoggerLevel_Error,
                             I18N("Response encrypted with invalid key"));
        return AB_ERROR_SECURITY;
      }
    }

    if (j->expectedCrypter) {
      /* check crypter */
      if (!s) {
        DBG_ERROR(AQHBCI_LOGDOMAIN,
                  "Response is not encrypted (but expected to be)");
        GWEN_Gui_ProgressLog(0,
                             GWEN_LoggerLevel_Error,
                             I18N("Response is not encrypted as expected"));
        return AB_ERROR_SECURITY;

      }

      if (strcasecmp(s, j->expectedCrypter)!=0) {
        DBG_WARN(AQHBCI_LOGDOMAIN,
                 "Not encrypted with the expected key "
                 "(exp: \"%s\", is: \"%s\"",
                 j->expectedCrypter, s);
        /*
         GWEN_Gui_ProgressLog(
                              0,
                              GWEN_LoggerLevel_Error,
                              I18N("Response not encrypted with expected key"));
         return AB_ERROR_SECURITY;
         */
      }
    }
    else {
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "No specific encrypter expected");
    }
  }

  return 0;
}



int AH_Job_CheckSignature(AH_JOB *j, GWEN_DB_NODE *dbRsp)
{
  if (AH_User_GetCryptMode(j->user)==AH_CryptMode_Pintan) {
    DBG_DEBUG(AQHBCI_LOGDOMAIN, "Not checking signature in PIN/TAN mode");
  }
  else {
    GWEN_DB_NODE *dbSecurity;
    int i;
    uint32_t uFlags;

    assert(j);
    assert(j->usage);

    uFlags=AH_User_GetFlags(j->user);

    assert(dbRsp);
    dbSecurity=GWEN_DB_GetGroup(dbRsp, GWEN_PATH_FLAGS_NAMEMUSTEXIST,
                                "security");
    if (!dbSecurity) {
      DBG_ERROR(AQHBCI_LOGDOMAIN,
                "No security settings, should not happen");
      GWEN_Gui_ProgressLog(
        0,
        GWEN_LoggerLevel_Error,
        I18N("Response without security info (internal)"));
      return GWEN_ERROR_GENERIC;
    }

    /* check for invalid signers */
    for (i=0; ; i++) {
      const char *s;

      s=GWEN_DB_GetCharValue(dbSecurity, "signer", i, 0);
      if (!s)
        break;
      if (*s=='!') {
        DBG_ERROR(AQHBCI_LOGDOMAIN,
                  "Invalid signature found, will not tolerate it");
        GWEN_Gui_ProgressLog(0,
                             GWEN_LoggerLevel_Error,
                             I18N("Invalid bank signature"));
        return AB_ERROR_SECURITY;
      }
    } /* for */

    if (j->expectedSigner && !(uFlags & AH_USER_FLAGS_BANK_DOESNT_SIGN)) {
      /* check signer */
      for (i=0; ; i++) {
        const char *s;

        s=GWEN_DB_GetCharValue(dbSecurity, "signer", i, 0);
        if (!s) {
          DBG_ERROR(AQHBCI_LOGDOMAIN,
                    "Not signed by expected signer (%d)", i);
          GWEN_Gui_ProgressLog(0,
                               GWEN_LoggerLevel_Error,
                               I18N("Response not signed by the bank"));
          if (i==0) {
            int but;

            /* check whether the user want's to accept the unsigned message */
            but=GWEN_Gui_MessageBox(GWEN_GUI_MSG_FLAGS_TYPE_WARN |
                                    GWEN_GUI_MSG_FLAGS_CONFIRM_B1 |
                                    GWEN_GUI_MSG_FLAGS_SEVERITY_DANGEROUS,
                                    I18N("Security Warning"),
                                    I18N(
                                      "The HBCI response of the bank has not been signed by the bank, \n"
                                      "contrary to what has been expected. This can be the case because the \n"
                                      "bank just stopped signing their HBCI responses. This error message \n"
                                      "would also occur if there were a replay attack against your computer \n"
                                      "in progress right now, which is probably quite unlikely. \n"
                                      " \n"
                                      "Please contact your bank and ask them whether their HBCI server \n"
                                      "stopped signing the HBCI responses. If the bank is concerned about \n"
                                      "your security, it should not stop signing the HBCI responses. \n"
                                      " \n"
                                      "Do you nevertheless want to accept this response this time or always?"
                                      "<html><p>"
                                      "The HBCI response of the bank has not been signed by the bank, \n"
                                      "contrary to what has been expected. This can be the case because the \n"
                                      "bank just stopped signing their HBCI responses. This error message \n"
                                      "would also occur if there were a replay attack against your computer \n"
                                      "in progress right now, which is probably quite unlikely. \n"
                                      "</p><p>"
                                      "Please contact your bank and ask them whether their HBCI server \n"
                                      "stopped signing the HBCI responses. If the bank is concerned about \n"
                                      "your security, it should not stop signing the HBCI responses. \n"
                                      "</p><p>"
                                      "Do you nevertheless want to accept this response this time or always?"
                                      "</p></html>"
                                    ),
                                    I18N("Accept this time"),
                                    I18N("Accept always"),
                                    I18N("Abort"), 0);
            if (but==1) {
              GWEN_Gui_ProgressLog(0,
                                   GWEN_LoggerLevel_Notice,
                                   I18N("User accepts this unsigned "
                                        "response"));
              AH_Job_SetExpectedSigner(j, 0);
              break;
            }
            else if (but==2) {
              GWEN_Gui_ProgressLog(0,
                                   GWEN_LoggerLevel_Notice,
                                   I18N("User accepts all further unsigned "
                                        "responses"));
              AH_User_AddFlags(j->user, AH_USER_FLAGS_BANK_DOESNT_SIGN);
              AH_Job_SetExpectedSigner(j, 0);
              break;
            }
            else {
              GWEN_Gui_ProgressLog(0,
                                   GWEN_LoggerLevel_Error,
                                   I18N("Aborted"));
              return AB_ERROR_SECURITY;
            }
          }
          else {
            int ii;

            DBG_ERROR(AQHBCI_LOGDOMAIN,
                      "Job signed with unexpected key(s)"
                      "(was expecting \"%s\"):",
                      j->expectedSigner);
            for (ii=0; ; ii++) {
              s=GWEN_DB_GetCharValue(dbSecurity, "signer", ii, 0);
              if (!s)
                break;
              DBG_ERROR(AQHBCI_LOGDOMAIN,
                        "Signed unexpectedly with key \"%s\"", s);
            }
            return AB_ERROR_SECURITY;
          }
        }
        else {
          if (strcasecmp(s, j->expectedSigner)==0) {
            DBG_DEBUG(AQHBCI_LOGDOMAIN,
                      "Jobs signed as expected with \"%s\"",
                      j->expectedSigner);
            break;
          }
          else if (*s!='!' && *s!='?') {
            DBG_INFO(AQHBCI_LOGDOMAIN,
                     "Signer name does not match expected name (%s!=%s), "
                     "but we accept it anyway",
                     s, j->expectedSigner);
            break;
          }
        }
      } /* for */
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "Signature check ok");
    }
    else {
      DBG_DEBUG(AQHBCI_LOGDOMAIN, "No signature expected");
    }
  }
  return 0;
}



const char *AH_Job_GetUsedTan(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->usedTan;
}



void AH_Job_SetUsedTan(AH_JOB *j, const char *s)
{
  assert(j);
  assert(j->usage);

  DBG_DEBUG(AQHBCI_LOGDOMAIN, "Changing TAN in job [%s](%08x) from [%s] to [%s]",
            j->name, j->id,
            (j->usedTan)?(j->usedTan):"(empty)",
            s?s:"(empty)");
  free(j->usedTan);
  if (s) {
    j->usedTan=strdup(s);
  }
  else
    j->usedTan=0;
}



void AH_Job_Log(AH_JOB *j, GWEN_LOGGER_LEVEL ll, const char *txt)
{
  char buffer[32];
  GWEN_TIME *ti;
  GWEN_BUFFER *lbuf;

  assert(j);

  lbuf=GWEN_Buffer_new(0, 128, 0, 1);
  snprintf(buffer, sizeof(buffer), "%02d", ll);
  GWEN_Buffer_AppendString(lbuf, buffer);
  GWEN_Buffer_AppendByte(lbuf, ':');
  ti=GWEN_CurrentTime();
  assert(ti);
  GWEN_Time_toString(ti, "YYYYMMDD:hhmmss:", lbuf);
  GWEN_Time_free(ti);
  GWEN_Text_EscapeToBufferTolerant(AH_PROVIDER_NAME, lbuf);
  GWEN_Buffer_AppendByte(lbuf, ':');
  GWEN_Text_EscapeToBufferTolerant(txt, lbuf);
  GWEN_StringList_AppendString(j->log,
                               GWEN_Buffer_GetStart(lbuf),
                               0, 0);
  GWEN_Buffer_free(lbuf);
}



const GWEN_STRINGLIST *AH_Job_GetLogs(const AH_JOB *j)
{
  assert(j);
  return j->log;
}



GWEN_STRINGLIST *AH_Job_GetChallengeParams(const AH_JOB *j)
{
  assert(j);
  return j->challengeParams;
}



void AH_Job_ClearChallengeParams(AH_JOB *j)
{
  assert(j);
  GWEN_StringList_Clear(j->challengeParams);
}



void AH_Job_AddChallengeParam(AH_JOB *j, const char *s)
{
  assert(j);
  GWEN_StringList_AppendString(j->challengeParams, s, 0, 0);
}



void AH_Job_ValueToChallengeString(const AB_VALUE *v, GWEN_BUFFER *buf)
{
  AB_Value_toHbciString(v, buf);
}



int AH_Job_GetTransferCount(AH_JOB *j)
{
  assert(j);
  return j->transferCount;
}



void AH_Job_IncTransferCount(AH_JOB *j)
{
  assert(j);
  j->transferCount++;
}



int AH_Job_GetMaxTransfers(AH_JOB *j)
{
  assert(j);
  return j->maxTransfers;
}



void AH_Job_SetMaxTransfers(AH_JOB *j, int i)
{
  assert(j);
  j->maxTransfers=i;
}



AB_TRANSACTION_LIST *AH_Job_GetTransferList(const AH_JOB *j)
{
  assert(j);
  return j->transferList;
}



AB_TRANSACTION *AH_Job_GetFirstTransfer(const AH_JOB *j)
{
  assert(j);
  if (j->transferList==NULL)
    return NULL;

  return AB_Transaction_List_First(j->transferList);
}



void AH_Job_AddTransfer(AH_JOB *j, AB_TRANSACTION *t)
{
  assert(j);
  if (j->transferList==NULL)
    j->transferList=AB_Transaction_List_new();

  AB_Transaction_List_Add(t, j->transferList);
  j->transferCount++;
}



static int AH_Job__SepaProfileSupported(GWEN_DB_NODE *profile, const GWEN_STRINGLIST *descriptors)
{
  GWEN_STRINGLISTENTRY *se;
  char pattern[13];
  const char *s;

  /* patterns shall always have the form *xxx.yyy.zz* */
  pattern[0]=pattern[11]='*';
  pattern[12]='\0';

  /* Well formed type strings are exactly 10 characters long. Others
   * will either not match or be rejected by the exporter. */
  strncpy(pattern+1, GWEN_DB_GetCharValue(profile, "params/sepaType", 0, ""), 10);
  se=GWEN_StringList_FirstEntry(descriptors);
  while (se) {
    s=GWEN_StringListEntry_Data(se);
    /*DBG_ERROR(AQHBCI_LOGDOMAIN, "Checking \"%s\" against pattern \"%s\"", s, pattern);*/
    if (s && GWEN_Text_ComparePattern(s, pattern, 1)!=-1) {
      /* record the descriptor matching this profile */
      GWEN_DB_SetCharValue(profile, GWEN_DB_FLAGS_OVERWRITE_VARS, "descriptor", s);
      /*DBG_ERROR(AQHBCI_LOGDOMAIN, "Match.");*/
      break;
    }
    /*DBG_ERROR(AQHBCI_LOGDOMAIN, "No match.");*/
    se=GWEN_StringListEntry_Next(se);
  }
  if (se)
    return 1;
  else
    return 0;
}



GWEN_DB_NODE *AH_Job_FindSepaProfile(AH_JOB *j, const char *sepaTypePattern, const char *name)
{
  const GWEN_STRINGLIST *descriptors;
  GWEN_DB_NODE *dbMatchingProfile;

  assert(j);

  DBG_INFO(AQHBCI_LOGDOMAIN, "Looking for profile matching \"%s\" (\"%s\")", sepaTypePattern, name?name:"<noname>");

  if (j->sepaDescriptors)
    descriptors=j->sepaDescriptors;
  else
    descriptors=AH_User_GetSepaDescriptors(j->user);
  if (GWEN_StringList_Count(descriptors)==0) {
    DBG_ERROR(AQHBCI_LOGDOMAIN, "No SEPA descriptor found, please update your account information");
    return NULL;
  }

  if (name) {
    GWEN_DB_NODE *profile;

    profile=AB_Banking_GetImExporterProfile(AH_Job_GetBankingApi(j), "xml", name);
    if (!profile) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "Profile \"%s\" not available", name);
      return NULL;
    }
    if (GWEN_Text_ComparePattern(GWEN_DB_GetCharValue(profile, "params/sepaType", 0, ""), sepaTypePattern, 1)==-1) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "Profile \"%s\" does not match type specification (\"%s\")", name, sepaTypePattern);
      return NULL;
    }
    if (!AH_Job__SepaProfileSupported(profile, descriptors)) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "Profile \"%s\" not supported by bank server", name);
      return NULL;
    }
    j->sepaProfile=profile;
    return profile;
  }

  if (!sepaTypePattern)
    return j->sepaProfile;

  if (j->sepaProfile &&
      GWEN_Text_ComparePattern(GWEN_DB_GetCharValue(j->sepaProfile, "params/sepaType", 0, ""), sepaTypePattern, 1)!=-1)
    return j->sepaProfile;

  dbMatchingProfile=_getHighestMatchingSepaProfile(j, descriptors, sepaTypePattern);
  if (dbMatchingProfile==NULL) {
    DBG_ERROR(AQHBCI_LOGDOMAIN, "No matching profile found for \"%s\" (\"%s\")", sepaTypePattern, name?name:"<noname>");
    return NULL;
  }

  /* set new profile */
  if (j->sepaProfile)
    GWEN_DB_Group_free(j->sepaProfile);
  j->sepaProfile=dbMatchingProfile;

  return j->sepaProfile;
}



GWEN_DB_NODE *_getHighestMatchingSepaProfile(AH_JOB *j, const GWEN_STRINGLIST *descriptors, const char *sepaTypePattern)
{
  GWEN_DB_NODE *dbProfiles;

  assert(j);

  dbProfiles=AB_Banking_GetImExporterProfiles(AH_Job_GetBankingApi(j), "xml");
  if (dbProfiles) {
    GWEN_DB_NODE *dbMatchingProfile;

    _removeNonMatchingSepaProfiles(dbProfiles, sepaTypePattern, descriptors);
    _sortSepaProfilesDescending(dbProfiles);

    dbMatchingProfile=GWEN_DB_GetFirstGroup(dbProfiles);
    if (dbMatchingProfile==NULL) {
      DBG_ERROR(AQHBCI_LOGDOMAIN, "No supported SEPA format found for job \"%s\"", j->name);
      GWEN_DB_Group_free(dbProfiles);
      return NULL;
    }
    GWEN_DB_UnlinkGroup(dbMatchingProfile);
    GWEN_DB_Group_free(dbProfiles);
    return dbMatchingProfile;
  }
  else {
    DBG_ERROR(AQHBCI_LOGDOMAIN, "No XML profiles found");
  }

  return NULL;
}



void _removeNonMatchingSepaProfiles(GWEN_DB_NODE *dbProfiles, const char *sepaTypePattern,
                                    const GWEN_STRINGLIST *descriptors)
{
  GWEN_DB_NODE *n, *nn;

  n=GWEN_DB_GetFirstGroup(dbProfiles);
  while (n) {
    const char *sSepaType;
    int doRemove=0;

    nn=n;
    n=GWEN_DB_GetNextGroup(n);

    sSepaType=GWEN_DB_GetCharValue(nn, "params/sepaType", 0, "");

    if (GWEN_Text_ComparePattern(sSepaType, sepaTypePattern, 1)==-1) {
      DBG_INFO(AQHBCI_LOGDOMAIN, "Profile \"%s\" does not match given pattern (%s)", sSepaType, sepaTypePattern);
      doRemove=1;
    }

    if (!AH_Job__SepaProfileSupported(nn, descriptors)) {
      DBG_INFO(AQHBCI_LOGDOMAIN, "Profile \"%s\" not supported", sSepaType);
      doRemove=1;
    }

    if (doRemove) {
      GWEN_DB_UnlinkGroup(nn);
      GWEN_DB_Group_free(nn);
    }
  }
}



void _sortSepaProfilesDescending(GWEN_DB_NODE *dbProfiles)
{
  int pCount;

  pCount=GWEN_DB_Groups_Count(dbProfiles)>1;
  if (pCount) {
    GWEN_DB_NODE **orderedProfiles;
    unsigned int i;

    /* cut groups out of the tree, add to flat list */
    orderedProfiles=malloc(pCount*sizeof(GWEN_DB_NODE *));
    assert(orderedProfiles);
    for (i=0; i<pCount; i++) {
      GWEN_DB_NODE *n;

      n=GWEN_DB_GetFirstGroup(dbProfiles);
      assert(n);
      GWEN_DB_UnlinkGroup(n);
      orderedProfiles[i]=n;
    }
    assert(i==pCount);

    /* sort flat list */
    qsort(orderedProfiles, pCount, sizeof(GWEN_DB_NODE *), _sortSepaProfilesXML_cb);

    /* put sorted groups back into tree */
    for (i=0; i<pCount; i++)
      GWEN_DB_AddGroup(dbProfiles, orderedProfiles[i]);
    free(orderedProfiles);
  }
}


int _sortSepaProfilesXML_cb(const void *a, const void *b)
{
  GWEN_DB_NODE **ppa=(GWEN_DB_NODE **)a;
  GWEN_DB_NODE **ppb=(GWEN_DB_NODE **)b;
  GWEN_DB_NODE *pa=*ppa;
  GWEN_DB_NODE *pb=*ppb;
  int res;

  /* This function is supposed to return a list of profiles in order
   * of decreasing precedence. */
  res=strcmp(GWEN_DB_GetCharValue(pb, "params/sepaType", 0, ""),
             GWEN_DB_GetCharValue(pa, "params/sepaType", 0, ""));
  if (res)
    return res;

  res=strcmp(GWEN_DB_GetCharValue(pb, "name", 0, ""),
             GWEN_DB_GetCharValue(pa, "name", 0, ""));

  return res;
}



AB_TRANSACTION_COMMAND AH_Job_GetSupportedCommand(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->supportedCommand;
}



void AH_Job_SetSupportedCommand(AH_JOB *j, AB_TRANSACTION_COMMAND tc)
{
  assert(j);
  assert(j->usage);
  j->supportedCommand=tc;
}



AB_PROVIDER *AH_Job_GetProvider(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);
  return j->provider;
}



void AH_Job_AddCommand(AH_JOB *j, AB_TRANSACTION *t)
{
  assert(j);
  assert(j->usage);

  if (j->commandList==NULL)
    j->commandList=AB_Transaction_List2_new();
  AB_Transaction_List2_PushBack(j->commandList, t);
}



AB_TRANSACTION_LIST2 *AH_Job_GetCommandList(const AH_JOB *j)
{
  assert(j);
  assert(j->usage);

  return j->commandList;
}



AH_JOB *AH_Job_List_GetById(AH_JOB_LIST *jl, uint32_t id)
{
  if (jl) {
    AH_JOB *j;

    j=AH_Job_List_First(jl);
    while (j) {
      if (AH_Job_GetId(j)==id)
        return j;
      j=AH_Job_List_Next(j);
    }
  }

  return NULL;
}



void AH_Job_SetStatusOnCommands(AH_JOB *j, AB_TRANSACTION_STATUS status)
{
  AB_TRANSACTION_LIST2 *cmdList;

  assert(j);

  cmdList=AH_Job_GetCommandList(j);
  if (cmdList) {
    AB_TRANSACTION_LIST2_ITERATOR *it;

    it=AB_Transaction_List2_First(cmdList);
    if (it) {
      AB_TRANSACTION *t;

      t=AB_Transaction_List2Iterator_Data(it);
      while (t) {
        AB_Transaction_SetStatus(t, status);
        t=AB_Transaction_List2Iterator_Next(it);
      }
      AB_Transaction_List2Iterator_free(it);
    }
  }
}



#include "job_new.c"
#include "job_virtual.c"



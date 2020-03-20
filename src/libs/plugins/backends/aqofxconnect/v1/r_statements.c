/***************************************************************************
 begin       : Mon Jan 13 2020
 copyright   : (C) 2020 by Martin Preuss
 email       : martin@libchipcard.de

 ***************************************************************************
 *          Please see toplevel file COPYING for license details           *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include "r_statements.h"

#include "n_header.h"
#include "n_toofx.h"
#include "n_signon.h"
#include "n_statement.h"
#include "io_network.h"

#include <aqbanking/banking_imex.h>
#include "aqbanking/i18n_l.h"

#include <gwenhywfar/gui.h>
#include <gwenhywfar/text.h>




int AO_V1_RequestStatements(AB_PROVIDER *pro, AB_USER *u, AB_ACCOUNT *a, AB_TRANSACTION *j,
                            AB_IMEXPORTER_CONTEXT *ctx)
{
  AB_BANKING *ab;
  GWEN_XMLNODE *xmlRoot;
  GWEN_XMLNODE *xmlOfx;
  GWEN_XMLNODE *xmlNode;
  GWEN_BUFFER *bufRequest;
  GWEN_BUFFER *bufResponse=NULL;
  int rv;

  ab=AB_Provider_GetBanking(pro);
  assert(ab);

  /* prepare XML request */
  xmlRoot=GWEN_XMLNode_new(GWEN_XMLNodeTypeTag, "root");

  xmlOfx=GWEN_XMLNode_new(GWEN_XMLNodeTypeTag, "OFX");
  GWEN_XMLNode_AddChild(xmlRoot, xmlOfx);

  xmlNode=AO_Provider_MkSignOnNode(u);
  if (xmlNode)
    GWEN_XMLNode_AddChild(xmlOfx, xmlNode);

  xmlNode=AO_Provider_MkStatementRqNode(u, a, j);
  if (xmlNode)
    GWEN_XMLNode_AddChild(xmlOfx, xmlNode);

  /* create and fill request buffer */
  bufRequest=GWEN_Buffer_new(0, 256, 0, 1);

  rv=AO_V1_AddOfxHeaders(pro, u, bufRequest, "USASCII");
  if (rv<0) {
    DBG_INFO(AQOFXCONNECT_LOGDOMAIN, "here (%d)", rv);
    GWEN_Buffer_free(bufRequest);
    GWEN_XMLNode_free(xmlRoot);
    return rv;
  }

  rv=AO_V1_XmlToOfx(xmlOfx, bufRequest, "USASCII");
  if (rv<0) {
    DBG_INFO(AQOFXCONNECT_LOGDOMAIN, "here (%d)", rv);
    GWEN_Buffer_free(bufRequest);
    GWEN_XMLNode_free(xmlRoot);
    return rv;
  }
  GWEN_XMLNode_free(xmlRoot);

#if 0
  DBG_ERROR(AQOFXCONNECT_LOGDOMAIN, "OFX request:");
  fprintf(stderr, "%s\n", GWEN_Buffer_GetStart(bufRequest));
  GWEN_Text_LogString(GWEN_Buffer_GetStart(bufRequest),
                      GWEN_Buffer_GetUsedBytes(bufRequest),
                      AQOFXCONNECT_LOGDOMAIN,
                      GWEN_LoggerLevel_Error);
#endif

  /* exchange messages */
  rv=AO_Provider_SendAndReceive(pro, u,
                                (const uint8_t *) GWEN_Buffer_GetStart(bufRequest),
                                GWEN_Buffer_GetUsedBytes(bufRequest),
                                &bufResponse);
  if (rv) {
    DBG_INFO(AQOFXCONNECT_LOGDOMAIN, "here (%d)", rv);
    GWEN_Buffer_free(bufRequest);
    return rv;
  }
  GWEN_Buffer_free(bufRequest);


#if 0
  DBG_ERROR(AQOFXCONNECT_LOGDOMAIN, "OFX response:");
  GWEN_Text_LogString(GWEN_Buffer_GetStart(bufResponse),
                      GWEN_Buffer_GetUsedBytes(bufResponse),
                      AQOFXCONNECT_LOGDOMAIN,
                      GWEN_LoggerLevel_Error);
#endif

  /* parse response */
  GWEN_Gui_ProgressLog(0, GWEN_LoggerLevel_Info, I18N("Parsing response..."));
  DBG_INFO(AQOFXCONNECT_LOGDOMAIN, "Importing OFX version 1 (sgml)");
  rv=AB_Banking_ImportFromBufferLoadProfile(ab, "ofx", ctx, "default", NULL,
                                            (const uint8_t *) GWEN_Buffer_GetStart(bufResponse),
                                            GWEN_Buffer_GetUsedBytes(bufResponse));
  if (rv<0) {
    DBG_ERROR(AQOFXCONNECT_LOGDOMAIN, "Bad data in OFX response (error: %d):", rv);
    GWEN_Text_LogString(GWEN_Buffer_GetStart(bufResponse),
                        GWEN_Buffer_GetUsedBytes(bufResponse),
                        AQOFXCONNECT_LOGDOMAIN,
                        GWEN_LoggerLevel_Error);
    GWEN_Buffer_free(bufResponse);
    return rv;
  }


  GWEN_Buffer_free(bufResponse);


  return 0;
}




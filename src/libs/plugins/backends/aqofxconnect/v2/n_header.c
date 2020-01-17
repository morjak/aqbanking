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


#include "n_header.h"
#include "n_utils.h"

#include "aqofxconnect/user.h"




/*
 *

 */

GWEN_XMLNODE *AO_V2_MkOfxHeader(AB_USER *u)
{
  GWEN_XMLNODE *xmlOfx;
  const char *s;

  xmlOfx=GWEN_XMLNode_new(GWEN_XMLNodeTypeTag, "?OFX");

  GWEN_XMLNode_SetProperty(xmlOfx, "OFXHEADER", "200");

  s=AO_User_GetHeaderVer(u);
  GWEN_XMLNode_SetProperty(xmlOfx, "VERSION", s?s:"220");

  s=AO_User_GetSecurityType(u);
  GWEN_XMLNode_SetProperty(xmlOfx, "SECURITY", s?s:"NONE");

  AO_V2_Util_SetCurrentTimeValue(xmlOfx, AO_User_GetFlags(u), "NEWFILEUID");

  GWEN_XMLNode_SetProperty(xmlOfx, "OLDFILEUID", "NONE");

  return xmlOfx;
}



GWEN_XMLNODE *AO_V2_MkXmlHeader(void)
{
  GWEN_XMLNODE *xmlNode;

  xmlNode=GWEN_XMLNode_new(GWEN_XMLNodeTypeTag, "?xml");

  GWEN_XMLNode_SetProperty(xmlNode, "version", "1.0");
  GWEN_XMLNode_SetProperty(xmlNode, "encoding", "utf-8");
  GWEN_XMLNode_SetProperty(xmlNode, "standalone", "no");

  return xmlNode;
}




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


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "wizard.h"

#include <qlabel.h>
#include <qpushbutton.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qtextbrowser.h>
#include <qapplication.h>

#include <gwenhywfar/debug.h>



Wizard::Wizard(QBanking *qb,
               WizardInfo *wInfo,
               const QString &title,
               QWidget* parent,
               const char* name, bool modal)
:WizardUi(parent, name)
,_app(qb)
,_wInfo(wInfo)
,_lastActionWidget(0){
  setModal(modal);
  if (!title.isEmpty())
    setCaption(title);
}



Wizard::~Wizard() {
}



QBanking *Wizard::getBanking() {
  return _app;
}



void Wizard::addAction(WizardAction *a) {
  addPage(a, a->getDescription());
  _lastActionWidget=a;
}



WizardInfo *Wizard::getWizardInfo() {
  return _wInfo;
}



void Wizard::log(AB_BANKING_LOGLEVEL level, const QString &text) {
  QString tmp;

  tmp+=_logtext;
  tmp+="<tr><td>" + 
      QTime::currentTime().toString() +
      "</td><td>";
  if (level<=AB_Banking_LogLevelError) {
    tmp+=QString("<font color=\"red\">%1</font>").arg(text);
  }
  else if (level==AB_Banking_LogLevelWarn) {
    tmp+=QString("<font color=\"blue\">%1</font>").arg(text);
  }
  else if (level==AB_Banking_LogLevelInfo) {
    tmp+=QString("<font color=\"green\">%1</font>").arg(text);
  }
  else if (level>=AB_Banking_LogLevelDebug) {
    return;
  }
  else
    tmp+=text;

  tmp+=QString("</td></tr>");
  _logtext=tmp;
  tmp="<qt><table>"+_logtext+"</table></qt>";
  /*
  logBrowser->setText(tmp);
  logBrowser->scrollToBottom();

  qApp->processEvents();
  */
}



void Wizard::setDescription(const QString &s) {
  textLabel->setText(s);
}



QWidget *Wizard::getWidgetAsParent() {
  return (QWidget*)this;
}



int Wizard::exec() {
  return WizardUi::exec();
}




void Wizard::back() {
  QWidget *w;

  QWizard::back();
  w=currentPage();
  if (w!=startPage) {
    WizardAction *p;

    p=dynamic_cast<WizardAction*>(w);
    assert(p);

    p->undo();
  }
}



void Wizard::next() {
  QWidget *w;

  w=currentPage();
  if (w==startPage) {
    QWizard::next();
  }
  else {
    WizardAction *p;

    p=dynamic_cast<WizardAction*>(w);
    assert(p);
    if (p->apply()) {
      QWizard::next();
      w=currentPage();
      p=dynamic_cast<WizardAction*>(w);
      assert(p);
      p->enter();
      if (w==_lastActionWidget)
        setFinishEnabled(w, TRUE);
      else
        setFinishEnabled(w, FALSE);
    }
  }
}










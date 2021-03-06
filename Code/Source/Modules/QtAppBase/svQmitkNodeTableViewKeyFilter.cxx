/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "svQmitkNodeTableViewKeyFilter.h"

#include <QKeyEvent>
#include <QKeySequence>
#include "svQmitkDataManager.h"

svQmitkNodeTableViewKeyFilter::svQmitkNodeTableViewKeyFilter( QObject* _DataManager )
: QObject(_DataManager)
{
}

bool svQmitkNodeTableViewKeyFilter::eventFilter( QObject *obj, QEvent *event )
{
  svQmitkDataManager* DataManager = qobject_cast<svQmitkDataManager*>(this->parent());
  if (event->type() == QEvent::KeyPress && DataManager)
  {

//    QKeySequence _MakeAllInvisible = QKeySequence(nodeTableKeyPrefs->Get("Make all nodes invisible", "Ctrl+, V"));
//    QKeySequence _ToggleVisibility = QKeySequence(nodeTableKeyPrefs->Get("Toggle visibility of selected nodes", "V"));
//    QKeySequence _DeleteSelectedNodes = QKeySequence(nodeTableKeyPrefs->Get("Delete selected nodes", "Del"));
//    QKeySequence _Reinit = QKeySequence(nodeTableKeyPrefs->Get("Reinit selected nodes", "R"));
//    QKeySequence _GlobalReinit = QKeySequence(nodeTableKeyPrefs->Get("Global Reinit", "Ctrl+, R"));
//    QKeySequence _ShowInfo = QKeySequence(nodeTableKeyPrefs->Get("Show Node Information", "Ctrl+, I"));

    QKeySequence _MakeAllInvisible = QKeySequence("Ctrl+, V");
    QKeySequence _ToggleVisibility = QKeySequence("V");
    QKeySequence _DeleteSelectedNodes = QKeySequence("Del");
    QKeySequence _Reinit = QKeySequence("R");
    QKeySequence _GlobalReinit = QKeySequence("Ctrl+, R");
    QKeySequence _ShowInfo = QKeySequence("Ctrl+, I");


    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    QKeySequence _KeySequence = QKeySequence(keyEvent->modifiers(), keyEvent->key());
    // if no modifier was pressed the sequence is now empty
    if(_KeySequence.isEmpty())
      _KeySequence = QKeySequence(keyEvent->key());

    if(_KeySequence == _MakeAllInvisible)
    {
      // trigger deletion of selected node(s)
      DataManager->MakeAllNodesInvisible(true);
      // return true: this means the delete key event is not send to the table
      return true;
    }
    else if(_KeySequence == _DeleteSelectedNodes)
    {
      // trigger deletion of selected node(s)
      DataManager->RemoveSelectedNodes(true);
      // return true: this means the delete key event is not send to the table
      return true;
    }
    else if(_KeySequence == _ToggleVisibility)
    {
      // trigger deletion of selected node(s)
      DataManager->ToggleVisibilityOfSelectedNodes(true);
      // return true: this means the delete key event is not send to the table
      return true;
    }
    else if(_KeySequence == _Reinit)
    {
      DataManager->ReinitSelectedNodes(true);
      return true;
    }
    else if(_KeySequence == _GlobalReinit)
    {
      DataManager->GlobalReinit(true);
      return true;
    }
    else if(_KeySequence == _ShowInfo)
    {
      DataManager->ShowInfoDialogForSelectedNodes(true);
      return true;
    }
  }
  // standard event processing
  return QObject::eventFilter(obj, event);
}

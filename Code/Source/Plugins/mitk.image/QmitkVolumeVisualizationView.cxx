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

#include "QmitkVolumeVisualizationView.h"

#include <QComboBox>
#include <QTreeView>

#include <vtkVersionMacros.h>

#include <mitkProperties.h>
#include <mitkNodePredicateDataType.h>

#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>
#include <mitkTransferFunctionInitializer.h>
#include "mitkHistogramGenerator.h"
#include "QmitkPiecewiseFunctionCanvas.h"
#include "QmitkColorTransferFunctionCanvas.h"

#include "mitkBaseRenderer.h"

#include "mitkVtkVolumeRenderingProperty.h"

#include <mitkRenderingManager.h>

#include <QToolTip>

enum RenderMode
{
  RM_CPU_COMPOSITE_RAYCAST = 0,
  RM_CPU_MIP_RAYCAST       = 1,
  RM_GPU_COMPOSITE_SLICING = 2,
  RM_GPU_COMPOSITE_RAYCAST = 3,
  RM_GPU_MIP_RAYCAST       = 4
};

const QString QmitkVolumeVisualizationView::EXTENSION_ID = "mitk.volumevisualization";

QmitkVolumeVisualizationView::QmitkVolumeVisualizationView()
: m_Controls(NULL)
{
}

QmitkVolumeVisualizationView::~QmitkVolumeVisualizationView()
{
    //Remove all registered actions from each descriptor
    for (std::vector< std::pair< QmitkNodeDescriptor*, QAction* > >::iterator it = mDescriptorActionList.begin();it != mDescriptorActionList.end(); it++)
    {
      // first== the NodeDescriptor; second== the registered QAction
      (it->first)->RemoveAction(it->second);
    }
}

void QmitkVolumeVisualizationView::CreateQtPartControl(QWidget* parent)
{

  if (!m_Controls)
  {
    m_Controls = new Ui::QmitkVolumeVisualizationViewControls;
    m_Controls->setupUi(parent);

//    parent->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
//    parent->setMaximumWidth(500);

    // Fill the tf presets in the generator widget
    std::vector<std::string> names;
    mitk::TransferFunctionInitializer::GetPresetNames(names);
    for (std::vector<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it)
    {
      m_Controls->m_TransferFunctionGeneratorWidget->AddPreset(QString::fromStdString(*it));
    }

    m_Controls->m_RenderMode->addItem("CPU raycast");
    m_Controls->m_RenderMode->addItem("CPU MIP raycast");
    m_Controls->m_RenderMode->addItem("GPU slicing");
    m_Controls->m_RenderMode->addItem("GPU raycast");
    m_Controls->m_RenderMode->addItem("GPU MIP raycast");

    connect( m_Controls->m_EnableRenderingCB, SIGNAL( toggled(bool) ),this, SLOT( OnEnableRendering(bool) ));
    connect( m_Controls->m_EnableLOD, SIGNAL( toggled(bool) ),this, SLOT( OnEnableLOD(bool) ));
    connect( m_Controls->m_RenderMode, SIGNAL( activated(int) ),this, SLOT( OnRenderMode(int) ));

    connect( m_Controls->m_TransferFunctionGeneratorWidget, SIGNAL( SignalUpdateCanvas( ) ),   m_Controls->m_TransferFunctionWidget, SLOT( OnUpdateCanvas( ) ) );
    connect( m_Controls->m_TransferFunctionGeneratorWidget, SIGNAL(SignalTransferFunctionModeChanged(int)), SLOT(OnMitkInternalPreset(int)));

    m_Controls->m_EnableRenderingCB->setEnabled(false);
    m_Controls->m_EnableLOD->setEnabled(false);
    m_Controls->m_RenderMode->setEnabled(false);
    m_Controls->m_TransferFunctionWidget->setEnabled(false);
    m_Controls->m_TransferFunctionGeneratorWidget->setEnabled(false);

    m_Controls->m_SelectedImageLabel->hide();
    m_Controls->m_ErrorImageLabel->hide();

    connect(GetDataManager()->GetTreeView(), SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(ShowVolumeRenderingPaneForImage()));

    QmitkNodeDescriptor* imageDataNodeDescriptor = getNodeDescriptorManager()->GetDescriptor("Image");
    QAction* vrAction = new QAction(QIcon(":volumerendering.png"), "Volume Rendering", this);
    QObject::connect( vrAction, SIGNAL( triggered() ) , this, SLOT( ShowVolumeRenderingPane() ) );
    imageDataNodeDescriptor->AddAction(vrAction,false);
    mDescriptorActionList.push_back(std::pair<QmitkNodeDescriptor*, QAction*>(imageDataNodeDescriptor, vrAction));

  }
}

void QmitkVolumeVisualizationView::Activated()
{
//    GetDisplayWidget()->SetWidgetPlanesVisibility(false);
//    GetDisplayWidget()->changeLayoutToBig3D();
}

void QmitkVolumeVisualizationView::Deactivated()
{
//    GetDisplayWidget()->SetWidgetPlanesVisibility(true);
//    GetDisplayWidget()->changeLayoutToDefault();
}

void QmitkVolumeVisualizationView::OnMitkInternalPreset( int mode )
{
  if (m_SelectedNode.IsNull()) return;

  mitk::DataNode::Pointer node(m_SelectedNode.GetPointer());
  mitk::TransferFunctionProperty::Pointer transferFuncProp;
  if (node->GetProperty(transferFuncProp, "TransferFunction"))
  {
    //first item is only information
    if( --mode == -1 )
      return;

    // -- Creat new TransferFunction
    mitk::TransferFunctionInitializer::Pointer tfInit = mitk::TransferFunctionInitializer::New(transferFuncProp->GetValue());
    tfInit->SetTransferFunctionMode(mode);
    RequestRenderWindowUpdate();
    m_Controls->m_TransferFunctionWidget->OnUpdateCanvas();
  }
}


void QmitkVolumeVisualizationView::OnSelectionChanged(const QList<mitk::DataNode::Pointer>& nodes)
{
  bool weHadAnImageButItsNotThreeDeeOrFourDee = false;

  mitk::DataNode::Pointer node;

  foreach (mitk::DataNode::Pointer currentNode, nodes)
  {
    if( currentNode.IsNotNull() && dynamic_cast<mitk::Image*>(currentNode->GetData()) )
    {
      if( dynamic_cast<mitk::Image*>(currentNode->GetData())->GetDimension()>=3 )
      {
        if (node.IsNull())
        {
          node = currentNode;
        }
      }
      else
      {
        weHadAnImageButItsNotThreeDeeOrFourDee = true;
      }
    }
  }

  if( node.IsNotNull() )
  {
    m_Controls->m_NoSelectedImageLabel->hide();
    m_Controls->m_ErrorImageLabel->hide();
    m_Controls->m_SelectedImageLabel->show();

    std::string  infoText;

    if (node->GetName().empty())
      infoText = std::string("Selected Image: [currently selected image has no name]");
    else
      infoText = std::string("Selected Image: ") + node->GetName();

    m_Controls->m_SelectedImageLabel->setText( QString( infoText.c_str() ) );

    m_SelectedNode = node;
  }
  else
  {
    if(weHadAnImageButItsNotThreeDeeOrFourDee)
    {
      m_Controls->m_NoSelectedImageLabel->hide();
      m_Controls->m_ErrorImageLabel->show();
      std::string  infoText;
      infoText = std::string("only 3D or 4D images are supported");
      m_Controls->m_ErrorImageLabel->setText( QString( infoText.c_str() ) );
    }
    else
    {
      m_Controls->m_SelectedImageLabel->hide();
      m_Controls->m_ErrorImageLabel->hide();
      m_Controls->m_NoSelectedImageLabel->show();
    }

    m_SelectedNode = 0;
  }

  UpdateInterface();
}


void QmitkVolumeVisualizationView::UpdateInterface()
{
  if(m_SelectedNode.IsNull())
  {
    // turnoff all
    m_Controls->m_EnableRenderingCB->setChecked(false);
    m_Controls->m_EnableRenderingCB->setEnabled(false);

    m_Controls->m_EnableLOD->setChecked(false);
    m_Controls->m_EnableLOD->setEnabled(false);

    m_Controls->m_RenderMode->setCurrentIndex(0);
    m_Controls->m_RenderMode->setEnabled(false);

    m_Controls->m_TransferFunctionWidget->SetDataNode(0);
    m_Controls->m_TransferFunctionWidget->setEnabled(false);

    m_Controls->m_TransferFunctionGeneratorWidget->SetDataNode(0);
    m_Controls->m_TransferFunctionGeneratorWidget->setEnabled(false);
    return;
  }

  bool enabled = false;

  m_SelectedNode->GetBoolProperty("volumerendering",enabled);
  m_Controls->m_EnableRenderingCB->setEnabled(true);
  m_Controls->m_EnableRenderingCB->setChecked(enabled);

  if(!enabled)
  {
    // turnoff all except volumerendering checkbox
    m_Controls->m_EnableLOD->setChecked(false);
    m_Controls->m_EnableLOD->setEnabled(false);

    m_Controls->m_RenderMode->setCurrentIndex(0);
    m_Controls->m_RenderMode->setEnabled(false);

    m_Controls->m_TransferFunctionWidget->SetDataNode(0);
    m_Controls->m_TransferFunctionWidget->setEnabled(false);

    m_Controls->m_TransferFunctionGeneratorWidget->SetDataNode(0);
    m_Controls->m_TransferFunctionGeneratorWidget->setEnabled(false);
    return;
  }

  // otherwise we can activate em all
  enabled = false;
  m_SelectedNode->GetBoolProperty("volumerendering.uselod",enabled);
  m_Controls->m_EnableLOD->setEnabled(true);
  m_Controls->m_EnableLOD->setChecked(enabled);

  m_Controls->m_RenderMode->setEnabled(true);

  // Determine Combo Box mode
  {
    bool usegpu=false;
    bool useray=false;
    bool usemip=false;
    m_SelectedNode->GetBoolProperty("volumerendering.usegpu",usegpu);
    m_SelectedNode->GetBoolProperty("volumerendering.useray",useray);
    m_SelectedNode->GetBoolProperty("volumerendering.usemip",usemip);

    int mode = 0;

    if(useray)
    {
      if(usemip)
        mode=RM_GPU_MIP_RAYCAST;
      else
        mode=RM_GPU_COMPOSITE_RAYCAST;
    }
    else if(usegpu)
      mode=RM_GPU_COMPOSITE_SLICING;
    else
    {
      if(usemip)
        mode=RM_CPU_MIP_RAYCAST;
      else
        mode=RM_CPU_COMPOSITE_RAYCAST;
    }

    m_Controls->m_RenderMode->setCurrentIndex(mode);
  }

  m_Controls->m_TransferFunctionWidget->SetDataNode(m_SelectedNode);
  m_Controls->m_TransferFunctionWidget->setEnabled(true);
  m_Controls->m_TransferFunctionGeneratorWidget->SetDataNode(m_SelectedNode);
  m_Controls->m_TransferFunctionGeneratorWidget->setEnabled(true);
}


void QmitkVolumeVisualizationView::OnEnableRendering(bool state)
{
  if(m_SelectedNode.IsNull())
    return;

  m_SelectedNode->SetProperty("volumerendering",mitk::BoolProperty::New(state));
  UpdateInterface();
  RequestRenderWindowUpdate();
}

void QmitkVolumeVisualizationView::OnEnableLOD(bool state)
{
  if(m_SelectedNode.IsNull())
    return;

  m_SelectedNode->SetProperty("volumerendering.uselod",mitk::BoolProperty::New(state));
  RequestRenderWindowUpdate();
}

void QmitkVolumeVisualizationView::OnRenderMode(int mode)
{
  if(m_SelectedNode.IsNull())
    return;

  bool usegpu=mode==RM_GPU_COMPOSITE_SLICING;
  bool useray=(mode==RM_GPU_COMPOSITE_RAYCAST)||(mode==RM_GPU_MIP_RAYCAST);
  bool usemip=(mode==RM_GPU_MIP_RAYCAST)||(mode==RM_CPU_MIP_RAYCAST);

  m_SelectedNode->SetProperty("volumerendering.usegpu",mitk::BoolProperty::New(usegpu));
  m_SelectedNode->SetProperty("volumerendering.useray",mitk::BoolProperty::New(useray));
  m_SelectedNode->SetProperty("volumerendering.usemip",mitk::BoolProperty::New(usemip));

  RequestRenderWindowUpdate();
}

void QmitkVolumeVisualizationView::SetFocus()
{

}

void QmitkVolumeVisualizationView::NodeRemoved(const mitk::DataNode* node)
{
  if(m_SelectedNode == node)
  {
    m_SelectedNode=0;
    m_Controls->m_SelectedImageLabel->hide();
    m_Controls->m_ErrorImageLabel->hide();
    m_Controls->m_NoSelectedImageLabel->show();
    UpdateInterface();
  }
}


//void QmitkVolumeVisualizationView::AppendMenuActions(QMenu *menu){

//    QList<mitk::DataNode::Pointer> selectedNodes=GetCurrentSelection();

//    if(Is3DImage(selectedNodes))
//    {
//        QAction* vrAction = new QAction(QIcon(":volumerendering.png"), "Volume Rendering", this);
//        QObject::connect( vrAction, SIGNAL( triggered() )
//        , this, SLOT( ShowVolumeRenderingPane() ) );

//        menu->addAction(vrAction);
//    }
//}

bool QmitkVolumeVisualizationView::Is3DImage(QList<mitk::DataNode::Pointer> nodes)
{

    if(!nodes.isEmpty())
    {
        mitk::DataNode::Pointer node=nodes.first();
        if( node.IsNotNull() && dynamic_cast<mitk::Image*>(node->GetData()) )
        {
          if( dynamic_cast<mitk::Image*>(node->GetData())->GetDimension()>=3 )
          {
               return true;
          }

        }

    }
    return false;
}

void QmitkVolumeVisualizationView::ShowVolumeRenderingPaneForImage()
{
    QList<mitk::DataNode::Pointer> selectedNodes=GetCurrentSelection();
    if(Is3DImage(selectedNodes))
    {
        ShowVolumeRenderingPane();
    }
}

void QmitkVolumeVisualizationView::ShowVolumeRenderingPane()
{
    useExtension(EXTENSION_ID);
}

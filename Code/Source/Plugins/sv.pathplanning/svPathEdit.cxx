#include "svPathEdit.h"
#include "svPathCreate.h"
#include "svPathSmooth.h"
#include "svPath.h"
#include "svPathOperation.h"
#include "svPathDataInteractor.h"
#include "svMath3.h"
#include <QmitkRenderWindow.h>

// mitk
#include <mitkDataStorage.h>
#include "mitkDataNode.h"
#include "mitkProperties.h"
//#include <mitkOperationEvent.h>
#include <mitkUndoController.h>
#include <mitkNodePredicateDataType.h>

#include <usModuleRegistry.h>

// Qt
#include <QMessageBox>
#include <QShortcut>
#include <QTreeView>

#include <iostream>
using namespace std;

const QString svPathEdit::EXTENSION_ID = "sv.pathedit";

svPathEdit::svPathEdit():
    ui(new Ui::svPathEdit),
    m_ParentNodeOriginalVisible(true),
    m_PathChangeObserverTag(0),
    m_PathNode(NULL),
    m_Path(NULL),
    m_SmoothWidget(NULL)
{
}

svPathEdit::~svPathEdit()
{
    for (std::vector< std::pair< QmitkNodeDescriptor*, QAction* > >::iterator it = mDescriptorActionList.begin();it != mDescriptorActionList.end(); it++)
    {
        // first== the NodeDescriptor; second== the registered QAction
        (it->first)->RemoveAction(it->second);
    }

}

void svPathEdit::CreateQtPartControl( QWidget *parent )
{
    m_Parent=parent;
    ui->setupUi(parent);
    //    parent->setMaximumWidth(500);

    ui->resliceSlider->SetDisplayWidget(GetDisplayWidget());
//    ui->resliceSlider->setCheckBoxVisible(true);
    ui->resliceSlider->SetResliceMode(mitk::ExtractSliceFilter::RESLICE_CUBIC);

    connect(ui->btnChange, SIGNAL(clicked()), this, SLOT(ChangePath()) );
    connect(ui->buttonSmartAdd, SIGNAL(clicked()), this, SLOT(SmartAdd()) );
    connect(ui->buttonInsertAbove, SIGNAL(clicked()), this, SLOT(InsertPointAbove()) );
    connect(ui->btnAddToEnd, SIGNAL(clicked()), this, SLOT(AddToEnd()) );
    connect(ui->btnAddToTop, SIGNAL(clicked()), this, SLOT(AddToTop()) );
    connect(ui->buttonDelete, SIGNAL(clicked()), this, SLOT(DeleteSelected()) );
    connect(ui->listWidget,SIGNAL(clicked(const QModelIndex&)), this, SLOT(SelectItem(const QModelIndex&)) );

    connect(ui->btnSmooth,SIGNAL(clicked()), this, SLOT(SmoothCurrentPath()) );

    QShortcut *shortcut = new QShortcut(QKeySequence("Ctrl+A"), parent);
    connect(shortcut, SIGNAL(activated()), ui->buttonSmartAdd, SLOT(click()));

    mitk::DataNode::Pointer parentNode=this->GetDataStorage()->GetNamedNode("Paths");
    if(parentNode) parentNode->GetBoolProperty("visible", m_ParentNodeOriginalVisible);

    connect(GetDataManager()->GetTreeView(), SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(ShowPathEditPaneForPath()));

    QmitkNodeDescriptor* dataNodeDescriptor = getNodeDescriptorManager()->GetDescriptor("svPath");
    QAction* action = new QAction(QIcon(":pathedit.png"), "Edit Path", this);
    QObject::connect( action, SIGNAL( triggered() ) , this, SLOT( ShowPathEditPane() ) );
    dataNodeDescriptor->AddAction(action,false);
    mDescriptorActionList.push_back(std::pair<QmitkNodeDescriptor*, QAction*>(dataNodeDescriptor, action));
}

void svPathEdit::Activated()
{
    //    ClearAll();
    OnSelectionChanged(GetCurrentSelection());
}

void svPathEdit::Deactivated()
{
    ui->resliceSlider->turnOnReslice(false);
    ClearAll();
}

void svPathEdit::OnSelectionChanged(const QList<mitk::DataNode::Pointer>& nodes )
{
    if(!IsActivated())
    {
        return;
    }

    if(nodes.size()==0)
    {
        ClearAll();
        setEnabled(false);
        return;
    }

    mitk::DataNode::Pointer pathNode=nodes.front();

    if(m_PathNode==pathNode)
    {
        return;
    }

    ClearAll();

    m_PathNode=pathNode;
    m_Path=dynamic_cast<svPath*>(pathNode->GetData());

    if(!m_Path)
    {
        ClearAll();
        setEnabled(false);
        return;
    }

    setEnabled(true);

    ui->labelPathName->setText(QString::fromStdString(m_PathNode->GetName()));

    int timeStep=GetTimeStep(m_Path);
    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    ui->labelPathPointNumber->setText(QString::number(pathElement->GetPathPointNumber()));

    UpdateGUI();

    m_DataInteractor = svPathDataInteractor::New();
    m_DataInteractor->LoadStateMachine("svPath.xml", us::ModuleRegistry::GetModule("svPath"));
    m_DataInteractor->SetEventConfig("svPathConfig.xml", us::ModuleRegistry::GetModule("svPath"));
    m_DataInteractor->SetDataNode(m_PathNode);

    //Add Observer
    itk::SimpleMemberCommand<svPathEdit>::Pointer pathChangeCommand = itk::SimpleMemberCommand<svPathEdit>::New();
    pathChangeCommand->SetCallbackFunction(this, &svPathEdit::UpdateGUI);
    m_PathChangeObserverTag = m_Path->AddObserver( svPathEvent(), pathChangeCommand);

    itk::SimpleMemberCommand<svPathEdit>::Pointer pointMoveCommand = itk::SimpleMemberCommand<svPathEdit>::New();
    pointMoveCommand->SetCallbackFunction(this, &svPathEdit::UpdateSlice);
    m_PointMoveObserverTag = m_Path->AddObserver( svPathFinishMovePointEvent(), pointMoveCommand);

     SetupResliceSlider();

}

void svPathEdit::NodeChanged(const mitk::DataNode* node)
{
    if(node->GetName()=="Paths"){//need change; using name is not good
        bool currentVisible=false;
        node->GetBoolProperty("visible", currentVisible);
        if(currentVisible!=m_ParentNodeOriginalVisible){
            mitk::DataStorage::SetOfObjects::ConstPointer rs = this->GetDataStorage()->GetDerivations(node);
            for(int i=0;i<rs->size();i++){
                rs->GetElement(i)->SetVisibility(currentVisible);
            }
            m_ParentNodeOriginalVisible=currentVisible;
        }

    }

}

void svPathEdit::NodeAdded(const mitk::DataNode* node)
{
}


void svPathEdit::NodeRemoved(const mitk::DataNode* node)
{
    OnSelectionChanged(GetCurrentSelection());
}

void svPathEdit::ShowPathEditPane()
{
    useExtension(EXTENSION_ID);
}

void svPathEdit::OpenPathCreateDialog()
{
    useExtension(svPathCreate::EXTENSION_ID);
}


void svPathEdit::ShowPathEditPaneForPath()
{
    QList<mitk::DataNode::Pointer> selectedNodes=GetCurrentSelection();
    if(IsPath(selectedNodes))
    {
        ShowPathEditPane();
    }
}

bool svPathEdit::IsPath(QList<mitk::DataNode::Pointer> nodes)
{
    if(!nodes.isEmpty())
    {
        mitk::DataNode::Pointer node=nodes.first();
        if( node.IsNotNull() && dynamic_cast<svPath*>(node->GetData()) )
        {
            return true;
        }

    }
    return false;
}

void svPathEdit::ClearAll()
{
    //Remove Observer
    if(m_Path && m_PathChangeObserverTag)
    {
        m_Path->RemoveObserver(m_PathChangeObserverTag);
    }

    if(m_PathNode)
    {
        m_PathNode->SetDataInteractor(NULL);
        m_DataInteractor=NULL;
    }

    m_Path=NULL;
    m_PathNode=NULL;

    ui->labelPathName->setText("");
    ui->labelPathPointNumber->setText("");
    ui->listWidget->clear();
}

void svPathEdit::UpdateGUI()
{
    if(m_Path==NULL) return;

    int timeStep=GetTimeStep(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    //Update Path Point Number
    ui->labelPathPointNumber->setText(QString::number(pathElement->GetPathPointNumber()));

    //Update Control Point List
    ui->listWidget->clear();
    double pt[3];
    char x[12];
    char y[12];
    char z[12];
    char xyz[50];
    for(int index=0;index<pathElement->GetControlPointNumber();index++){

        mitk::Point3D pt= pathElement->GetControlPoint(index);
        sprintf(x,"%.4f",pt[0]);
        sprintf(y,"%.4f",pt[1]);
        sprintf(z,"%.4f",pt[2]);
        sprintf(xyz,"%d: %8s, %8s, %8s",index,x,y,z);
        ui->listWidget->addItem(xyz);
    }

    int selectedIndex=pathElement->GetControlPointSelectedIndex();
    if(selectedIndex!=-2)
    {
        QModelIndex mIndex=ui->listWidget->model()->index(selectedIndex,0);
        ui->listWidget->selectionModel()->select(mIndex, QItemSelectionModel::ClearAndSelect);
    }

    //Update Reslice
    switch(m_Path->GetOperationType())
    {
    case svPathOperation::OpINSERTCONTROLPOINT:
//    case svPathOperation::OpMOVECONTROLPOINT:
        SetupResliceSlider();
        if(ui->resliceSlider->isResliceOn())
            ui->resliceSlider->moveToPathPosPoint(m_Path->GetNewControlPoint());
        break;
    case svPathOperation::OpREMOVECONTROLPOINT:
    {
        mitk::Point3D lastPoint=ui->resliceSlider->getCurrentPathPoint().pos;
        SetupResliceSlider();
        if(ui->resliceSlider->isResliceOn())
            ui->resliceSlider->moveToClosestPathPosPoint(lastPoint);
    }
        break;
    case svPathOperation::OpSETPATHELEMENT:
        SetupResliceSlider();
        break;
    }

}

void svPathEdit::UpdateSlice()
{
    if(m_Path->GetOperationType()==svPathOperation::OpMOVECONTROLPOINT)
    {
        SetupResliceSlider();
        if(ui->resliceSlider->isResliceOn())
            ui->resliceSlider->moveToPathPosPoint(m_Path->GetNewControlPoint());
    }
}

void svPathEdit::SetupResliceSlider()
{
    mitk::DataNode::Pointer imageNode=NULL;
    mitk::NodePredicateDataType::Pointer isProjFolder = mitk::NodePredicateDataType::New("svProjectFolder");
    mitk::DataStorage::SetOfObjects::ConstPointer rs=GetDataStorage()->GetSources (m_PathNode,isProjFolder,false);
    if(rs->size()>0)
    {
        mitk::DataNode::Pointer projFolderNode=rs->GetElement(0);

        rs=GetDataStorage()->GetDerivations (projFolderNode,mitk::NodePredicateDataType::New("svImageFolder"));
        if(rs->size()>0)
        {

            mitk::DataNode::Pointer imageFolderNode=rs->GetElement(0);
            rs=GetDataStorage()->GetDerivations(imageFolderNode);
            if(rs->size()<1) return;
            imageNode=rs->GetElement(0);
            m_Image=dynamic_cast<mitk::Image*>(imageNode->GetData());

        }
    }

    if(imageNode.IsNull()) return;

    if(m_Path==NULL)
        return;

    int timeStep=GetTimeStep(m_Path);
    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    if(pathElement->GetControlPointNumber()>1)
    {
//        ui->resliceSlider->setPathPoints(pathElement->GetPathPoints());
        int startingIndex=0;
        double realBounds[6];
        GetImageRealBounds(realBounds);
        ui->resliceSlider->setPathPoints(pathElement->GetExtendedPathPoints(realBounds,GetVolumeImageSpacing(),startingIndex));
        ui->resliceSlider->SetStartingSlicePos(startingIndex);
        ui->resliceSlider->setImageNode(imageNode);
        ui->resliceSlider->setResliceSize(5.0);

        ui->resliceSlider->setEnabled(true);
        if(ui->resliceSlider->isResliceOn())
            ui->resliceSlider->updateReslice();
    }
    else
    {
        ui->resliceSlider->turnOnReslice(false);
        ui->resliceSlider->setEnabled(false);
    }
}

void svPathEdit::GetImageRealBounds(double realBounds[6])
{
    for(int i=0;i<6;i++)
        realBounds[i]=0.0;

    if(m_Image==NULL)
        return;

    mitk::Point3D ori=m_Image->GetGeometry()->GetOrigin();
    mitk::Vector3D spacing=m_Image->GetGeometry()->GetSpacing();
    mitk::BaseGeometry::BoundsArrayType bounds=m_Image->GetGeometry()->GetBounds();
    realBounds[0]=ori[0]+bounds[0]*spacing[0];
    realBounds[2]=ori[1]+bounds[2]*spacing[1];
    realBounds[4]=ori[2]+bounds[4]*spacing[2];

    realBounds[1]=ori[0]+bounds[1]*spacing[0];
    realBounds[3]=ori[1]+bounds[3]*spacing[1];
    realBounds[5]=ori[2]+bounds[5]*spacing[2];
}

double svPathEdit::GetVolumeImageSpacing()
{
    mitk::Vector3D spacing=m_Image->GetGeometry()->GetSpacing();
    double minSpacing=std::min(spacing[0],std::min(spacing[1],spacing[2]));
    return minSpacing;

}

void svPathEdit::ChangePath(){

    if(m_Path==NULL) return;

    int timeStep=GetTimeStep(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    svPathCreate* pathCreateWidget=dynamic_cast<svPathCreate*>(GetExtension("sv.pathcreate"));
    if(pathCreateWidget)
    {
        pathCreateWidget->SetCreatePath(false);
        pathCreateWidget->SetPathName(ui->labelPathName->text());
        pathCreateWidget->SetSubdivisionType(pathElement->GetMethod());
        pathCreateWidget->SetNumber(pathElement->GetCalculationNumber());
        pathCreateWidget->show();
    }

}

void svPathEdit::AddPoint(int index,mitk::Point3D point, int timeStep)
{
    if(index!=-2 ){

        //        UnselectAll(timeStep, timeInMs,true);

        mitk::OperationEvent::IncCurrObjectEventId();

        svPathOperation* doOp = new svPathOperation(svPathOperation::OpINSERTCONTROLPOINT,timeStep, point, index);
        svPathOperation *undoOp = new svPathOperation(svPathOperation::OpREMOVECONTROLPOINT,timeStep, point, index);
        mitk::OperationEvent *operationEvent = new mitk::OperationEvent(m_Path, doOp, undoOp, "Insert Control Point");
        mitk::UndoController::GetCurrentUndoModel()->SetOperationEvent( operationEvent );

        m_Path->ExecuteOperation(doOp);

        RequestRenderWindowUpdate();
    }

}

void svPathEdit::SmartAdd()
{
    if(m_Path==NULL){
        QMessageBox::information(NULL,"No Path Selected","Please select a path in data manager!");
        return;
    }

    mitk::Point3D point=GetRenderWindowPart()->GetSelectedPosition();

    int timeStep=GetTimeStep(m_Path);
    mitk::ScalarType timeInMs=GetTime(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    //Check if the point already exists
    if(pathElement->SearchControlPoint(point,0)!=-2)
    {
        return;
    }

    int index=pathElement->GetInsertintIndexByDistance(point);

    AddPoint(index,point,timeStep);

}

void svPathEdit::AddToEnd()
{
    if(m_Path==NULL){
        QMessageBox::information(NULL,"No Path Selected","Please select a path in data manager!");
        return;
    }

    mitk::Point3D point=GetRenderWindowPart()->GetSelectedPosition();

    int timeStep=GetTimeStep(m_Path);
    mitk::ScalarType timeInMs=GetTime(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    //Check if the point already exists
    if(pathElement->SearchControlPoint(point,0)!=-2)
    {
        return;
    }

    //    int index=currentPath->GetSize(timeStep);
    int index=-1;

    AddPoint(index,point,timeStep);
}

void svPathEdit::AddToTop()
{
    if(m_Path==NULL){
        QMessageBox::information(NULL,"No Path Selected","Please select a path in data manager!");
        return;
    }

    mitk::Point3D point=GetRenderWindowPart()->GetSelectedPosition();

    int timeStep=GetTimeStep(m_Path);
    mitk::ScalarType timeInMs=GetTime(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    //Check if the point already exists
    if(pathElement->SearchControlPoint(point,0)!=-2)
    {
        return;
    }

    AddPoint(0,point,timeStep);
}

void svPathEdit::InsertPointAbove()
{

    if(m_Path==NULL){
        QMessageBox::information(NULL,"No Path Selected","Please select a path in data manager!");
        return;
    }

    mitk::Point3D point=GetRenderWindowPart()->GetSelectedPosition();

    int timeStep=GetTimeStep(m_Path);
    mitk::ScalarType timeInMs=GetTime(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    //Check if the point already exists
    if(pathElement->SearchControlPoint(point,0)!=-2)
    {
        return;
    }

    int index= ui->listWidget->selectionModel()->selectedRows().front().row();
    AddPoint(index,point,timeStep);

}

void svPathEdit::DeleteSelected(){
    if(m_Path==NULL){
        QMessageBox::information(NULL,"No Path Selected","Please select a path in data manager!");
        return;
    }

    int timeStep=GetTimeStep(m_Path);
    mitk::ScalarType timeInMs=GetTime(m_Path);

    svPathElement* pathElement=m_Path->GetPathElement(timeStep);
    if(pathElement==NULL) return;

    if(pathElement->GetControlPointNumber()<1){
        return;
    }

    int index= ui->listWidget->selectionModel()->selectedRows().front().row();

    if(index>-1 && index<pathElement->GetControlPointNumber()){
        mitk::Point3D point=pathElement->GetControlPoint(index);
        svPathOperation* doOp = new svPathOperation(svPathOperation::OpREMOVECONTROLPOINT,timeStep, point, index);

        svPathOperation* undoOp = new svPathOperation(svPathOperation::OpINSERTCONTROLPOINT,timeStep, point, index);
        mitk::OperationEvent *operationEvent = new mitk::OperationEvent(m_Path, doOp, undoOp, "Remove Control Point");
        mitk::UndoController::GetCurrentUndoModel()->SetOperationEvent( operationEvent );

        m_Path->ExecuteOperation(doOp);
        RequestRenderWindowUpdate();
    }
}

void svPathEdit::SelectItem(const QModelIndex & idx){

    int index=idx.row();

    if(m_Path)
    {
        int timeStep=GetTimeStep(m_Path);
        svPathElement* pathElement=m_Path->GetPathElement(timeStep);
        if(pathElement==NULL) return;

        if(pathElement->GetControlPointSelectedIndex()==-2 || !pathElement->IsControlPointSelected(index))
        {
            if(pathElement->GetControlPointSelectedIndex()!=-2)
            {
                pathElement->DeselectControlPoint();
            }

//            pathElement->SetControlPointSelected(index,true);
            svPathOperation* doOp = new svPathOperation(svPathOperation::OpSELECTCONTROLPOINT,timeStep, index,true);
            m_Path->ExecuteOperation(doOp);

            if(ui->resliceSlider->isResliceOn())
            {
                ui->resliceSlider->moveToPathPosPoint(pathElement->GetControlPoint(index));
            }
            else
            {
                GetRenderWindowPart()->SetSelectedPosition(pathElement->GetControlPoint(index));
            }
        }
        else
        {
//            pathElement->SetControlPointSelected(index,false);
            svPathOperation* doOp = new svPathOperation(svPathOperation::OpSELECTCONTROLPOINT,timeStep, index,false);
            m_Path->ExecuteOperation(doOp);
        }

        RequestRenderWindowUpdate();
    }

}

void svPathEdit::SmoothCurrentPath()
{
    if(m_SmoothWidget==NULL)
    {
        m_SmoothWidget=new svPathSmooth();
        m_SmoothWidget->CreatePartControl();
    }

    m_SmoothWidget->show();
}



#ifndef SVSEGMENTATION2D_H
#define SVSEGMENTATION2D_H

#include "svAbstractView.h"
#include "svPath.h"
#include "svSegmentationUtils.h"
#include "svContourGroup.h"
#include "svContourModel.h"
#include "svContourGroupDataInteractor.h"

#include "svResliceSlider.h"
#include "svLevelSet2DWidget.h"
#include "svLoftParamWidget.h"

#include <QmitkSliceWidget.h>
#include <QmitkSliderNavigatorWidget.h>
#include <QmitkStepperAdapter.h>

#include <mitkDataStorage.h>
#include <mitkDataNode.h>
#include <mitkSurface.h>
#include <mitkPointSet.h>
#include <mitkDataInteractor.h>
#include <mitkImage.h>

#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkTransform.h>

#include <ctkSliderWidget.h>

#include <QSlider>
#include <QPushButton>
#include <QWidget>


namespace Ui {
class svSegmentation2D;
}

class svSegmentation2D : public svAbstractView
{
    Q_OBJECT

public:

    enum SegmentationMethod {LEVELSET_METHOD, THRESHOLD_METHOD, REGION_GROWING_METHOD};

    static const QString EXTENSION_ID;

    svSegmentation2D();

    virtual ~svSegmentation2D();

public slots:

    void CreateLSContour();

    void CreateThresholdContour();

    void CreateCircle();

    void CreateEllipse();

    void CreateSplinePoly();

    void CreatePolygon();

    void UpdateContourList();

    void InsertContour(svContour* contour, int contourIndex);

    void InsertContourByPathPosPoint(svContour* contour);

    void SetContour(int contourIndex, svContour* newContour);

    void RemoveContour(int contourIndex);

    void DeleteSelected();

    void SelectItem(const QModelIndex & idx);

    void ShowGroupEditPane();

    void ShowGroupEditPaneForGroup();

    bool IsContourGroup(QList<mitk::DataNode::Pointer> nodes);

    void ClearAll();

    void UpdatePreview();

    void FinishPreview();

    void SmoothSelected();

    void CreateContours(SegmentationMethod method);

    void SetSecondaryWidgetsVisible(bool visible);

    std::vector<int> GetBatchList();

    svContour* PostprocessContour(svContour* contour);

    double GetVolumeImageSpacing();

    void LoftContourGroup();

    void ShowLoftWidget();

    void UpdateContourGroupLoftingParam();

    void OKLofting();

    void ApplyLofting();

    void HideLoftWidget();

    void ContourChangingOn();

    void ContourChangingOff();

protected:
  virtual void CreateQtPartControl(QWidget *parent) override;

  virtual void OnSelectionChanged(const QList<mitk::DataNode::Pointer>& nodes ) override;

  virtual void NodeChanged(const mitk::DataNode* node) override;

  virtual void NodeAdded(const mitk::DataNode* node) override;

  virtual void NodeRemoved(const mitk::DataNode* node) override;

  virtual void Activated() override;

  virtual void Deactivated() override;

  QWidget* m_Parent;

  QWidget* m_CurrentParamWidget;

//  svLevelSet2DWidget* m_LSParamWidget;

  svLoftParamWidget* m_LoftWidget;

  std::vector< std::pair< QmitkNodeDescriptor*, QAction* > > mDescriptorActionList;

  mitk::Image* m_Image;

  cvStrPts* m_cvImage;

  Ui::svSegmentation2D *ui;

  svContourGroup* m_ContourGroup;

  svPath* m_Path;

  mitk::DataNode::Pointer m_ContourGroupNode;

  svContourGroupDataInteractor::Pointer m_DataInteractor;

  long m_ContourGroupChangeObserverTag;

  mitk::DataNode::Pointer m_LoftSurfaceNode;

  mitk::Surface::Pointer m_LoftSurface;

  bool groupCreated=false;

  mitk::DataInteractor::Pointer m_PreviewDataNodeInteractor;

  mitk::DataNode::Pointer m_PreviewDataNode;

  long m_PreviewContourModelObserverFinishTag;
  long m_PreviewContourModelObserverUpdateTag;

  svContourModel::Pointer m_PreviewContourModel;

  long m_StartLoftContourGroupObserverTag;
  long m_StartLoftContourGroupObserverTag2;
  long m_StartChangingContourObserverTag;
  long m_EndChangingContourObserverTag;

  bool m_ContourChanging;
};

#endif // SVSEGMENTATION2D_H

#include "svProjectManager.h"

#include "svProjectFolder.h"
#include "svImageFolder.h"
#include "svPathFolder.h"
#include "svSegmentationFolder.h"
#include "svModelFolder.h"
#include "svMeshFolder.h"
#include "svSimulationFolder.h"

#include <mitkNodePredicateDataType.h>
#include <mitkIOUtil.h>
#include <mitkRenderingManager.h>

#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QTextStream>

template <typename TDataFolder>
mitk::DataNode::Pointer svProjectManager::CreateDataFolder(mitk::DataStorage::Pointer dataStorage, QString folderName, mitk::DataNode::Pointer projFolderNode)
{
    mitk::NodePredicateDataType::Pointer isDataFolder = mitk::NodePredicateDataType::New(TDataFolder::GetStaticNameOfClass());

    mitk::DataStorage::SetOfObjects::ConstPointer rs;
    if(projFolderNode.IsNull())
    {
        rs=dataStorage->GetSubset(isDataFolder);
    }else
    {
        rs=dataStorage->GetDerivations (projFolderNode,isDataFolder);
    }

    bool exists=false;
    mitk::DataNode::Pointer dataFolderNode=NULL;
    std::string fdName=folderName.toStdString();

    for(int i=0;i<rs->size();i++)
    {
        if(rs->GetElement(i)->GetName()==fdName)
        {
            exists=true;
            dataFolderNode=rs->GetElement(i);
            break;
        }
    }

    if(!exists)
    {
        dataFolderNode=mitk::DataNode::New();
        dataFolderNode->SetName(fdName);
        dataFolderNode->SetVisibility(true);
        typename TDataFolder::Pointer dataFolder=TDataFolder::New();
        dataFolderNode->SetData(dataFolder);
        if(projFolderNode.IsNull())
        {
            dataStorage->Add(dataFolderNode);
        }else
        {
            dataStorage->Add(dataFolderNode,projFolderNode);
        }

    }

    return dataFolderNode;
}

void svProjectManager::AddProject(mitk::DataStorage::Pointer dataStorage, QString projName, QString projParentDir,bool newProject)
{
    QString projectConfigFileName=".svproj";
    QString imageFolderName="Images";
    QString pathFolderName="Paths";
    QString segFolderName="Segmentations";
    QString modelFolderName="Models";
    QString meshFolerName="Meshes";
    QString simFolderName="Simulations";

    QDir dir(projParentDir);
    if(newProject)
    {
        dir.mkdir(projName);
    }
    dir.cd(projName);

    QString projPath=dir.absolutePath();
    QString projectConfigFilePath=dir.absoluteFilePath(projectConfigFileName);

    QStringList imageFilePathList;
    if(newProject)
    {
        WriteEmptyConfigFile(projectConfigFilePath);
        dir.mkdir(imageFolderName);
        dir.mkdir(pathFolderName);
        dir.mkdir(segFolderName);
        dir.mkdir(modelFolderName);
        dir.mkdir(meshFolerName);
        dir.mkdir(simFolderName);
    }else{

        QFile xmlFile(projectConfigFilePath);
        xmlFile.open(QIODevice::ReadOnly);
        QDomDocument doc("svproj");
        QString *em=NULL;
        doc.setContent(&xmlFile,em);
        xmlFile.close();

        QDomElement projDesc = doc.firstChildElement("projectDescription");
        QDomElement imagesElement=projDesc.firstChildElement("images");
        imageFolderName=imagesElement.attribute("folder_name");
        QDomNodeList imageList=imagesElement.elementsByTagName("image");
        for(int i=0;i<imageList.size();i++)
        {
            QDomNode imageNode=imageList.item(i);
            if(imageNode.isNull()) continue;

            QDomElement imageElement=imageNode.toElement();
            if(imageElement.isNull()) continue;

            imageFilePathList<<imageElement.attribute("path");
        }

        pathFolderName=projDesc.firstChildElement("paths").attribute("folder_name");
        segFolderName=projDesc.firstChildElement("segmentations").attribute("folder_name");
        modelFolderName=projDesc.firstChildElement("models").attribute("folder_name");
        meshFolerName=projDesc.firstChildElement("meshes").attribute("folder_name");
        simFolderName=projDesc.firstChildElement("simulations").attribute("folder_name");

    }

    mitk::DataNode::Pointer projectFolderNode= CreateDataFolder<svProjectFolder>(dataStorage,projName);
    projectFolderNode->AddProperty( "project path",mitk::StringProperty::New(projPath.toStdString().c_str()));

    mitk::DataNode::Pointer imageFolderNode=CreateDataFolder<svImageFolder>(dataStorage, imageFolderName, projectFolderNode);
    mitk::DataNode::Pointer pathFolderNode=CreateDataFolder<svPathFolder>(dataStorage, pathFolderName, projectFolderNode);
    mitk::DataNode::Pointer segFolderNode=CreateDataFolder<svSegmentationFolder>(dataStorage,segFolderName, projectFolderNode);
    mitk::DataNode::Pointer modelFolderNode=CreateDataFolder<svModelFolder>(dataStorage, modelFolderName, projectFolderNode);
    mitk::DataNode::Pointer meshFolderNode=CreateDataFolder<svMeshFolder>(dataStorage, meshFolerName, projectFolderNode);
    mitk::DataNode::Pointer simFolderNode=CreateDataFolder<svSimulationFolder>(dataStorage, simFolderName, projectFolderNode);

    if(!newProject)
    {
        for(int i=0;i<imageFilePathList.size();i++)
        {
            mitk::DataNode::Pointer imageNode=mitk::IOUtil::LoadDataNode(imageFilePathList[i].toStdString());
            dataStorage->Add(imageNode,imageFolderNode);
        }


        pathFolderNode->SetVisibility(false);
        QDir dir1(projPath);
        dir1.cd(pathFolderName);
        QFileInfoList fileInfoList=dir1.entryInfoList(QStringList("*.pth"), QDir::Files, QDir::Name);
        for(int i=0;i<fileInfoList.size();i++)
        {
            mitk::DataNode::Pointer pathNode=mitk::IOUtil::LoadDataNode(fileInfoList[i].absoluteFilePath().toStdString());
            pathNode->SetVisibility(false);
            dataStorage->Add(pathNode,pathFolderNode);
        }

        segFolderNode->SetVisibility(false);
        QDir dirSeg(projPath);
        dirSeg.cd(segFolderName);
        fileInfoList=dirSeg.entryInfoList(QStringList("*.ctgr"), QDir::Files, QDir::Name);
        for(int i=0;i<fileInfoList.size();i++)
        {
            mitk::DataNode::Pointer contourGroupNode=mitk::IOUtil::LoadDataNode(fileInfoList[i].absoluteFilePath().toStdString());
            contourGroupNode->SetVisibility(false);
            dataStorage->Add(contourGroupNode,segFolderNode);
        }

        modelFolderNode->SetVisibility(false);
        QDir dirModel(projPath);
        dirModel.cd(modelFolderName);
        fileInfoList=dirModel.entryInfoList(QStringList("*.mdl"), QDir::Files, QDir::Name);
        for(int i=0;i<fileInfoList.size();i++)
        {
            mitk::DataNode::Pointer modelNode=mitk::IOUtil::LoadDataNode(fileInfoList[i].absoluteFilePath().toStdString());
            modelNode->SetVisibility(false);
            dataStorage->Add(modelNode,modelFolderNode);
        }

    }

    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);

}

void svProjectManager::WriteEmptyConfigFile(QString projConfigFilePath)
{
    QDomDocument doc;
    //    doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");

    QDomNode xmlNode = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    doc.appendChild(xmlNode);

    QDomElement root = doc.createElement("projectDescription");
    root.setAttribute("version", "1.0");
    doc.appendChild(root);

    QDomElement tag = doc.createElement("images");
    tag.setAttribute("folder_name","Images");
    root.appendChild(tag);

    tag = doc.createElement("paths");
    tag.setAttribute("folder_name","Paths");
    root.appendChild(tag);

    tag = doc.createElement("segmentations");
    tag.setAttribute("folder_name","Segmentations");
    root.appendChild(tag);

    tag = doc.createElement("models");
    tag.setAttribute("folder_name","Models");
    root.appendChild(tag);

    tag = doc.createElement("meshes");
    tag.setAttribute("folder_name","Meshes");
    root.appendChild(tag);

    tag = doc.createElement("simulations");
    tag.setAttribute("folder_name","Simulations");
    root.appendChild(tag);

    QString xml = doc.toString(4);

    QFile file( projConfigFilePath );

    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << xml <<endl;
    }


}

// so far, no copy into project
void svProjectManager::AddImage(mitk::DataStorage::Pointer dataStorage, QString imageFilePath, mitk::DataNode::Pointer imageFolderNode)
{
    mitk::DataNode::Pointer imageNode=mitk::IOUtil::LoadDataNode(imageFilePath.toStdString());

    dataStorage->Add(imageNode,imageFolderNode);

    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);


    //add image to config

    mitk::NodePredicateDataType::Pointer isProjFolder = mitk::NodePredicateDataType::New("svProjectFolder");
    mitk::DataStorage::SetOfObjects::ConstPointer rs=dataStorage->GetSources (imageFolderNode,isProjFolder);

    std::string projPath;
    mitk::DataNode::Pointer projectFolderNode=rs->GetElement(0);

    projectFolderNode->GetStringProperty("project path",projPath);

    QDir projDir(QString::fromStdString(projPath));
    QString	configFilePath=projDir.absoluteFilePath(".svproj");

    QFile xmlFile(configFilePath);
    xmlFile.open(QIODevice::ReadOnly);
    QDomDocument doc("svproj");
    QString *em=NULL;
    doc.setContent(&xmlFile,em);
    xmlFile.close();

    QDomElement projDesc = doc.firstChildElement("projectDescription");
    QDomElement imagesElement=projDesc.firstChildElement("images");

    QDomElement imgElement = doc.createElement("image");
    imgElement.setAttribute("in_project","no");
    imgElement.setAttribute("path",imageFilePath);
    imgElement.setAttribute("name","");
    imagesElement.appendChild(imgElement);

    QString xml = doc.toString(4);

    QFile file( configFilePath );

    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << xml <<endl;
    }


}


void svProjectManager::SaveProject(mitk::DataStorage::Pointer dataStorage, mitk::DataNode::Pointer projFolderNode)
{
    std::string projPath;
    projFolderNode->GetStringProperty("project path",projPath);

    //save paths
    mitk::DataStorage::SetOfObjects::ConstPointer rs=dataStorage->GetDerivations(projFolderNode,mitk::NodePredicateDataType::New("svPathFolder"));

    mitk::DataNode::Pointer pathFolderNode=rs->GetElement(0);
    std::string pathFolderName=pathFolderNode->GetName();

    rs=dataStorage->GetDerivations(pathFolderNode,mitk::NodePredicateDataType::New("svPath"));

    QDir dir(QString::fromStdString(projPath));
    dir.cd(QString::fromStdString(pathFolderName));

    for(int i=0;i<rs->size();i++)
    {
        QString	filePath=dir.absoluteFilePath(QString::fromStdString(rs->GetElement(i)->GetName())+".pth");
        mitk::IOUtil::Save(rs->GetElement(i)->GetData(),filePath.toStdString());
    }

    //save contour groups
    rs=dataStorage->GetDerivations(projFolderNode,mitk::NodePredicateDataType::New("svSegmentationFolder"));

    mitk::DataNode::Pointer segFolderNode=rs->GetElement(0);
    std::string segFolderName=segFolderNode->GetName();

    rs=dataStorage->GetDerivations(segFolderNode,mitk::NodePredicateDataType::New("svContourGroup"));

    QDir dirSeg(QString::fromStdString(projPath));
    dirSeg.cd(QString::fromStdString(segFolderName));

    for(int i=0;i<rs->size();i++)
    {
        QString	filePath=dirSeg.absoluteFilePath(QString::fromStdString(rs->GetElement(i)->GetName())+".ctgr");
        mitk::IOUtil::Save(rs->GetElement(i)->GetData(),filePath.toStdString());
    }

    //save models
    rs=dataStorage->GetDerivations(projFolderNode,mitk::NodePredicateDataType::New("svModelFolder"));

    mitk::DataNode::Pointer modelFolderNode=rs->GetElement(0);
    std::string modelFolderName=modelFolderNode->GetName();

    rs=dataStorage->GetDerivations(modelFolderNode,mitk::NodePredicateDataType::New("svModel"));

    QDir dirModel(QString::fromStdString(projPath));
    dirModel.cd(QString::fromStdString(modelFolderName));

    for(int i=0;i<rs->size();i++)
    {
        QString	filePath=dirModel.absoluteFilePath(QString::fromStdString(rs->GetElement(i)->GetName())+".mdl");
        mitk::IOUtil::Save(rs->GetElement(i)->GetData(),filePath.toStdString());
    }
}

void svProjectManager::SaveAllProjects(mitk::DataStorage::Pointer dataStorage)
{
    mitk::DataStorage::SetOfObjects::ConstPointer rs=dataStorage->GetSubset(mitk::NodePredicateDataType::New("svProjectFolder"));
    for(int i=0;i<rs->size();i++)
    {
        SaveProject(dataStorage, rs->GetElement(i));
    }
}

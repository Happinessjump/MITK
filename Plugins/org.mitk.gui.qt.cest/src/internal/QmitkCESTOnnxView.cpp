/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "QmitkCESTOnnxView.h"
#include <ui_QmitkCESTOnnxViewControls.h>

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include <QmitkDataStorageComboBoxWithSelectNone.h>

#include <mitkCESTOnnxConfig.h>
#include <mitkCESTOnnxEvaluator.h>
#include <mitkException.h>
#include <mitkImage.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateDimension.h>

const std::string QmitkCESTOnnxView::VIEW_ID = "org.mitk.views.cest.onnx";

QmitkCESTOnnxView::QmitkCESTOnnxView() : m_Controls(std::make_unique<Ui::QmitkCESTOnnxViewControls>())
{
}

QmitkCESTOnnxView::~QmitkCESTOnnxView() = default;

void QmitkCESTOnnxView::SetFocus()
{
  m_Controls->btnRun->setFocus();
}

void QmitkCESTOnnxView::CreateQtPartControl(QWidget* parent)
{
  m_Controls->setupUi(parent);

  m_Controls->comboCESTImage->SetPredicate(
    mitk::NodePredicateAnd::New(mitk::NodePredicateDataType::New("Image"), mitk::NodePredicateDimension::New(4)));
  m_Controls->comboCESTImage->SetDataStorage(this->GetDataStorage());

  connect(m_Controls->btnBrowseModel, &QToolButton::clicked, this, &QmitkCESTOnnxView::OnBrowseModel);
  connect(m_Controls->btnBrowseIni, &QToolButton::clicked, this, &QmitkCESTOnnxView::OnBrowseIni);
  connect(m_Controls->btnRun, &QPushButton::clicked, this, &QmitkCESTOnnxView::OnRun);
  connect(m_Controls->lineModel, &QLineEdit::textChanged, this, &QmitkCESTOnnxView::UpdateGUIControls);
  connect(m_Controls->lineIni, &QLineEdit::textChanged, this, &QmitkCESTOnnxView::UpdateGUIControls);
  connect(m_Controls->comboCESTImage,
          SIGNAL(OnSelectionChanged(const mitk::DataNode*)),
          this,
          SLOT(UpdateGUIControls()));

  this->UpdateGUIControls();
}

void QmitkCESTOnnxView::UpdateGUIControls()
{
  m_Controls->btnRun->setEnabled(m_Controls->comboCESTImage->GetSelectedNode().IsNotNull() &&
                                 !m_Controls->lineModel->text().isEmpty() && !m_Controls->lineIni->text().isEmpty());
}

void QmitkCESTOnnxView::OnBrowseModel()
{
  const auto path = QFileDialog::getOpenFileName(
    nullptr, "Select ONNX model", m_Controls->lineModel->text(), "ONNX model (*.onnx);;All files (*)");
  if (!path.isEmpty())
    m_Controls->lineModel->setText(path);
}

void QmitkCESTOnnxView::OnBrowseIni()
{
  const auto path = QFileDialog::getOpenFileName(
    nullptr, "Select settings file", m_Controls->lineIni->text(), "Settings (*.ini);;All files (*)");
  if (!path.isEmpty())
    m_Controls->lineIni->setText(path);
}

void QmitkCESTOnnxView::OnRun()
{
  const auto node = m_Controls->comboCESTImage->GetSelectedNode();
  if (node.IsNull())
    return;

  const auto* image = dynamic_cast<mitk::Image*>(node->GetData());
  if (image == nullptr)
    return;

  QProgressDialog progressDialog("Calculating parameter maps...", "Cancel", 0, 100, nullptr);
  progressDialog.setWindowModality(Qt::WindowModal);
  progressDialog.setMinimumDuration(0);
  progressDialog.setValue(0);

  try
  {
    const auto config = mitk::CESTOnnxConfig::Load(m_Controls->lineIni->text().toStdString());
    const mitk::CESTOnnxEvaluator evaluator(m_Controls->lineModel->text().toStdString());

    const auto maps = evaluator.Evaluate(
      image,
      config,
      m_Controls->checkDicomRange->isChecked(),
      [&progressDialog](double fraction)
      {
        progressDialog.setValue(static_cast<int>(fraction * 100.0));
        QApplication::processEvents();
        return !progressDialog.wasCanceled();
      });

    progressDialog.close();

    for (const auto& map : maps)
    {
      auto mapNode = mitk::DataNode::New();
      mapNode->SetData(map.Image);
      mapNode->SetName(node->GetName() + "_" + map.Name);
      this->GetDataStorage()->Add(mapNode, node);
    }
  }
  catch (const mitk::Exception& e)
  {
    progressDialog.close();
    QMessageBox::warning(nullptr, "CEST ONNX", QString::fromStdString(e.GetDescription()));
  }
}

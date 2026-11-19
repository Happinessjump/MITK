/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef QmitkCESTOnnxView_h
#define QmitkCESTOnnxView_h

#include <QmitkAbstractView.h>

#include <memory>

namespace Ui
{
  class QmitkCESTOnnxViewControls;
}

/**
 * \brief Calculates CEST parameter maps from a 4D image with an ONNX network.
 */
class QmitkCESTOnnxView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const std::string VIEW_ID;

  QmitkCESTOnnxView();
  ~QmitkCESTOnnxView() override;

protected slots:

  void OnBrowseModel();
  void OnBrowseIni();
  void OnRun();
  void UpdateGUIControls();

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;

  std::unique_ptr<Ui::QmitkCESTOnnxViewControls> m_Controls;
};

#endif

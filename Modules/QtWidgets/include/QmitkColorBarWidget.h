/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef QmitkColorBarWidget_h
#define QmitkColorBarWidget_h

#include <MitkQtWidgetsExports.h>

#include <itkCommand.h>
#include <mitkDataNode.h>
#include <mitkLevelWindowManager.h>
#include <mitkWeakPointer.h>

#include <QWidget>

/** \brief Vertical color bar showing the colormap and value range of the current image.
 *
 * The widget follows the image selected by a LevelWindowManager. The bar
 * spans the current level/window range of that image and is colored by its
 * "LookupTable" property. It is visible only while the image node has the
 * boolean property PropertyName set to true.
 *
 * \ingroup QmitkModule
 */
class MITKQTWIDGETS_EXPORT QmitkColorBarWidget : public QWidget
{
  Q_OBJECT

public:
  /** \brief Boolean node property that switches the color bar on for an image. */
  static const char* const PropertyName;

  explicit QmitkColorBarWidget(QWidget* parent = nullptr);
  ~QmitkColorBarWidget() override;

  /** \brief Set the manager that determines the image and level/window range to display. */
  void SetLevelWindowManager(mitk::LevelWindowManager* levelWindowManager);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  void OnManagerModified(const itk::EventObject&);
  void OnNodeModified(const itk::EventObject&);
  void Refresh();
  void ObserveNode(mitk::DataNode* node);

  mitk::LevelWindowManager::Pointer m_Manager;
  unsigned long m_ManagerObserverTag;
  mitk::WeakPointer<mitk::DataNode> m_Node;
  unsigned long m_NodeObserverTag;
};

#endif

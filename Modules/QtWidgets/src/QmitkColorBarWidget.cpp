/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "QmitkColorBarWidget.h"

#include <mitkBaseProperty.h>
#include <mitkImage.h>
#include <mitkLookupTable.h>
#include <mitkLookupTableProperty.h>

#include <vtkLookupTable.h>

#include <QLinearGradient>
#include <QPainter>

#include <algorithm>

const char* const QmitkColorBarWidget::PropertyName = "Colorbar.Show";

namespace
{
  constexpr int barWidth = 16;
  constexpr int margin = 8;
  constexpr int tickLength = 4;
  constexpr int labelWidth = 52;
  constexpr int tickCount = 5;
}

QmitkColorBarWidget::QmitkColorBarWidget(QWidget* parent)
  : QWidget(parent),
    m_ManagerObserverTag(0),
    m_NodeObserverTag(0)
{
  this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  this->hide();
}

QmitkColorBarWidget::~QmitkColorBarWidget()
{
  this->ObserveNode(nullptr);

  if (m_Manager.IsNotNull())
    m_Manager->RemoveObserver(m_ManagerObserverTag);
}

void QmitkColorBarWidget::SetLevelWindowManager(mitk::LevelWindowManager* levelWindowManager)
{
  if (m_Manager.IsNotNull())
    m_Manager->RemoveObserver(m_ManagerObserverTag);

  m_Manager = levelWindowManager;

  if (m_Manager.IsNotNull())
  {
    auto command = itk::ReceptorMemberCommand<QmitkColorBarWidget>::New();
    command->SetCallbackFunction(this, &QmitkColorBarWidget::OnManagerModified);
    m_ManagerObserverTag = m_Manager->AddObserver(itk::ModifiedEvent(), command);
  }

  this->Refresh();
}

QSize QmitkColorBarWidget::sizeHint() const
{
  return QSize(margin + barWidth + tickLength + labelWidth, 200);
}

void QmitkColorBarWidget::OnManagerModified(const itk::EventObject&)
{
  this->Refresh();
}

void QmitkColorBarWidget::OnNodeModified(const itk::EventObject&)
{
  this->Refresh();
}

void QmitkColorBarWidget::ObserveNode(mitk::DataNode* node)
{
  auto oldNode = m_Node.Lock();
  if (oldNode.GetPointer() == node)
    return;

  if (oldNode.IsNotNull())
    oldNode->RemoveObserver(m_NodeObserverTag);

  m_Node = node;
  m_NodeObserverTag = 0;

  if (nullptr != node)
  {
    auto command = itk::ReceptorMemberCommand<QmitkColorBarWidget>::New();
    command->SetCallbackFunction(this, &QmitkColorBarWidget::OnNodeModified);
    m_NodeObserverTag = node->AddObserver(itk::ModifiedEvent(), command);
  }
}

void QmitkColorBarWidget::Refresh()
{
  mitk::DataNode::Pointer currentNode;

  if (m_Manager.IsNotNull() && nullptr != m_Manager->GetCurrentImage())
  {
    const auto nodes = m_Manager->GetRelevantNodes();
    if (nodes.IsNotNull())
    {
      for (const auto& node : *nodes)
      {
        if (node->GetData() == m_Manager->GetCurrentImage())
        {
          currentNode = node;
          break;
        }
      }
    }
  }

  this->ObserveNode(currentNode);

  bool show = false;
  if (currentNode.IsNotNull())
    currentNode->GetBoolProperty(PropertyName, show);

  this->setVisible(show);
  this->update();
}

void QmitkColorBarWidget::paintEvent(QPaintEvent*)
{
  const auto node = m_Node.Lock();
  if (node.IsNull() || m_Manager.IsNull())
    return;

  const QRect bar(margin, margin, barWidth, this->height() - 2 * margin);
  if (bar.height() <= 0)
    return;

  QLinearGradient gradient(0, bar.bottom(), 0, bar.top());

  const auto* lookupTableProperty = dynamic_cast<mitk::LookupTableProperty*>(node->GetProperty("LookupTable"));
  vtkLookupTable* lookupTable = nullptr != lookupTableProperty && lookupTableProperty->GetValue().IsNotNull()
                                        ? lookupTableProperty->GetValue()->GetVtkLookupTable()
                                        : nullptr;

  if (nullptr != lookupTable && lookupTable->GetNumberOfTableValues() > 1)
  {
    const auto count = lookupTable->GetNumberOfTableValues();
    for (vtkIdType i = 0; i < count; ++i)
    {
      double rgba[4];
      lookupTable->GetTableValue(i, rgba);
      gradient.setColorAt(static_cast<double>(i) / static_cast<double>(count - 1),
                          QColor::fromRgbF(rgba[0], rgba[1], rgba[2]));
    }
  }
  else
  {
    gradient.setColorAt(0.0, Qt::black);
    gradient.setColorAt(1.0, Qt::white);
  }

  QPainter painter(this);
  painter.fillRect(bar, gradient);
  painter.setPen(this->palette().color(QPalette::WindowText));
  painter.drawRect(bar.adjusted(0, 0, -1, -1));

  const auto& levelWindow = m_Manager->GetLevelWindow();
  const double lower = levelWindow.GetLowerWindowBound();
  const double upper = levelWindow.GetUpperWindowBound();

  const int textHeight = painter.fontMetrics().height();
  for (int i = 0; i < tickCount; ++i)
  {
    const double fraction = static_cast<double>(i) / (tickCount - 1);
    const int y = bar.bottom() - static_cast<int>(fraction * (bar.height() - 1));
    const int xTick = bar.right() + 1;

    painter.drawLine(xTick, y, xTick + tickLength, y);

    const QRect textRect(xTick + tickLength + 2, y - textHeight / 2, labelWidth, textHeight);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(lower + fraction * (upper - lower), 'g', 4));
  }
}

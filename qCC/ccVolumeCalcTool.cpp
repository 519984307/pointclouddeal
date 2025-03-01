﻿//##########################################################################
//#                                                                        #
//#                              CLOUDCOMPARE                              #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "ccVolumeCalcTool.h"

//Local
#include "ccBoundingBoxEditorDlg.h"
#include "ccPersistentSettings.h"
#include "ccCommon.h"
#include "mainwindow.h"
#include "ccIsolines.h"

//qCC_db
#include <ccGenericPointCloud.h>
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>
#include <ccMesh.h>

//qCC_gl
#include <ccGLWindow.h>

//CCLib
#include <Delaunay2dMesh.h>
#include <PointProjectionTools.h>

//Qt
#include <QSettings>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QClipboard>
#include <QApplication>
#include <QLocale>

//System
#include <assert.h>

ccVolumeCalcTool::ccVolumeCalcTool(ccGenericPointCloud* cloud1, ccGenericPointCloud* cloud2, QWidget* parent/*=0*/)
	: QDialog(parent, Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint)
	, cc2Point5DimEditor()
	, Ui::VolumeCalcDialog()
	, m_cloud1(cloud1)
	, m_cloud2(cloud2)
{
	setupUi(this);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &ccVolumeCalcTool::saveSettingsAndAccept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &ccVolumeCalcTool::reject);
	connect(gridStepDoubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &ccVolumeCalcTool::updateGridInfo);
	connect(gridStepDoubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &ccVolumeCalcTool::gridOptionChanged);
	connect(groundEmptyValueDoubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &ccVolumeCalcTool::gridOptionChanged);
	connect(ceilEmptyValueDoubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &ccVolumeCalcTool::gridOptionChanged);
	connect(projDimComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::projectionDirChanged);
	connect(updatePushButton, &QPushButton::clicked, this, &ccVolumeCalcTool::updateGridAndDisplay);
	connect(heightProjectionComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::gridOptionChanged);
	connect(fillGroundEmptyCellsComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::groundFillEmptyCellStrategyChanged);
	connect(fillCeilEmptyCellsComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::ceilFillEmptyCellStrategyChanged);
	connect(swapToolButton, &QToolButton::clicked, this, &ccVolumeCalcTool::swapRoles);
	connect(groundComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::groundSourceChanged);
	connect(ceilComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ccVolumeCalcTool::ceilSourceChanged);
	connect(clipboardPushButton, &QPushButton::clicked, this, &ccVolumeCalcTool::exportToClipboard);
	connect(exportGridPushButton, &QPushButton::clicked, this, &ccVolumeCalcTool::exportGridAsCloud);
	connect(precisionSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ccVolumeCalcTool::setDisplayedNumberPrecision);

	if (m_cloud1 && !m_cloud2)
	{
		//the existing cloud is always the second by default
		std::swap(m_cloud1, m_cloud2);
	}
	assert(m_cloud2);

	//custom bbox editor
	ccBBox gridBBox = m_cloud1 ? m_cloud1->getOwnBB() : ccBBox();
	if (m_cloud2)
	{
		gridBBox += m_cloud2->getOwnBB();
	}
	if (gridBBox.isValid())
	{
		createBoundingBoxEditor(gridBBox, this);
		connect(editGridToolButton, &QToolButton::clicked, this, &ccVolumeCalcTool::showGridBoxEditor);
	}
	else
	{
		editGridToolButton->setEnabled(false);
	}

	groundComboBox->addItem("常数");
	ceilComboBox->addItem("常数");
	if (m_cloud1)
	{
		groundComboBox->addItem(m_cloud1->getName());
		ceilComboBox->addItem(m_cloud1->getName());
	}
	if (m_cloud2)
	{
		groundComboBox->addItem(m_cloud2->getName());
		ceilComboBox->addItem(m_cloud2->getName());
	}
	assert(groundComboBox->count() >= 2);
	groundComboBox->setCurrentIndex(groundComboBox->count() - 2);
	ceilComboBox->setCurrentIndex(ceilComboBox->count() - 1);

	//add window
	create2DView(mapFrame);
	if (m_glWindow)
	{
		ccGui::ParamStruct params = m_glWindow->getDisplayParameters();
		params.colorScaleShowHistogram = false;
		params.displayedNumPrecision = precisionSpinBox->value();
		m_glWindow->setDisplayParameters(params, true);
	}

	loadSettings();

	updateGridInfo();

	gridIsUpToDate(false);
}

void ccVolumeCalcTool::setDisplayedNumberPrecision(int precision)
{
	//update window
	if (m_glWindow)
	{
		ccGui::ParamStruct params = m_glWindow->getDisplayParameters();
		params.displayedNumPrecision = precision;
		m_glWindow->setDisplayParameters(params, true);
		m_glWindow->redraw(true, false);
	}

	//update report
	if (clipboardPushButton->isEnabled())
	{
		outputReport(m_lastReport);
	}
}

void ccVolumeCalcTool::groundSourceChanged(int)
{
	fillGroundEmptyCellsComboBox->setEnabled(groundComboBox->currentIndex() > 0);
	groundFillEmptyCellStrategyChanged(-1);
}

void ccVolumeCalcTool::ceilSourceChanged(int)
{
	fillCeilEmptyCellsComboBox->setEnabled(ceilComboBox->currentIndex() > 0);
	ceilFillEmptyCellStrategyChanged(-1);
}

void ccVolumeCalcTool::swapRoles()
{
	int sourceIndex = ceilComboBox->currentIndex();
	int emptyCellStrat = fillCeilEmptyCellsComboBox->currentIndex();
	double emptyCellValue = ceilEmptyValueDoubleSpinBox->value();

	ceilComboBox->setCurrentIndex(groundComboBox->currentIndex());
	fillCeilEmptyCellsComboBox->setCurrentIndex(fillGroundEmptyCellsComboBox->currentIndex());
	ceilEmptyValueDoubleSpinBox->setValue(groundEmptyValueDoubleSpinBox->value());

	groundComboBox->setCurrentIndex(sourceIndex);
	fillGroundEmptyCellsComboBox->setCurrentIndex(emptyCellStrat);
	groundEmptyValueDoubleSpinBox->setValue(emptyCellValue);

	gridIsUpToDate(false);
}

bool ccVolumeCalcTool::showGridBoxEditor()
{
	if (cc2Point5DimEditor::showGridBoxEditor())
	{
		updateGridInfo();
		return true;
	}

	return false;
}

void ccVolumeCalcTool::updateGridInfo()
{
	gridWidthLabel->setText(getGridSizeAsString());
}

double ccVolumeCalcTool::getGridStep() const
{
	return gridStepDoubleSpinBox->value();
}

unsigned char ccVolumeCalcTool::getProjectionDimension() const
{
	int dim = projDimComboBox->currentIndex();
	assert(dim >= 0 && dim < 3);

	return static_cast<unsigned char>(dim);
}

void ccVolumeCalcTool::sfProjectionTypeChanged(int index)
{
	gridIsUpToDate(false);
}

void ccVolumeCalcTool::projectionDirChanged(int dir)
{
	updateGridInfo();
	gridIsUpToDate(false);
}

void ccVolumeCalcTool::groundFillEmptyCellStrategyChanged(int)
{
	ccRasterGrid::EmptyCellFillOption fillEmptyCellsStrategy = getFillEmptyCellsStrategy(fillGroundEmptyCellsComboBox);

	groundEmptyValueDoubleSpinBox->setEnabled(groundComboBox->currentIndex() == 0
		|| fillEmptyCellsStrategy == ccRasterGrid::FILL_CUSTOM_HEIGHT);
	gridIsUpToDate(false);
}

void ccVolumeCalcTool::ceilFillEmptyCellStrategyChanged(int)
{
	ccRasterGrid::EmptyCellFillOption fillEmptyCellsStrategy = getFillEmptyCellsStrategy(fillCeilEmptyCellsComboBox);

	ceilEmptyValueDoubleSpinBox->setEnabled(ceilComboBox->currentIndex() == 0
		|| fillEmptyCellsStrategy == ccRasterGrid::FILL_CUSTOM_HEIGHT);
	gridIsUpToDate(false);
}

void ccVolumeCalcTool::gridOptionChanged()
{
	gridIsUpToDate(false);
}

ccRasterGrid::ProjectionType ccVolumeCalcTool::getTypeOfProjection() const
{
	switch (heightProjectionComboBox->currentIndex())
	{
	case 0:
		return ccRasterGrid::PROJ_MINIMUM_VALUE;
	case 1:
		return ccRasterGrid::PROJ_AVERAGE_VALUE;
	case 2:
		return ccRasterGrid::PROJ_MAXIMUM_VALUE;
	default:
		//shouldn't be possible for this option!
		assert(false);
	}

	return ccRasterGrid::INVALID_PROJECTION_TYPE;
}

void ccVolumeCalcTool::loadSettings()
{
	QSettings settings;
	settings.beginGroup(ccPS::VolumeCalculation());
	int projType = settings.value("ProjectionType", heightProjectionComboBox->currentIndex()).toInt();
	int projDim = settings.value("ProjectionDim", projDimComboBox->currentIndex()).toInt();
	int groundFillStrategy = settings.value("gFillStrategy", fillGroundEmptyCellsComboBox->currentIndex()).toInt();
	int ceilFillStrategy = settings.value("cFillStrategy", fillCeilEmptyCellsComboBox->currentIndex()).toInt();
	double step = settings.value("GridStep", gridStepDoubleSpinBox->value()).toDouble();
	double groundEmptyHeight = settings.value("gEmptyCellsHeight", groundEmptyValueDoubleSpinBox->value()).toDouble();
	double ceilEmptyHeight = settings.value("cEmptyCellsHeight", ceilEmptyValueDoubleSpinBox->value()).toDouble();
	int precision = settings.value("NumPrecision", precisionSpinBox->value()).toInt();
	settings.endGroup();

	gridStepDoubleSpinBox->setValue(step);
	heightProjectionComboBox->setCurrentIndex(projType);
	fillGroundEmptyCellsComboBox->setCurrentIndex(groundFillStrategy);
	fillCeilEmptyCellsComboBox->setCurrentIndex(ceilFillStrategy);
	groundEmptyValueDoubleSpinBox->setValue(groundEmptyHeight);
	ceilEmptyValueDoubleSpinBox->setValue(ceilEmptyHeight);
	projDimComboBox->setCurrentIndex(projDim);
	precisionSpinBox->setValue(precision);
}

void ccVolumeCalcTool::saveSettingsAndAccept()
{
	saveSettings();
	accept();
}

void ccVolumeCalcTool::saveSettings()
{
	QSettings settings;
	settings.beginGroup(ccPS::VolumeCalculation());
	settings.setValue("ProjectionType", heightProjectionComboBox->currentIndex());
	settings.setValue("ProjectionDim", projDimComboBox->currentIndex());
	settings.setValue("gFillStrategy", fillGroundEmptyCellsComboBox->currentIndex());
	settings.setValue("cFillStrategy", fillCeilEmptyCellsComboBox->currentIndex());
	settings.setValue("GridStep", gridStepDoubleSpinBox->value());
	settings.setValue("gEmptyCellsHeight", groundEmptyValueDoubleSpinBox->value());
	settings.setValue("cEmptyCellsHeight", ceilEmptyValueDoubleSpinBox->value());
	settings.setValue("NumPrecision", precisionSpinBox->value());
	settings.endGroup();
}

void ccVolumeCalcTool::gridIsUpToDate(bool state)
{
	if (state)
	{
		//standard button
		updatePushButton->setStyleSheet(QString());
	}
	else
	{
		//red button
		updatePushButton->setStyleSheet("颜色: 白; 背景色:红;");
	}
	updatePushButton->setDisabled(state);
	clipboardPushButton->setEnabled(state);
	exportGridPushButton->setEnabled(state);
	if (!state)
	{
		spareseWarningLabel->hide();
		reportPlainTextEdit->setPlainText("请先更新单元格");
	}
}

ccPointCloud* ccVolumeCalcTool::ConvertGridToCloud(ccRasterGrid& grid,
	const ccBBox& gridBox,
	unsigned char vertDim,
	bool exportToOriginalCS)
{
	assert(gridBox.isValid());
	assert(vertDim < 3);

	ccPointCloud* rasterCloud = nullptr;
	try
	{
		//we only compute the default 'height' layer
		std::vector<ccRasterGrid::ExportableFields> exportedFields;
		exportedFields.push_back(ccRasterGrid::PER_CELL_HEIGHT);

		rasterCloud = grid.convertToCloud(exportedFields,
			false,
			false,
			false,
			false,
			nullptr,
			vertDim,
			gridBox,
			false,
			std::numeric_limits<double>::quiet_NaN(),
			exportToOriginalCS);

		if (rasterCloud && rasterCloud->hasScalarFields())
		{
			rasterCloud->showSF(true);
			rasterCloud->setCurrentDisplayedScalarField(0);
			ccScalarField* sf = static_cast<ccScalarField*>(rasterCloud->getScalarField(0));
			assert(sf);
			sf->setName("相对高度");
			sf->setSymmetricalScale(sf->getMin() < 0 && sf->getMax() > 0);
			rasterCloud->showSFColorsScale(true);
		}
	}
	catch (const std::bad_alloc&)
	{
		ccLog::Warning("[转换单元格为点云] 内存不足!");
		if (rasterCloud)
		{
			delete rasterCloud;
			rasterCloud = nullptr;
		}
	}

	return rasterCloud;
}

ccPointCloud* ccVolumeCalcTool::convertGridToCloud(bool exportToOriginalCS) const
{
	ccPointCloud* rasterCloud = nullptr;
	try
	{
		//we only compute the default 'height' layer
		std::vector<ccRasterGrid::ExportableFields> exportedFields;
		exportedFields.push_back(ccRasterGrid::PER_CELL_HEIGHT);
		rasterCloud = cc2Point5DimEditor::convertGridToCloud(exportedFields,
			false,
			false,
			false,
			false,
			nullptr,
			false,
			std::numeric_limits<double>::quiet_NaN(),
			exportToOriginalCS);

		if (rasterCloud && rasterCloud->hasScalarFields())
		{
			rasterCloud->showSF(true);
			rasterCloud->setCurrentDisplayedScalarField(0);
			ccScalarField* sf = static_cast<ccScalarField*>(rasterCloud->getScalarField(0));
			assert(sf);
			sf->setName("相对高度");
			sf->setSymmetricalScale(sf->getMin() < 0 && sf->getMax() > 0);
			rasterCloud->showSFColorsScale(true);
		}
	}
	catch (const std::bad_alloc&)
	{
		ccLog::Error("内存不足!");
		if (rasterCloud)
		{
			delete rasterCloud;
			rasterCloud = nullptr;
		}
	}

	return rasterCloud;
}

void ccVolumeCalcTool::updateGridAndDisplay()
{
	bool success = updateGrid();
	if (success && m_glWindow)
	{
		//convert grid to point cloud
		if (m_rasterCloud)
		{
			m_glWindow->removeFromOwnDB(m_rasterCloud);
			delete m_rasterCloud;
			m_rasterCloud = nullptr;
		}

		m_rasterCloud = convertGridToCloud(false);
		if (m_rasterCloud)
		{
			m_glWindow->addToOwnDB(m_rasterCloud);
			ccBBox box = m_rasterCloud->getDisplayBB_recursive(false, m_glWindow);
			update2DDisplayZoom(box);
		}
		else
		{
			ccLog::Error("内存不足!");
			m_glWindow->redraw();
		}
	}

	gridIsUpToDate(success);
}

QString ccVolumeCalcTool::ReportInfo::toText(int precision) const
{
	QLocale locale(QLocale::English);

	QStringList reportText;
	reportText << QString("体积: %1").arg(locale.toString(volume, 'f', precision));
	reportText << QString("表面积: %1").arg(locale.toString(surface, 'f', precision));
	reportText << QString("----------------------");
	reportText << QString("计入体积数: (+)%1").arg(locale.toString(addedVolume, 'f', precision));
	reportText << QString("舍去体积数: (-)%1").arg(locale.toString(removedVolume, 'f', precision));
	reportText << QString("----------------------");
	reportText << QString("匹配单元格: %1%").arg(matchingPrecent, 0, 'f', 1);
	reportText << QString("未匹配单元格:");
	reportText << QString("    底 = %1%").arg(groundNonMatchingPercent, 0, 'f', 1);
	reportText << QString("    顶 = %1%").arg(ceilNonMatchingPercent, 0, 'f', 1);
	reportText << QString("每个单元的平均邻居数: %1 / 8.0").arg(averageNeighborsPerCell, 0, 'f', 1);

	return reportText.join("\n");
}

void ccVolumeCalcTool::outputReport(const ReportInfo& info)
{
	int precision = precisionSpinBox->value();

	reportPlainTextEdit->setPlainText(info.toText(precision));

	//below 7 neighbors per cell, at least one of the cloud is very sparse!
	spareseWarningLabel->setVisible(info.averageNeighborsPerCell < 7.0f);

	m_lastReport = info;
	clipboardPushButton->setEnabled(true);
}

bool SendError(const QString& message, QWidget* parentWidget)
{
	if (parentWidget)
	{
		ccLog::Error(message);
	}
	else
	{
		ccLog::Warning("[Volume] " + message);
	}
	return false;
}

bool ccVolumeCalcTool::ComputeVolume(ccRasterGrid& grid,
	ccGenericPointCloud* ground,
	ccGenericPointCloud* ceil,
	const ccBBox& gridBox,
	unsigned char vertDim,
	double gridStep,
	unsigned gridWidth,
	unsigned gridHeight,
	ccRasterGrid::ProjectionType projectionType,
	ccRasterGrid::EmptyCellFillOption groundEmptyCellFillStrategy,
	ccRasterGrid::EmptyCellFillOption ceilEmptyCellFillStrategy,
	ccVolumeCalcTool::ReportInfo& reportInfo,
	double groundHeight = std::numeric_limits<double>::quiet_NaN(),
	double ceilHeight = std::numeric_limits<double>::quiet_NaN(),
	QWidget* parentWidget/*=0*/)
{
	if (gridStep <= 1.0e-8
		|| gridWidth == 0
		|| gridHeight == 0
		|| vertDim > 2)
	{
		assert(false);
		ccLog::Warning("[体积] 无效输入参数");
		return false;
	}

	if (!ground && !ceil)
	{
		assert(false);
		ccLog::Warning("[体积] 无效输入点云");
		return false;
	}

	if (!gridBox.isValid())
	{
		ccLog::Warning("[体积] 无效边框");
		return false;
	}

	//grid size
	unsigned gridTotalSize = gridWidth * gridHeight;
	if (gridTotalSize == 1)
	{
		if (parentWidget && QMessageBox::question(parentWidget, "不理想的单元格大小", "生成的网格将只有1个单元格！你想继续吗？", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
			return false;
	}
	else if (gridTotalSize > 10000000)
	{
		if (parentWidget && QMessageBox::question(parentWidget, "单元格太大", "生成的网格将有超过10000个单元格！你想继续吗？", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
			return false;
	}

	//memory allocation
	CCVector3d minCorner = CCVector3d::fromArray(gridBox.minCorner().u);
	if (!grid.init(gridWidth, gridHeight, gridStep, minCorner))
	{
		//not enough memory
		return SendError("内存不足", parentWidget);
	}

	//progress dialog
	QScopedPointer<ccProgressDialog> pDlg(nullptr);
	if (parentWidget)
	{
		pDlg.reset(new ccProgressDialog(true, parentWidget));
	}

	ccRasterGrid groundRaster;
	if (ground)
	{
		if (!groundRaster.init(gridWidth, gridHeight, gridStep, minCorner))
		{
			//not enough memory
			return SendError("内存不足", parentWidget);
		}

		if (groundRaster.fillWith(ground,
			vertDim,
			projectionType,
			groundEmptyCellFillStrategy == ccRasterGrid::INTERPOLATE,
			ccRasterGrid::INVALID_PROJECTION_TYPE,
			pDlg.data()))
		{
			groundRaster.fillEmptyCells(groundEmptyCellFillStrategy, groundHeight);
			ccLog::Print(QString("[体积] 底部栅格:大小: %1 x %2 / 高度: [%3 ; %4]").arg(groundRaster.width).arg(groundRaster.height).arg(groundRaster.minHeight).arg(groundRaster.maxHeight));
		}
		else
		{
			return false;
		}
	}

	//ceil
	ccRasterGrid ceilRaster;
	if (ceil)
	{
		if (!ceilRaster.init(gridWidth, gridHeight, gridStep, minCorner))
		{
			//not enough memory
			return SendError("Not enough memory", parentWidget);
		}

		if (ceilRaster.fillWith(ceil,
			vertDim,
			projectionType,
			ceilEmptyCellFillStrategy == ccRasterGrid::INTERPOLATE,
			ccRasterGrid::INVALID_PROJECTION_TYPE,
			pDlg.data()))
		{
			ceilRaster.fillEmptyCells(ceilEmptyCellFillStrategy, ceilHeight);
			ccLog::Print(QString("[体积] 顶部栅格: 大小: %1 x %2 / 高度s: [%3 ; %4]").arg(ceilRaster.width).arg(ceilRaster.height).arg(ceilRaster.minHeight).arg(ceilRaster.maxHeight));
		}
		else
		{
			return false;
		}
	}

	//update grid and compute volume
	{
		if (pDlg)
		{
			pDlg->setMethodTitle(QObject::tr("体积计算"));
			pDlg->setInfo(QObject::tr("单元格数: %1 x %2").arg(grid.width).arg(grid.height));
			pDlg->start();
			pDlg->show();
			QCoreApplication::processEvents();
		}
		CCLib::NormalizedProgress nProgress(pDlg.data(), grid.width * grid.height);

		size_t ceilNonMatchingCount = 0;
		size_t groundNonMatchingCount = 0;
		size_t cellCount = 0;

		//at least one of the grid is based on a cloud
		grid.nonEmptyCellCount = 0;
		for (unsigned i = 0; i < grid.height; ++i)
		{
			for (unsigned j = 0; j < grid.width; ++j)
			{
				ccRasterCell& cell = grid.rows[i][j];

				bool validGround = true;
				cell.minHeight = groundHeight;
				if (ground)
				{
					cell.minHeight = groundRaster.rows[i][j].h;
					validGround = std::isfinite(cell.minHeight);
				}

				bool validCeil = true;
				cell.maxHeight = ceilHeight;
				if (ceil)
				{
					cell.maxHeight = ceilRaster.rows[i][j].h;
					validCeil = std::isfinite(cell.maxHeight);
				}

				if (validGround && validCeil)
				{
					cell.h = cell.maxHeight - cell.minHeight;
					cell.nbPoints = 1;

					reportInfo.volume += cell.h;
					if (cell.h < 0)
					{
						reportInfo.removedVolume -= cell.h;
					}
					else if (cell.h > 0)
					{
						reportInfo.addedVolume += cell.h;
					}
					reportInfo.surface += 1.0;
					++grid.nonEmptyCellCount; // matching count
					++cellCount;
				}
				else
				{
					if (validGround)
					{
						++cellCount;
						++groundNonMatchingCount;
					}
					else if (validCeil)
					{
						++cellCount;
						++ceilNonMatchingCount;
					}
					cell.h = std::numeric_limits<double>::quiet_NaN();
					cell.nbPoints = 0;
				}

				cell.avgHeight = (groundHeight + ceilHeight) / 2;
				cell.stdDevHeight = 0;

				if (pDlg && !nProgress.oneStep())
				{
					ccLog::Warning("[体积] 进程被用户取消");
					return false;
				}
			}
		}
		grid.validCellCount = grid.nonEmptyCellCount;

		//count the average number of valid neighbors
		{
			size_t validNeighborsCount = 0;
			size_t count = 0;
			for (unsigned i = 1; i < grid.height - 1; ++i)
			{
				for (unsigned j = 1; j < grid.width - 1; ++j)
				{
					ccRasterCell& cell = grid.rows[i][j];
					if (cell.h == cell.h)
					{
						for (unsigned k = i - 1; k <= i + 1; ++k)
						{
							for (unsigned l = j - 1; l <= j + 1; ++l)
							{
								if (k != i || l != j)
								{
									ccRasterCell& otherCell = grid.rows[k][l];
									if (std::isfinite(otherCell.h))
									{
										++validNeighborsCount;
									}
								}
							}
						}

						++count;
					}
				}
			}

			if (count)
			{
				reportInfo.averageNeighborsPerCell = static_cast<double>(validNeighborsCount) / count;
			}
		}

		reportInfo.matchingPrecent = static_cast<float>(grid.validCellCount * 100) / cellCount;
		reportInfo.groundNonMatchingPercent = static_cast<float>(groundNonMatchingCount * 100) / cellCount;
		reportInfo.ceilNonMatchingPercent = static_cast<float>(ceilNonMatchingCount * 100) / cellCount;
		float cellArea = static_cast<float>(grid.gridStep * grid.gridStep);
		reportInfo.volume *= cellArea;
		reportInfo.addedVolume *= cellArea;
		reportInfo.removedVolume *= cellArea;
		reportInfo.surface *= cellArea;
	}

	grid.setValid(true);

	return true;
}

bool ccVolumeCalcTool::updateGrid()
{
	if (!m_cloud2)
	{
		assert(false);
		return false;
	}

	//cloud bounding-box --> grid size
	ccBBox box = getCustomBBox();
	if (!box.isValid())
	{
		return false;
	}

	unsigned gridWidth = 0;
	unsigned gridHeight = 0;
	if (!getGridSize(gridWidth, gridHeight))
	{
		return false;
	}

	//grid step
	double gridStep = getGridStep();
	assert(gridStep != 0);

	//ground
	ccGenericPointCloud* groundCloud = nullptr;
	double groundHeight = 0;
	switch (groundComboBox->currentIndex())
	{
	case 0:
		groundHeight = groundEmptyValueDoubleSpinBox->value();
		break;
	case 1:
		groundCloud = m_cloud1 ? m_cloud1 : m_cloud2;
		break;
	case 2:
		groundCloud = m_cloud2;
		break;
	default:
		assert(false);
		return false;
	}

	//ceil
	ccGenericPointCloud* ceilCloud = nullptr;
	double ceilHeight = 0;
	switch (ceilComboBox->currentIndex())
	{
	case 0:
		ceilHeight = ceilEmptyValueDoubleSpinBox->value();
		break;
	case 1:
		ceilCloud = m_cloud1 ? m_cloud1 : m_cloud2;
		break;
	case 2:
		ceilCloud = m_cloud2;
		break;
	default:
		assert(false);
		return false;
	}

	ccVolumeCalcTool::ReportInfo reportInfo;

	if (ComputeVolume(m_grid,
		groundCloud,
		ceilCloud,
		box,
		getProjectionDimension(),
		gridStep,
		gridWidth,
		gridHeight,
		getTypeOfProjection(),
		getFillEmptyCellsStrategy(fillGroundEmptyCellsComboBox),
		getFillEmptyCellsStrategy(fillCeilEmptyCellsComboBox),
		reportInfo,
		groundHeight,
		ceilHeight,
		this))
	{
		outputReport(reportInfo);
		return true;
	}
	else
	{
		return false;
	}
}

void ccVolumeCalcTool::exportToClipboard() const
{
	QClipboard* clipboard = QApplication::clipboard();
	if (clipboard)
	{
		clipboard->setText(reportPlainTextEdit->toPlainText());
	}
}

void ccVolumeCalcTool::exportGridAsCloud() const
{
	if (!m_grid.isValid())
	{
		assert(false);
	}

	ccPointCloud* rasterCloud = convertGridToCloud(true);
	if (!rasterCloud)
	{
		//error message should have already been issued
		return;
	}

	rasterCloud->setName("高度差 " + rasterCloud->getName());
	ccGenericPointCloud* originCloud = (m_cloud1 ? m_cloud1 : m_cloud2);
	assert(originCloud);
	if (originCloud)
	{
		if (originCloud->getParent())
		{
			originCloud->getParent()->addChild(rasterCloud);
		}
		rasterCloud->setDisplay(originCloud->getDisplay());
	}

	MainWindow* mainWindow = MainWindow::TheInstance();
	if (mainWindow)
	{
		mainWindow->addToDB(rasterCloud);
		ccLog::Print(QString("[体积] 点云 '%1' 导出成功").arg(rasterCloud->getName()));
	}
	else
	{
		assert(false);
		delete rasterCloud;
	}
}

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

#include "ccGeomFeaturesDlg.h"

//Qt
#include <QPushButton>
#include <QDialogButtonBox>

ccGeomFeaturesDlg::ccGeomFeaturesDlg(QWidget* parent/*=nullptr*/)
	: QDialog(parent, Qt::Tool)
	, Ui::GeomFeaturesDialog()
{
	setupUi(this);


}

float ccGeomFeaturesDlg::radius()
{
	return doubleSpinBox->value();
}
int ccGeomFeaturesDlg::num()
{
	return spinBox->value();
}

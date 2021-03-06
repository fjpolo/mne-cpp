//=============================================================================================================
/**
* @file     brainmonitor.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     February, 2013
*
* @section  LICENSE
*
* Copyright (C) 2013, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Contains the implementation of the BrainMonitor class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "brainmonitor.h"

#include <xMeas/Measurement/sngchnmeasurement.h>

#include <xMeas/Measurement/realtimesamplearray.h>

#include "FormFiles/brainmonitorsetupwidget.h"
#include "FormFiles/brainmonitorrunwidget.h"

#include <fs/label.h>
#include <fs/surface.h>
#include <fs/annotationset.h>

#include <fiff/fiff_evoked.h>
#include <inverse/sourceestimate.h>
#include <inverse/minimumNorm/minimumnorm.h>

#include <disp3D/inverseview.h>


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtCore/QtPlugin>
#include <QDebug>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace BrainMonitorPlugin;
using namespace MNEX;
using namespace XMEASLIB;

using namespace MNELIB;
using namespace FSLIB;
using namespace FIFFLIB;
using namespace INVERSELIB;
using namespace DISP3DLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

BrainMonitor::BrainMonitor()
{

    // This needs Qt 5.1

//    //########################################################################################
//    // Source Estimate

//    QFile t_fileFwd("./MNE-sample-data/MEG/sample/sample_audvis-meg-eeg-oct-6-fwd.fif");
//    QFile t_fileCov("./MNE-sample-data/MEG/sample/sample_audvis-cov.fif");
//    QFile t_fileEvoked("./MNE-sample-data/MEG/sample/sample_audvis-ave.fif");

//    QFile t_fileClusteredInverse("./clusteredInverse-inv.fif");

//    double snr = 3.0;
//    double lambda2 = 1.0 / pow(snr, 2);
//    QString method("dSPM"); //"MNE" | "dSPM" | "sLORETA"

//    // Load data
//    fiff_int_t setno = 0;
//    QPair<QVariant, QVariant> baseline(QVariant(), 0);
//    FiffEvoked evoked(t_fileEvoked, setno, baseline);
//    if(evoked.isEmpty())
//        return;

//    MNEForwardSolution t_Fwd(t_fileFwd);
//    if(t_Fwd.isEmpty())
//        return;

//    AnnotationSet t_annotationSet("./MNE-sample-data/subjects/sample/label/lh.aparc.a2009s.annot", "./MNE-sample-data/subjects/sample/label/rh.aparc.a2009s.annot");

//    FiffCov noise_cov(t_fileCov);

//    // regularize noise covariance
//    noise_cov = noise_cov.regularize(evoked.info, 0.05, 0.05, 0.1, true);

//    //
//    // Cluster forward solution;
//    //
//    MNEForwardSolution t_clusteredFwd = t_Fwd.cluster_forward_solution(t_annotationSet, 40);

//    //
//    // make an inverse operators
//    //
//    FiffInfo info = evoked.info;

//    MNEInverseOperator inverse_operator(info, t_clusteredFwd, noise_cov, 0.2f, 0.8f);

//    inverse_operator.write_inverse_operator(t_fileClusteredInverse);

//    //
//    // Compute inverse solution
//    //
//    MinimumNorm minimumNorm(inverse_operator, lambda2, method);
//    SourceEstimate sourceEstimate = minimumNorm.calculateInverse(evoked);

//    if(sourceEstimate.isEmpty())
//        return;

//    // View activation time-series
//    std::cout << "\nsourceEstimate:\n" << sourceEstimate.data.block(0,0,10,10) << std::endl;
//    std::cout << "time\n" << sourceEstimate.times.block(0,0,1,10) << std::endl;
//    std::cout << "timeMin\n" << sourceEstimate.times[0] << std::endl;
//    std::cout << "timeMax\n" << sourceEstimate.times[sourceEstimate.times.size()-1] << std::endl;
//    std::cout << "time step\n" << sourceEstimate.tstep << std::endl;

//    //Source Estimate end
//    //########################################################################################

//    AnnotationSet t_annotSet("./MNE-sample-data/subjects/sample/label/lh.aparc.a2009s.annot","./MNE-sample-data/subjects/sample/label/rh.aparc.a2009s.annot");
//    SurfaceSet t_surfSet("./MNE-sample-data/subjects/sample/surf/lh.white", "./MNE-sample-data/subjects/sample/surf/rh.white");

//    QList<Label> t_qListLabels;
//    QList<RowVector4i> t_qListRGBAs;

//    //ToDo overload toLabels using instead of t_surfSet rr of MNESourceSpace
//    t_annotSet.toLabels(t_surfSet, t_qListLabels, t_qListRGBAs);

//    InverseView view(minimumNorm.getSourceSpace(), t_qListLabels, t_qListRGBAs);

//    if (view.stereoType() != QGLView::RedCyanAnaglyph)
//        view.camera()->setEyeSeparation(0.3f);
//    QStringList args = QCoreApplication::arguments();
//    int w_pos = args.indexOf("-width");
//    int h_pos = args.indexOf("-height");
//    if (w_pos >= 0 && h_pos >= 0)
//    {
//        bool ok = true;
//        int w = args.at(w_pos + 1).toInt(&ok);
//        if (!ok)
//        {
//            qWarning() << "Could not parse width argument:" << args;
//            return;
//        }
//        int h = args.at(h_pos + 1).toInt(&ok);
//        if (!ok)
//        {
//            qWarning() << "Could not parse height argument:" << args;
//            return;
//        }
//        view.resize(w, h);
//    }
//    else
//    {
//        view.resize(800, 600);
//    }
//    view.show();


}


//*************************************************************************************************************

BrainMonitor::~BrainMonitor()
{
    stop();

}


//*************************************************************************************************************

bool BrainMonitor::start()
{
    // Initialize displaying widgets
    init();

    QThread::start();
    return true;
}


//*************************************************************************************************************

bool BrainMonitor::stop()
{
    // Stop threads
    QThread::terminate();
    QThread::wait();


    return true;
}


//*************************************************************************************************************

Type BrainMonitor::getType() const
{
    return _IRTVisualization;
}


//*************************************************************************************************************

const char* BrainMonitor::getName() const
{
    return "Brain Monitor";
}


//*************************************************************************************************************

QWidget* BrainMonitor::setupWidget()
{
    BrainMonitorSetupWidget* setupWidget = new BrainMonitorSetupWidget(this);//widget is later distroyed by CentralWidget - so it has to be created everytime new
    return setupWidget;
}


//*************************************************************************************************************

QWidget* BrainMonitor::runWidget()
{
    BrainMonitorRunWidget* runWidget = new BrainMonitorRunWidget(this);//widget is later distroyed by CentralWidget - so it has to be created everytime new
    return runWidget;
}


//*************************************************************************************************************

void BrainMonitor::update(Subject* pSubject)
{

}


//*************************************************************************************************************

void BrainMonitor::run()
{
    while (true)
    {
//        double v_one = m_pDummyMultiChannelBuffer->pop(0);

//        double v_two = m_pDummyMultiChannelBuffer->pop(1);

    }
}


//*************************************************************************************************************
//=============================================================================================================
// Creating required display instances and set configurations
//=============================================================================================================

void BrainMonitor::init()
{

}

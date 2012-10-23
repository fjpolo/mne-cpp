
//=============================================================================================================
/**
* @file     fiff_file.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti H�m�l�inen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     July, 2012
*
* @section  LICENSE
*
* Copyright (C) 2012, Christoph Dinh and Matti Hamalainen. All rights reserved.
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
* @brief    Contains the implementation of the FiffFile Class.
*
*/


//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "fiff_file.h"
#include "fiff_tag.h"
#include "fiff_dir_tree.h"


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FIFFLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffFile::FiffFile(QString& p_sFilename)
: QFile(p_sFilename)
{

}


//*************************************************************************************************************

FiffFile::~FiffFile()
{
    if(this->isOpen())
    {
        printf("Closing file %s.\n", this->fileName().toUtf8().constData());
        this->close();
    }
}


//*************************************************************************************************************

void FiffFile::end_block(fiff_int_t kind)
{
    this->write_int(FIFF_BLOCK_END,&kind);
}


//*************************************************************************************************************

void FiffFile::end_file()
{
    fiff_int_t datasize = 0;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)FIFF_NOP;
    out << (qint32)FIFFT_VOID;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_NONE;
}


//*************************************************************************************************************

void FiffFile::finish_writing_raw()
{
    this->end_block(FIFFB_RAW_DATA);
    this->end_block(FIFFB_MEAS);
    this->end_file();
}


//*************************************************************************************************************

bool FiffFile::open(FiffDirTree*& p_pTree, QList<FiffDirEntry>*& p_pDir)
{

    if (!this->open(QIODevice::ReadOnly))
    {
        printf("Cannot open file %s\n", this->fileName().toUtf8().constData());//consider throw
        return false;
    }

    FIFFLIB::FiffTag* t_pTag = NULL;
    FiffTag::read_tag_info(this, t_pTag);

    if (t_pTag->kind != FIFF_FILE_ID)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }

    if (t_pTag->type != FIFFT_ID_STRUCT)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }
    if (t_pTag->size != 20)
    {
        printf("Fiff::open: file does not start with a file id tag");//consider throw
        return false;
    }

    FiffTag::read_tag(this, t_pTag);

    if (t_pTag->kind != FIFF_DIR_POINTER)
    {
        printf("Fiff::open: file does have a directory pointer");//consider throw
        return false;
    }

    //
    //   Read or create the directory tree
    //
    printf("\nCreating tag directory for %s...", this->fileName().toUtf8().constData());

    if (p_pDir)
        delete p_pDir;
    p_pDir = new QList<FiffDirEntry>;

    qint32 dirpos = *t_pTag->toInt();
    if (dirpos > 0)
    {
        FiffTag::read_tag(this, t_pTag, dirpos);
        *p_pDir = t_pTag->toDirEntry();
    }
    else
    {
        qint32 k = 0;
        this->seek(0);//fseek(fid,0,'bof');
        FiffDirEntry t_fiffDirEntry;
        while (t_pTag->next >= 0)
        {
            t_fiffDirEntry.pos = this->pos();//pos = ftell(fid);
            FiffTag::read_tag_info(this, t_pTag);
            ++k;
            t_fiffDirEntry.kind = t_pTag->kind;
            t_fiffDirEntry.type = t_pTag->type;
            t_fiffDirEntry.size = t_pTag->size;
            p_pDir->append(t_fiffDirEntry);
        }
    }
    delete t_pTag;
    //
    //   Create the directory tree structure
    //

    FiffDirTree::make_dir_tree(this, p_pDir, p_pTree);

    printf("[done]\n");

    //
    //   Back to the beginning
    //
    this->seek(0); //fseek(fid,0,'bof');
    return true;
}


//*************************************************************************************************************

QStringList FiffFile::read_bad_channels(FiffDirTree* p_pTree)
{
    QList<FiffDirTree*> node = p_pTree->dir_tree_find(FIFFB_MNE_BAD_CHANNELS);
    FIFFLIB::FiffTag* t_pTag = NULL;

    QStringList bads;

    if (node.size() > 0)
        if(node.at(0)->find_tag(this, FIFF_MNE_CH_NAME_LIST, t_pTag))
            bads = split_name_list(t_pTag->toString());

    return bads;
}


//*************************************************************************************************************

bool FiffFile::read_cov(FiffDirTree* node, fiff_int_t cov_kind, FiffCov*& p_covData)
{
    if(p_covData)
        delete p_covData;
    p_covData = NULL;

    //
    //   Find all covariance matrices
    //
    QList<FiffDirTree*> covs = node->dir_tree_find(FIFFB_MNE_COV);
    if (covs.size() == 0)
    {
        printf("No covariance matrices found");
        return false;
    }
    //
    //   Is any of the covariance matrices a noise covariance
    //
    qint32 p = 0;
    FiffTag* tag = NULL;
    bool success = false;
    fiff_int_t dim, nfree, nn;
    QStringList names;
    bool diagmat = false;
    VectorXd* eig = NULL;
    MatrixXf* eigvec = NULL;
    VectorXd* cov_diag = NULL;
    VectorXd* cov = NULL;
    VectorXd* cov_sparse = NULL;
    QStringList bads;
    for(p = 0; p < covs.size(); ++p)
    {
        success = covs[p]->find_tag(this, FIFF_MNE_COV_KIND, tag);
        if (success && *tag->toInt() == cov_kind)
        {
            FiffDirTree* current = covs[p];
            //
            //   Find all the necessary data
            //
            if (!current->find_tag(this, FIFF_MNE_COV_DIM, tag))
            {
                printf("Covariance matrix dimension not found.\n");
                return false;
            }
            dim = *tag->toInt();
            if (!current->find_tag(this, FIFF_MNE_COV_NFREE, tag))
                nfree = -1;
            else
                nfree = *tag->toInt();

            if (current->find_tag(this, FIFF_MNE_ROW_NAMES, tag))
            {
                names = FiffFile::split_name_list(tag->toString());
                if (names.size() != dim)
                {
                    printf("Number of names does not match covariance matrix dimension\n");
                    return false;
                }
            }
            if (!current->find_tag(this, FIFF_MNE_COV, tag))
            {
                if (!current->find_tag(this, FIFF_MNE_COV_DIAG, tag))
                {
                    printf("No covariance matrix data found\n");
                    return false;
                }
                else
                {
                    //
                    //   Diagonal is stored
                    //
                    if (tag->type == FIFFT_DOUBLE)
                    {
                        cov_diag = new VectorXd(Map<VectorXd>(tag->toDouble(),dim));
                    }
                    else if (tag->type == FIFFT_FLOAT)
                    {
                        cov_diag = new VectorXd(Map<VectorXf>(tag->toFloat(),dim).cast<double>());
                    }
                    else {
                        printf("Illegal data type for covariance matrix\n");
                        return false;
                    }

                    diagmat = true;
                    printf("\t%d x %d diagonal covariance (kind = %d) found.\n", dim, dim, cov_kind);
                }
            }
            else
            {
                diagmat = false;
                nn = dim*(dim+1)/2;
                if (tag->type == FIFFT_DOUBLE)
                {
                    cov =  new VectorXd(Map<VectorXd>(tag->toDouble(),nn));
                }
                else if (tag->type == FIFFT_FLOAT)
                {
                    cov = new VectorXd(Map<VectorXf>(tag->toFloat(),nn).cast<double>());
                }
                else
                {
                    qDebug() << tag->getInfo();
                    return false;
                }


//                    if ~issparse(tag.data)
//                        //
//                        //   Lower diagonal is stored
//                        //
//                        qDebug() << tag->getInfo();
//                        vals = tag.data;
//                        data = zeros(dim,dim);
//                        % XXX : should remove for loops
//                        q = 1;
//                        for j = 1:dim
//                            for k = 1:j
//                                data(j,k) = vals(q);
//                                q = q + 1;
//                            end
//                        end
//                        for j = 1:dim
//                            for k = j+1:dim
//                                data(j,k) = data(k,j);
//                            end
//                        end
//                        diagmat = false;
//                        fprintf('\t%d x %d full covariance (kind = %d) found.\n',dim,dim,cov_kind);
//                    else
//                        diagmat = false;
//                        data = tag.data;
//                        fprintf('\t%d x %d sparse covariance (kind = %d) found.\n',dim,dim,cov_kind);
//                    end
            }
            //
            //   Read the possibly precomputed decomposition
            //
            FiffTag* tag1 = NULL;
            FiffTag* tag2 = NULL;
            if (current->find_tag(this, FIFF_MNE_COV_EIGENVALUES, tag1) && current->find_tag(this, FIFF_MNE_COV_EIGENVECTORS, tag2))
            {
                eig = new VectorXd(Map<VectorXd>(tag1->toDouble(),dim));
                eigvec = tag2->toFloatMatrix();
                eigvec->transposeInPlace();
            }
            //
            //   Read the projection operator
            //
            QList<FiffProj*> projs = this->read_proj(current);
            //
            //   Read the bad channel list
            //
            bads = this->read_bad_channels(current);
            //
            //   Put it together
            //
            p_covData = new FiffCov();

            p_covData->kind   = cov_kind;
            p_covData->diag   = diagmat;
            p_covData->dim    = dim;
            p_covData->names  = names;

            if(cov_diag)
                p_covData->data   = new MatrixXd(*cov_diag);
            else if(cov)
                p_covData->data   = new MatrixXd(*cov);
            else if(cov_sparse)
                p_covData->data   = new MatrixXd(*cov_sparse);

            p_covData->projs  = projs;
            p_covData->bads   = bads;
            p_covData->nfree  = nfree;
            p_covData->eig    = eig;
            p_covData->eigvec = eigvec;

            if (cov_diag)
                delete cov_diag;
            if (cov)
                delete cov;
            if (cov_sparse)
                delete cov_sparse;

            if (tag)
                delete tag;
            //
            return true;
        }
    }

    printf("Did not find the desired covariance matrix\n");
    return false;
}


//*************************************************************************************************************

QList<FiffCtfComp*> FiffFile::read_ctf_comp(FiffDirTree* p_pNode, QList<FiffChInfo>& chs)
{
    QList<FiffCtfComp*> compdata;
    QList<FiffDirTree*> t_qListComps = p_pNode->dir_tree_find(FIFFB_MNE_CTF_COMP_DATA);

    qint32 i, k, p, col, row;
    fiff_int_t kind, pos;
    FiffTag* t_pTag = NULL;
    for (k = 0; k < t_qListComps.size(); ++k)
    {
        FiffDirTree* node = t_qListComps.at(k);
        //
        //   Read the data we need
        //
        FiffNamedMatrix* mat = NULL;
        this->read_named_matrix(node, FIFF_MNE_CTF_COMP_DATA, mat);
        for(p = 0; p < node->nent; ++p)
        {
            kind = node->dir.at(p).kind;
            pos  = node->dir.at(p).pos;
            if (kind == FIFF_MNE_CTF_COMP_KIND)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                break;
            }
        }
        if (!t_pTag)
        {
            printf("Compensation type not found\n");
            return compdata;
        }
        //
        //   Get the compensation kind and map it to a simple number
        //
        FiffCtfComp* one = new FiffCtfComp();
        one->ctfkind = *t_pTag->toInt();
        delete t_pTag;
        t_pTag = NULL;

        one->kind   = -1;
        if (one->ctfkind == 1194410578) //hex2dec('47314252')
            one->kind = 1;
        else if (one->ctfkind == 1194476114) //hex2dec('47324252')
            one->kind = 2;
        else if (one->ctfkind == 1194541650) //hex2dec('47334252')
            one->kind = 3;
        else
            one->kind = one->ctfkind;

        for (p = 0; p < node->nent; ++p)
        {
            kind = node->dir.at(p).kind;
            pos  = node->dir.at(p).pos;
            if (kind == FIFF_MNE_CTF_COMP_CALIBRATED)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                break;
            }
        }
        bool calibrated;
        if (!t_pTag)
            calibrated = false;
        else
            calibrated = (bool)*t_pTag->toInt();

        one->save_calibrated = calibrated;
        one->rowcals = MatrixXf::Ones(1,mat->data->rows());//ones(1,size(mat.data,1));
        one->colcals = MatrixXf::Ones(1,mat->data->cols());//ones(1,size(mat.data,2));
        if (!calibrated)
        {
            //
            //   Calibrate...
            //
            //
            //   Do the columns first
            //
            QStringList ch_names;
            for (p  = 0; p < chs.size(); ++p)
                ch_names.append(chs.at(p).ch_name);

            qint32 count;
            MatrixXf col_cals(mat->data->cols(), 1);
            col_cals.setZero();
            for (col = 0; col < mat->data->cols(); ++col)
            {
                count = 0;
                for (i = 0; i < ch_names.size(); ++i)
                {
                    if (QString::compare(mat->col_names.at(col),ch_names.at(i)) == 0)
                    {
                        count += 1;
                        p = i;
                    }
                }
                if (count == 0)
                {
                    printf("Channel %s is not available in data",mat->col_names.at(col).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                else if (count > 1)
                {
                    printf("Ambiguous channel %s",mat->col_names.at(col).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                col_cals(col,0) = 1.0f/(chs.at(p).range*chs.at(p).cal);
            }
            //
            //    Then the rows
            //
            MatrixXf row_cals(mat->data->rows(), 1);
            row_cals.setZero();
            for (row = 0; row < mat->data->rows(); ++row)
            {
                count = 0;
                for (i = 0; i < ch_names.size(); ++i)
                {
                    if (QString::compare(mat->row_names.at(row),ch_names.at(i)) == 0)
                    {
                        count += 1;
                        p = i;
                    }
                }

                if (count == 0)
                {
                    printf("Channel %s is not available in data",mat->row_names.at(row).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }
                else if (count > 1)
                {
                    printf("Ambiguous channel %s",mat->row_names.at(row).toUtf8().constData());
                    delete t_pTag;
                    return compdata;
                }

                row_cals(row, 0) = chs.at(p).range*chs.at(p).cal;
            }
            *mat->data           = row_cals.asDiagonal()* (*mat->data) *col_cals.asDiagonal();
            one->rowcals         = row_cals;
            one->colcals         = col_cals;
        }
        one->data       = mat;
        compdata.append(one);
    }

    if (compdata.size() > 0)
        printf("\tRead %d compensation matrices\n",compdata.size());

    if(t_pTag)
        delete t_pTag;
    return compdata;
}


//*************************************************************************************************************

FiffDirTree* FiffFile::read_meas_info(FiffDirTree* p_pTree, FiffInfo*& info)
{
    if (info)
        delete info;
    info = NULL;
    //
    //   Find the desired blocks
    //
    QList<FiffDirTree*> meas = p_pTree->dir_tree_find(FIFFB_MEAS);

    if (meas.size() == 0)
    {
        printf("Could not find measurement data\n");
        return NULL;
    }
    //
    QList<FiffDirTree*> meas_info = meas.at(0)->dir_tree_find(FIFFB_MEAS_INFO);
    if (meas_info.count() == 0)
    {
        printf("Could not find measurement info\n");
        delete meas[0];
        return NULL;
    }
    //
    //   Read measurement info
    //
    FiffTag* t_pTag = NULL;

    fiff_int_t nchan = -1;
    float sfreq = -1.0f;
    QList<FiffChInfo> chs;
    float lowpass = -1.0f;
    float highpass = -1.0f;

    FiffChInfo t_chInfo;

    FiffCoordTrans* cand = NULL;
    FiffCoordTrans* dev_head_t = NULL;
    FiffCoordTrans* ctf_head_t = NULL;

    fiff_int_t meas_date[2];
    meas_date[0] = -1;
    meas_date[1] = -1;

    fiff_int_t kind = -1;
    fiff_int_t pos = -1;

    for (qint32 k = 0; k < meas_info.at(0)->nent; ++k)
    {
        kind = meas_info.at(0)->dir.at(k).kind;
        pos  = meas_info.at(0)->dir.at(k).pos;
        switch (kind)
        {
            case FIFF_NCHAN:
                FiffTag::read_tag(this, t_pTag, pos);
                nchan = *t_pTag->toInt();
                break;
            case FIFF_SFREQ:
                FiffTag::read_tag(this, t_pTag, pos);
                sfreq = *t_pTag->toFloat();
                break;
            case FIFF_CH_INFO:
                FiffTag::read_tag(this, t_pTag, pos);
                chs.append( t_pTag->toChInfo() );
                break;
            case FIFF_LOWPASS:
                FiffTag::read_tag(this, t_pTag, pos);
                lowpass = *t_pTag->toFloat();
                break;
            case FIFF_HIGHPASS:
                FiffTag::read_tag(this, t_pTag, pos);
                highpass = *t_pTag->toFloat();
                break;
            case FIFF_MEAS_DATE:
                FiffTag::read_tag(this, t_pTag, pos);
                meas_date[0] = t_pTag->toInt()[0];
                meas_date[1] = t_pTag->toInt()[1];
                break;
            case FIFF_COORD_TRANS:
                //ToDo: This has to be debugged!!
                FiffTag::read_tag(this, t_pTag, pos);
                cand = t_pTag->toCoordTrans();
                if(cand->from == FIFFV_COORD_DEVICE && cand->to == FIFFV_COORD_HEAD)
                    dev_head_t = cand;
                else if (cand->from == FIFFV_MNE_COORD_CTF_HEAD && cand->to == FIFFV_COORD_HEAD)
                    ctf_head_t = cand;
                break;
        }
    }
    //
    //   Check that we have everything we need
    //
    if (nchan < 0)
    {
        printf("Number of channels in not defined\n");
        delete meas[0];
        return NULL;
    }
    if (sfreq < 0)
    {
        printf("Sampling frequency is not defined\n");
        delete meas[0];
        return NULL;
    }
    if (chs.size() == 0)
    {
        printf("Channel information not defined\n");
        delete meas[0];
        return NULL;
    }
    if (chs.size() != nchan)
    {
        printf("Incorrect number of channel definitions found\n");
        delete meas[0];
        return NULL;
    }

    if ((dev_head_t == NULL) || (ctf_head_t == NULL)) //if isempty(dev_head_t) || isempty(ctf_head_t)
    {
        QList<FiffDirTree*> hpi_result = meas_info.at(0)->dir_tree_find(FIFFB_HPI_RESULT);
        if (hpi_result.size() == 1)
        {
            for( qint32 k = 0; k < hpi_result.at(0)->nent; ++k)
            {
                kind = hpi_result.at(0)->dir.at(k).kind;
                pos  = hpi_result.at(0)->dir.at(k).pos;
                if (kind == FIFF_COORD_TRANS)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    cand = t_pTag->toCoordTrans();
                    if (cand->from == FIFFV_COORD_DEVICE && cand->to == FIFFV_COORD_HEAD)
                        dev_head_t = cand;
                    else if (cand->from == FIFFV_MNE_COORD_CTF_HEAD && cand->to == FIFFV_COORD_HEAD)
                        ctf_head_t = cand;
                }
            }
        }
    }
    //
    //   Locate the Polhemus data
    //
    QList<FiffDirTree*> isotrak = meas_info.at(0)->dir_tree_find(FIFFB_ISOTRAK);

    QList<FiffDigPoint> dig;// = struct('kind',{},'ident',{},'r',{},'coord_frame',{});
    fiff_int_t coord_frame = FIFFV_COORD_HEAD;
    FiffCoordTrans* dig_trans = NULL;
    qint32 k = 0;

    if (isotrak.size() == 1)
    {
        for (k = 0; k < isotrak.at(0)->nent; ++k)
        {
            kind = isotrak.at(0)->dir.at(k).kind;
            pos  = isotrak.at(0)->dir.at(k).pos;
            if (kind == FIFF_DIG_POINT)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                dig.append(t_pTag->toDigPoint());
            }
            else
            {
                if (kind == FIFF_MNE_COORD_FRAME)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    qDebug() << "NEEDS To BE DEBBUGED: FIFF_MNE_COORD_FRAME" << t_pTag->getType();
                    coord_frame = *t_pTag->toInt();
                }
                else if (kind == FIFF_COORD_TRANS)
                {
                    FiffTag::read_tag(this, t_pTag, pos);
                    qDebug() << "NEEDS To BE DEBBUGED: FIFF_COORD_TRANS" << t_pTag->getType();
                    dig_trans = t_pTag->toCoordTrans();
                }
            }
        }
    }
    for(k = 0; k < dig.size(); ++k)
        dig[k].coord_frame = coord_frame;

    if (dig_trans != NULL) //if exist('dig_trans','var')
    {
        if (dig_trans->from != coord_frame && dig_trans->to != coord_frame)
        {
            delete dig_trans;
            dig_trans = NULL; //clear('dig_trans');

        }
    }

    //
    //   Locate the acquisition information
    //
    QList<FiffDirTree*> acqpars = meas_info.at(0)->dir_tree_find(FIFFB_DACQ_PARS);
    QString acq_pars;
    QString acq_stim;
    if (acqpars.size() == 1)
    {
        for( k = 0; k < acqpars.at(0)->nent; ++k)
        {
            kind = acqpars.at(0)->dir.at(k).kind;
            pos  = acqpars.at(0)->dir.at(k).pos;
            if (kind == FIFF_DACQ_PARS)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                acq_pars = t_pTag->toString();
            }
            else if (kind == FIFF_DACQ_STIM)
            {
                FiffTag::read_tag(this, t_pTag, pos);
                acq_stim = t_pTag->toString();
            }
        }
    }
    //
    //   Load the SSP data
    //
    QList<FiffProj*> projs = this->read_proj(meas_info[0]);//ToDo Member Function
    //
    //   Load the CTF compensation data
    //
    QList<FiffCtfComp*> comps = this->read_ctf_comp(meas_info[0], chs);//ToDo Member Function
    //
    //   Load the bad channel list
    //
    QStringList bads = this->read_bad_channels(p_pTree);
    //
    //   Put the data together
    //
    info = new FiffInfo();
    if (p_pTree->id.version != -1)
        info->file_id = p_pTree->id;
    else
        info->file_id.version = -1;

    //
    //  Make the most appropriate selection for the measurement id
    //
    if (meas_info.at(0)->parent_id.version == -1)
    {
        if (meas_info.at(0)->id.version == -1)
        {
            if (meas.at(0)->id.version == -1)
            {
                if (meas.at(0)->parent_id.version == -1)
                    info->meas_id = info->file_id;
                else
                    info->meas_id = meas.at(0)->parent_id;
            }
            else
                info->meas_id = meas.at(0)->id;
        }
        else
            info->meas_id = meas_info.at(0)->id;
    }
    else
        info->meas_id = meas_info.at(0)->parent_id;

    if (meas_date[0] == -1)
    {
        info->meas_date[0] = info->meas_id.time.secs;
        info->meas_date[1] = info->meas_id.time.usecs;
    }
    else
    {
        info->meas_date[0] = meas_date[0];
        info->meas_date[1] = meas_date[1];
    }

    info->nchan  = nchan;
    info->sfreq  = sfreq;
    if (highpass != -1.0f)
        info->highpass = highpass;
    else
        info->highpass = 0.0f;

    if (lowpass != -1.0f)
        info->lowpass = lowpass;
    else
        info->lowpass = info->sfreq/2.0;

    //
    //   Add the channel information and make a list of channel names
    //   for convenience
    //
    info->chs = chs;
    for (qint32 c = 0; c < info->nchan; ++c)
        info->ch_names << info->chs.at(c).ch_name;

    //
    //  Add the coordinate transformations
    //
    info->dev_head_t = dev_head_t;
    info->ctf_head_t = ctf_head_t;
    if ((info->dev_head_t != NULL) && (info->ctf_head_t != NULL)) //~isempty(info.dev_head_t) && ~isempty(info.ctf_head_t)
    {
        info->dev_ctf_t     = info->dev_head_t;
        info->dev_ctf_t->to = info->ctf_head_t->from;
        info->dev_ctf_t->trans = ctf_head_t->trans.inverse()*info->dev_ctf_t->trans;
    }
    else
    {
        if(info->dev_ctf_t)
            delete info->dev_ctf_t;
        info->dev_ctf_t = NULL;
    }

    //
    //   All kinds of auxliary stuff
    //
    info->dig   = dig;
    if (dig_trans != NULL)
        info->dig_trans = dig_trans;

    info->bads  = bads;
    info->projs = projs;
    info->comps = comps;
    info->acq_pars = acq_pars;
    info->acq_stim = acq_stim;

    return meas[0];
}


//*************************************************************************************************************

bool FiffFile::read_named_matrix(FiffDirTree* p_pTree, fiff_int_t matkind, FiffNamedMatrix*& mat)
{
    if (mat != NULL)
        delete mat;
    mat = new FiffNamedMatrix();

    FiffDirTree* node = p_pTree;
    //
    //   Descend one level if necessary
    //
    bool found_it = false;
    if (node->block != FIFFB_MNE_NAMED_MATRIX)
    {
        for (int k = 0; k < node->nchild; ++k)
        {
            if (node->children.at(k)->block == FIFFB_MNE_NAMED_MATRIX)
            {
                if(node->children.at(k)->has_tag(matkind))
                {
                    node = node->children.at(k);
                    found_it = true;
                    break;
                }
            }
       }
       if (!found_it)
       {
          printf("Fiff::read_named_matrix: Desired named matrix (kind = %d) not available\n",matkind);
          return false;
       }
    }
    else
    {
        if (!node->has_tag(matkind))
        {
            printf("Desired named matrix (kind = %d) not available",matkind);
            return false;
        }
    }

    FIFFLIB::FiffTag* t_pTag = NULL;
    //
    //   Read everything we need
    //
    if(!node->find_tag(this, matkind, t_pTag))
    {
        printf("Matrix data missing.\n");
        return false;
    }
    else
    {
        //qDebug() << "Is Matrix" << t_pTag->isMatrix() << "Special Type:" << t_pTag->getType();
        mat->data = t_pTag->toFloatMatrix();
        mat->data->transposeInPlace();
    }

    mat->nrow = mat->data->rows();
    mat->ncol = mat->data->cols();

    if(node->find_tag(this, FIFF_MNE_NROW, t_pTag))
        if (*t_pTag->toInt() != mat->nrow)
        {
            printf("Number of rows in matrix data and FIFF_MNE_NROW tag do not match");
            return false;
        }
    if(node->find_tag(this, FIFF_MNE_NCOL, t_pTag))
        if (*t_pTag->toInt() != mat->ncol)
        {
            printf("Number of columns in matrix data and FIFF_MNE_NCOL tag do not match");
            return false;
        }

    QString row_names;
    if(node->find_tag(this, FIFF_MNE_ROW_NAMES, t_pTag))
        row_names = t_pTag->toString();

    QString col_names;
    if(node->find_tag(this, FIFF_MNE_COL_NAMES, t_pTag))
        col_names = t_pTag->toString();

    //
    //   Put it together
    //
    if (!row_names.isEmpty())
        mat->row_names = split_name_list(row_names);

    if (!col_names.isEmpty())
        mat->col_names = split_name_list(col_names);

    return true;
}


//*************************************************************************************************************

QList<FiffProj*> FiffFile::read_proj(FiffDirTree* p_pNode)
{
    QList<FiffProj*> projdata;// = struct('kind',{},'active',{},'desc',{},'data',{});
    //
    //   Locate the projection data
    //
    QList<FiffDirTree*> t_qListNodes = p_pNode->dir_tree_find(FIFFB_PROJ);
    if ( t_qListNodes.size() == 0 )
        return projdata;


    FIFFLIB::FiffTag* t_pTag = NULL;
    t_qListNodes.at(0)->find_tag(this, FIFF_NCHAN, t_pTag);
    fiff_int_t global_nchan;
    if (t_pTag)
        global_nchan = *t_pTag->toInt();


    fiff_int_t nchan;
    QList<FiffDirTree*> t_qListItems = t_qListNodes.at(0)->dir_tree_find(FIFFB_PROJ_ITEM);
    for ( qint32 i = 0; i < t_qListItems.size(); ++i)
    {
        //
        //   Find all desired tags in one item
        //
        FiffDirTree* t_pFiffDirTreeItem = t_qListItems[i];
        t_pFiffDirTreeItem->find_tag(this, FIFF_NCHAN, t_pTag);
        if (t_pTag)
            nchan = *t_pTag->toInt();
        else
            nchan = global_nchan;

        t_pFiffDirTreeItem->find_tag(this, FIFF_DESCRIPTION, t_pTag);
        QString desc; // maybe, in some cases this has to be a struct.
        if (t_pTag)
        {
            qDebug() << "read_proj: this has to be debugged";
            desc = t_pTag->toString();
        }
        else
        {
            t_pFiffDirTreeItem->find_tag(this, FIFF_NAME, t_pTag);
            if (t_pTag)
                desc = t_pTag->toString();
            else
            {
                printf("Projection item description missing\n");
                return projdata;
            }
        }
//            t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_CH_NAME_LIST, t_pTag);
//            QString namelist;
//            if (t_pTag)
//            {
//                namelist = t_pTag->toString();
//            }
//            else
//            {
//                printf("Projection item channel list missing\n");
//                return projdata;
//            }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_KIND, t_pTag);
        fiff_int_t kind;
        if (t_pTag)
        {
            kind = *t_pTag->toInt();
        }
        else
        {
            printf("Projection item kind missing");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_NVEC, t_pTag);
        fiff_int_t nvec;
        if (t_pTag)
        {
            nvec = *t_pTag->toInt();
        }
        else
        {
            printf("Number of projection vectors not specified\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_CH_NAME_LIST, t_pTag);
        QStringList names;
        if (t_pTag)
        {
            names = split_name_list(t_pTag->toString());
        }
        else
        {
            printf("Projection item channel list missing\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_PROJ_ITEM_VECTORS, t_pTag);
        MatrixXf* data = NULL;
        if (t_pTag)
        {
            data = t_pTag->toFloatMatrix();
            data->transposeInPlace();
        }
        else
        {
            printf("Projection item data missing\n");
            return projdata;
        }
        t_pFiffDirTreeItem->find_tag(this, FIFF_MNE_PROJ_ITEM_ACTIVE, t_pTag);
        bool active;
        if (t_pTag)
            active = *t_pTag->toInt();
        else
            active = false;

        if (data->cols() != names.size())
        {
            printf("Number of channel names does not match the size of data matrix\n");
            delete data;
            return projdata;
        }



        //
        //   create a named matrix for the data
        //
        QStringList defaultList;
        FiffNamedMatrix* t_fiffNamedMatrix = new FiffNamedMatrix(nvec, nchan, defaultList, names, data);
        delete data;

        FiffProj* one = new FiffProj(kind, active, desc, t_fiffNamedMatrix);
        //
        projdata.append(one);
    }

    if (projdata.size() > 0)
    {
        printf("\tRead a total of %d projection items:\n", projdata.size());
        for(qint32 k = 0; k < projdata.size(); ++k)
        {
            printf("\t\t%s (%d x %d)",projdata.at(k)->desc.toUtf8().constData(), projdata.at(k)->data->nrow, projdata.at(k)->data->ncol);
            if (projdata.at(k)->active)
                printf(" active\n");
            else
                printf(" idle\n");
        }
    }

    if (t_pTag)
        delete t_pTag;

    return projdata;
}


//*************************************************************************************************************

bool FiffFile::setup_read_raw(QString& p_sFileName, FiffRawData*& data, bool allow_maxshield)
{
    if(data)
        delete data;
    data = NULL;

    //
    //   Open the file
    //
    printf("Opening raw data file %s...\n",p_sFileName.toUtf8().constData());

    FiffFile* p_pFile = new FiffFile(p_sFileName);
    FiffDirTree* t_pTree = NULL;
    QList<FiffDirEntry>* t_pDir = NULL;

    if(!p_pFile->open(t_pTree, t_pDir))
    {
        if(p_pFile)
            delete p_pFile;
        if(t_pTree)
            delete t_pTree;
        if(t_pDir)
            delete t_pDir;

        return false;
    }

    //
    //   Read the measurement info
    //
//        [ info, meas ] = fiff_read_meas_info(fid,tree);
    FiffInfo* info = NULL;
    FiffDirTree* meas = p_pFile->read_meas_info(t_pTree, info);

    if (!meas)
        return false; //ToDo garbage collecting

    //
    //   Locate the data of interest
    //
    QList<FiffDirTree*> raw = meas->dir_tree_find(FIFFB_RAW_DATA);
    if (raw.size() == 0)
    {
        raw = meas->dir_tree_find(FIFFB_CONTINUOUS_DATA);
        if(allow_maxshield)
        {
            for (qint32 i = 0; i < raw.size(); ++i)
                if(raw[i])
                    delete raw[i];
            raw = meas->dir_tree_find(FIFFB_SMSH_RAW_DATA);
            if (raw.size() == 0)
            {
                printf("No raw data in %s\n", p_sFileName.toUtf8().constData());
                if(p_pFile)
                    delete p_pFile;
                if(t_pTree)
                    delete t_pTree;
                if(t_pDir)
                    delete t_pDir;

                return false;
            }
        }
        else
        {
            if (raw.size() == 0)
            {
                printf("No raw data in %s\n", p_sFileName.toUtf8().constData());
                if(p_pFile)
                    delete p_pFile;
                if(t_pTree)
                    delete t_pTree;
                if(t_pDir)
                    delete t_pDir;

                return false;
            }
        }
    }

    //
    //   Set up the output structure
    //
    info->filename   = p_sFileName;

    data = new FiffRawData();
    data->file = p_pFile;// fid;
    data->info       = info;
    data->first_samp = 0;
    data->last_samp  = 0;
    //
    //   Process the directory
    //
    QList<FiffDirEntry> dir = raw.at(0)->dir;
    fiff_int_t nent = raw.at(0)->nent;
    fiff_int_t nchan = info->nchan;
    fiff_int_t first = 0;
    fiff_int_t first_samp = 0;
    fiff_int_t first_skip   = 0;
    //
    //  Get first sample tag if it is there
    //
    FiffTag* t_pTag = NULL;
    if (dir.at(first).kind == FIFF_FIRST_SAMPLE)
    {
        FiffTag::read_tag(p_pFile, t_pTag, dir.at(first).pos);
        first_samp = *t_pTag->toInt();
        ++first;
    }
    //
    //  Omit initial skip
    //
    if (dir.at(first).kind == FIFF_DATA_SKIP)
    {
        //
        //  This first skip can be applied only after we know the buffer size
        //
        FiffTag::read_tag(p_pFile, t_pTag, dir.at(first).pos);
        first_skip = *t_pTag->toInt();
        ++first;
    }
    data->first_samp = first_samp;
    //
    //   Go through the remaining tags in the directory
    //
    QList<FiffRawDir> rawdir;
//        rawdir = struct('ent',{},'first',{},'last',{},'nsamp',{});
    fiff_int_t nskip = 0;
    fiff_int_t ndir  = 0;
    fiff_int_t nsamp = 0;
    for (qint32 k = first; k < nent; ++k)
    {
        FiffDirEntry ent = dir.at(k);
        if (ent.kind == FIFF_DATA_SKIP)
        {
            FiffTag::read_tag(p_pFile, t_pTag, ent.pos);
            nskip = *t_pTag->toInt();
        }
        else if(ent.kind == FIFF_DATA_BUFFER)
        {
            //
            //   Figure out the number of samples in this buffer
            //
            switch(ent.type)
            {
                case FIFFT_DAU_PACK16:
                    nsamp = ent.size/(2*nchan);
                    break;
                case FIFFT_SHORT:
                    nsamp = ent.size/(2*nchan);
                    break;
                case FIFFT_FLOAT:
                    nsamp = ent.size/(4*nchan);
                    break;
                case FIFFT_INT:
                    nsamp = ent.size/(4*nchan);
                    break;
                default:
                    printf("Cannot handle data buffers of type %d\n",ent.type);
                    return false;
            }
            //
            //  Do we have an initial skip pending?
            //
            if (first_skip > 0)
            {
                first_samp += nsamp*first_skip;
                data->first_samp = first_samp;
                first_skip = 0;
            }
            //
            //  Do we have a skip pending?
            //
            if (nskip > 0)
            {
                FiffRawDir t_RawDir;
                t_RawDir.first = first_samp;
                t_RawDir.last  = first_samp + nskip*nsamp - 1;//ToDo -1 right or is that MATLAB syntax
                t_RawDir.nsamp = nskip*nsamp;
                rawdir.append(t_RawDir);
                first_samp = first_samp + nskip*nsamp;
                nskip = 0;
                ++ndir;
            }
            //
            //  Add a data buffer
            //
            FiffRawDir t_RawDir;
            t_RawDir.ent   = ent;
            t_RawDir.first = first_samp;
            t_RawDir.last  = first_samp + nsamp - 1;//ToDo -1 right or is that MATLAB syntax
            t_RawDir.nsamp = nsamp;
            rawdir.append(t_RawDir);
            first_samp += nsamp;
            ++ndir;
        }
    }
    data->last_samp  = first_samp - 1;//ToDo -1 right or is that MATLAB syntax
    //
    //   Add the calibration factors
    //
    MatrixXf cals(1,data->info->nchan);
    cals.setZero();
    for (qint32 k = 0; k < data->info->nchan; ++k)
        cals(0,k) = data->info->chs.at(k).range*data->info->chs.at(k).cal;
    //
    data->cals       = cals;
    data->rawdir     = rawdir;
    //data->proj       = [];
    //data.comp       = [];
    //
    printf("\tRange : %d ... %d  =  %9.3f ... %9.3f secs\n",
           data->first_samp,data->last_samp,
           (double)data->first_samp/data->info->sfreq,
           (double)data->last_samp/data->info->sfreq);
    printf("Ready.\n");
    data->file->close();

    if(t_pTree)
        delete t_pTree;
    if(t_pDir)
        delete t_pDir;

    return true;
}


//*************************************************************************************************************

QStringList FiffFile::split_name_list(QString p_sNameList)
{
    return p_sNameList.split(":");
}

//*************************************************************************************************************

void FiffFile::start_block(fiff_int_t kind)
{
    this->write_int(FIFF_BLOCK_START,&kind);
}


//*************************************************************************************************************

FiffFile* FiffFile::start_file(QString& p_sFilename)
{
    FiffFile* p_pFile = new FiffFile(p_sFilename);

    if(!p_pFile->open(QIODevice::WriteOnly))
    {
        printf("Cannot write to %s\n", p_pFile->fileName().toUtf8().constData());//consider throw
        delete p_pFile;
        return NULL;
    }

    //
    //   Write the compulsory items
    //
    p_pFile->write_id(FIFF_FILE_ID);//1
    qint32 data = -1;
    p_pFile->write_int(FIFF_DIR_POINTER,&data);//2
    p_pFile->write_int(FIFF_FREE_LIST,&data);//3
    //
    //   Ready for more
    //
    return p_pFile;
}


//*************************************************************************************************************

FiffFile* FiffFile::start_writing_raw(QString& p_sFileName, FiffInfo* info, MatrixXf*& cals, MatrixXi sel)
{
    //
    //   We will always write floats
    //
    fiff_int_t data_type = 4;
    qint32 k;

    if(sel.cols() == 0)
    {
        sel.resize(1,info->nchan);
        for (k = 0; k < info->nchan; ++k)
            sel(0, k) = k; //+1 when MATLAB notation
    }

    QList<FiffChInfo> chs;

    for(k = 0; k < sel.cols(); ++k)
        chs << info->chs.at(sel(0,k));

    fiff_int_t nchan = chs.size();
    //
    //  Create the file and save the essentials
    //

    FiffFile* t_pFile = start_file(p_sFileName);//1, 2, 3
    t_pFile->start_block(FIFFB_MEAS);//4
    t_pFile->write_id(FIFF_BLOCK_ID);//5
    if(info->meas_id.version != -1)
    {
        t_pFile->write_id(FIFF_PARENT_BLOCK_ID,info->meas_id);//6
    }
    //
    //
    //    Measurement info
    //
    t_pFile->start_block(FIFFB_MEAS_INFO);//7
    //
    //    Blocks from the original
    //
    QList<fiff_int_t> blocks;
    blocks << FIFFB_SUBJECT << FIFFB_HPI_MEAS << FIFFB_HPI_RESULT << FIFFB_ISOTRAK << FIFFB_PROCESSING_HISTORY;
    bool have_hpi_result = false;
    bool have_isotrak    = false;
    if (blocks.size() > 0 && !info->filename.isEmpty())
    {
        FiffFile* t_pFile2 = new FiffFile(info->filename);

        FiffDirTree* t_pTree = NULL;
        QList<FiffDirEntry>* t_pDir = NULL;
        t_pFile2->open(t_pTree, t_pDir);

        for(qint32 k = 0; k < blocks.size(); ++k)
        {
            QList<FiffDirTree*> nodes = t_pTree->dir_tree_find(blocks.at(k));
            FiffDirTree::copy_tree(t_pFile2,t_pTree->id,nodes,t_pFile);
            if(blocks[k] == FIFFB_HPI_RESULT && nodes.size() > 0)
                have_hpi_result = true;

            if(blocks[k] == FIFFB_ISOTRAK && nodes.size() > 0)
                have_isotrak = true;
        }

        delete t_pDir;
        delete t_pTree;
        delete t_pFile2;
        t_pFile2 = NULL;
    }
    //
    //    megacq parameters
    //
    if (!info->acq_pars.isEmpty() || !info->acq_stim.isEmpty())
    {
        t_pFile->start_block(FIFFB_DACQ_PARS);
        if (!info->acq_pars.isEmpty())
            t_pFile->write_string(FIFF_DACQ_PARS, info->acq_pars);

        if (!info->acq_stim.isEmpty())
            t_pFile->write_string(FIFF_DACQ_STIM, info->acq_stim);

        t_pFile->end_block(FIFFB_DACQ_PARS);
    }
    //
    //    Coordinate transformations if the HPI result block was not there
    //
    if (!have_hpi_result)
    {
        if (info->dev_head_t != NULL)
            t_pFile->write_coord_trans(info->dev_head_t);

        if (info->ctf_head_t != NULL)
            t_pFile->write_coord_trans(info->ctf_head_t);
    }
    //
    //    Polhemus data
    //
    if (info->dig.size() > 0 && !have_isotrak)
    {
        t_pFile->start_block(FIFFB_ISOTRAK);
        for (qint32 k = 0; k < info->dig.size(); ++k)
            t_pFile->write_dig_point(info->dig[k]);

        t_pFile->end_block(FIFFB_ISOTRAK);
    }
    //
    //    Projectors
    //
    t_pFile->write_proj(info->projs);
    //
    //    CTF compensation info
    //
    t_pFile->write_ctf_comp(info->comps);
    //
    //    Bad channels
    //
    if (info->bads.size() > 0)
    {
        t_pFile->start_block(FIFFB_MNE_BAD_CHANNELS);
        t_pFile->write_name_list(FIFF_MNE_CH_NAME_LIST,info->bads);
        t_pFile->end_block(FIFFB_MNE_BAD_CHANNELS);
    }
    //
    //    General
    //
    t_pFile->write_float(FIFF_SFREQ,&info->sfreq);
    t_pFile->write_float(FIFF_HIGHPASS,&info->highpass);
    t_pFile->write_float(FIFF_LOWPASS,&info->lowpass);
    t_pFile->write_int(FIFF_NCHAN,&nchan);
    t_pFile->write_int(FIFF_DATA_PACK,&data_type);
    if (info->meas_date[0] != -1)
        t_pFile->write_int(FIFF_MEAS_DATE,info->meas_date, 2);
    //
    //    Channel info
    //
    if (cals)
        delete cals;
    cals = new MatrixXf(1,nchan);

    for(k = 0; k < nchan; ++k)
    {
        //
        //    Scan numbers may have been messed up
        //
        chs[k].scanno = k+1;//+1 because
        chs[k].range  = 1.0f;
        (*cals)(0,k) = chs[k].cal;
        t_pFile->write_ch_info(&chs[k]);
    }
    //
    //
    t_pFile->end_block(FIFFB_MEAS_INFO);
    //
    // Start the raw data
    //
    t_pFile->start_block(FIFFB_RAW_DATA);

    return t_pFile;
}


//*************************************************************************************************************

void FiffFile::write_ch_info(FiffChInfo* ch)
{
    //typedef struct _fiffChPosRec {
    //  fiff_int_t   coil_type;          /*!< What kind of coil. */
    //  fiff_float_t r0[3];              /*!< Coil coordinate system origin */
    //  fiff_float_t ex[3];              /*!< Coil coordinate system x-axis unit vector */
    //  fiff_float_t ey[3];              /*!< Coil coordinate system y-axis unit vector */
    //  fiff_float_t ez[3];             /*!< Coil coordinate system z-axis unit vector */
    //} fiffChPosRec,*fiffChPos;        /*!< Measurement channel position and coil type */


    //typedef struct _fiffChInfoRec {
    //  fiff_int_t    scanNo;        /*!< Scanning order # */
    //  fiff_int_t    logNo;         /*!< Logical channel # */
    //  fiff_int_t    kind;          /*!< Kind of channel */
    //  fiff_float_t  range;         /*!< Voltmeter range (only applies to raw data ) */
    //  fiff_float_t  cal;           /*!< Calibration from volts to... */
    //  fiff_ch_pos_t chpos;         /*!< Channel location */
    //  fiff_int_t    unit;          /*!< Unit of measurement */
    //  fiff_int_t    unit_mul;      /*!< Unit multiplier exponent */
    //  fiff_char_t   ch_name[16];   /*!< Descriptive name for the channel */
    //} fiffChInfoRec,*fiffChInfo;   /*!< Description of one channel */

    fiff_int_t datasize= 4*13 + 4*7 + 16;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)FIFF_CH_INFO;
    out << (qint32)FIFFT_CH_INFO_STRUCT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffChInfoRec
    //
    out << (qint32)ch->scanno;
    out << (qint32)ch->logno;
    out << (qint32)ch->kind;

    int iData = 0;
    iData = *(int *)&ch->range;
    out << iData;
    iData = *(int *)&ch->cal;
    out << iData;

    //
    //   fiffChPosRec follows
    //
    out << (qint32)ch->coil_type;
    qint32 i;
    for(i = 0; i < 12; ++i)
    {
        iData = *(int *)&ch->loc(i,0);
        out << iData;
    }

    //
    //   unit and unit multiplier
    //
    out << (qint32)ch->unit;
    out << (qint32)ch->unit_mul;

    //
    //   Finally channel name
    //
    fiff_int_t len = ch->ch_name.size();
    QString ch_name;
    if(len > 15)
    {
        ch_name = ch->ch_name.mid(0, 15);
    }
    else
        ch_name = ch->ch_name;

    len = ch_name.size();


    out.writeRawData(ch_name.toUtf8().constData(),len);

    if (len < 16)
    {
        const char* chNull = "";
        for(i = 0; i < 16-len; ++i)
            out.writeRawData(chNull,1);
    }
}


//*************************************************************************************************************

void FiffFile::write_coord_trans(FiffCoordTrans* trans)
{
    //?typedef struct _fiffCoordTransRec {
    //  fiff_int_t   from;                   /*!< Source coordinate system. */
    //  fiff_int_t   to;                     /*!< Destination coordinate system. */
    //  fiff_float_t rot[3][3];              /*!< The forward transform (rotation part) */
    //  fiff_float_t move[3];                /*!< The forward transform (translation part) */
    //  fiff_float_t invrot[3][3];           /*!< The inverse transform (rotation part) */
    //  fiff_float_t invmove[3];             /*!< The inverse transform (translation part) */
    //} *fiffCoordTrans, fiffCoordTransRec;  /*!< Coordinate transformation descriptor */

    fiff_int_t datasize = 4*2*12 + 4*2;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)FIFF_COORD_TRANS;
    out << (qint32)FIFFT_COORD_TRANS_STRUCT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffCoordTransRec
    //
    out << (qint32)trans->from;
    out << (qint32)trans->to;

    //
    //   The transform...
    //
    qint32 r, c;
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            out << (float)trans->trans(r,c);
    for (r = 0; r < 3; ++r)
        out << (float)trans->trans(r,3);

    //
    //   ...and its inverse
    //
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            out << (float)trans->invtrans(r,c);
    for (r = 0; r < 3; ++r)
        out << (float)trans->invtrans(r,3);
}


//*************************************************************************************************************

void FiffFile::write_ctf_comp(QList<FiffCtfComp*>& comps)
{
    if (comps.size() <= 0)
        return;
    //
    //  This is very simple in fact
    //
    this->start_block(FIFFB_MNE_CTF_COMP);
    for(qint32 k = 0; k < comps.size(); ++k)
    {
        FiffCtfComp* comp = new FiffCtfComp(comps[k]);
        this->start_block(FIFFB_MNE_CTF_COMP_DATA);
        //
        //    Write the compensation kind
        //
        this->write_int(FIFF_MNE_CTF_COMP_KIND, &comp->ctfkind);
        qint32 save_calibrated = comp->save_calibrated;
        this->write_int(FIFF_MNE_CTF_COMP_CALIBRATED, &save_calibrated);
        //
        //    Write an uncalibrated or calibrated matrix
        //
        *comp->data->data = (comp->rowcals.diagonal()).inverse()*(*comp->data->data)*(comp->colcals.diagonal()).inverse();
        this->write_named_matrix(FIFF_MNE_CTF_COMP_DATA,comp->data);
        this->end_block(FIFFB_MNE_CTF_COMP_DATA);

        delete comp;
    }
    this->end_block(FIFFB_MNE_CTF_COMP);

    return;
}


//*************************************************************************************************************

void FiffFile::write_dig_point(FiffDigPoint& dig)
{
    //?typedef struct _fiffDigPointRec {
    //  fiff_int_t kind;               /*!< FIFF_POINT_CARDINAL,
    //                                  *   FIFF_POINT_HPI, or
    //                                  *   FIFF_POINT_EEG */
    //  fiff_int_t ident;              /*!< Number identifying this point */
    //  fiff_float_t r[3];             /*!< Point location */
    //} *fiffDigPoint,fiffDigPointRec; /*!< Digitization point description */

    fiff_int_t datasize = 5*4;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)FIFF_DIG_POINT;
    out << (qint32)FIFFT_DIG_POINT_STRUCT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    //
    //   Start writing fiffDigPointRec
    //
    out << (qint32)dig.kind;
    out << (qint32)dig.ident;
    for(qint32 i = 0; i < 3; ++i)
        out << dig.r[i];
}


//*************************************************************************************************************

void FiffFile::write_float(fiff_int_t kind, float* data, fiff_int_t nel)
{
    qint32 datasize = nel * 4;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)kind;
    out << (qint32)FIFFT_FLOAT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    qint32 iData = 0;
    for(qint32 i = 0; i < nel; ++i)
    {
        iData = *(qint32 *)&data[i];
        out << iData;
    }
}


//*************************************************************************************************************

void FiffFile::write_float_matrix(fiff_int_t kind, const MatrixXf* mat)
{
    qint32 FIFFT_MATRIX = 1 << 30;
    qint32 FIFFT_MATRIX_FLOAT = FIFFT_FLOAT | FIFFT_MATRIX;

    qint32 numel = mat->rows()*mat->cols();

    fiff_int_t datasize = 4*numel + 4*3;


    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)kind;
    out << (qint32)FIFFT_MATRIX_FLOAT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    qint32 i;
    int iData = 0;

    for(i = 0; i < numel; ++i)
    {
        iData = *(int *)&mat->data()[i];
        out << iData;
    }

    qint32 dims[3];
    dims[0] = mat->cols();
    dims[1] = mat->rows();
    dims[2] = 2;

    for(i = 0; i < 3; ++i)
        out << dims[i];
}


//*************************************************************************************************************

void FiffFile::write_id(fiff_int_t kind, FiffId& id)
{
    if(id.version == -1)
    {
        /* initialize random seed: */
        srand ( time(NULL) );
        double rand_1 = (double)(rand() % 100);rand_1 /= 100;
        double rand_2 = (double)(rand() % 100);rand_2 /= 100;

        time_t seconds;
        seconds = time (NULL);

        //fiff_int_t timezone = 5;      //   Matlab does not know the timezone
        id.version   = (1 << 16) | 2;   //   Version (1 << 16) | 2
        id.machid[0] = 65536*rand_1;    //   Machine id is random for now
        id.machid[1] = 65536*rand_2;    //   Machine id is random for now
        id.time.secs = (int)seconds;    //seconds since January 1, 1970 //3600*(24*(now-datenum(1970,1,1,0,0,0))+timezone);
        id.time.usecs = 0;              //   Do not know how we could get this
    }

    //
    //
    fiff_int_t datasize = 5*4;                       //   The id comprises five integers

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)kind;
    out << (qint32)FIFFT_ID_STRUCT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;
    //
    // Collect the bits together for one write
    //
    qint32 data[5];
    data[0] = id.version;
    data[1] = id.machid[0];
    data[2] = id.machid[1];
    data[3] = id.time.secs;
    data[4] = id.time.usecs;

    for(qint32 i = 0; i < 5; ++i)
        out << data[i];
}


//*************************************************************************************************************

void FiffFile::write_int(fiff_int_t kind, fiff_int_t* data, fiff_int_t nel)
{
    fiff_int_t datasize = nel * 4;

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)kind;
    out << (qint32)FIFFT_INT;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    for(qint32 i = 0; i < nel; ++i)
        out << data[i];
}


//*************************************************************************************************************

void FiffFile::write_name_list(fiff_int_t kind,QStringList& data)
{
    QString all = data.join(":");
    this->write_string(kind,all);
}


//*************************************************************************************************************

void FiffFile::write_named_matrix(fiff_int_t kind,FiffNamedMatrix* mat)
{
    this->start_block(FIFFB_MNE_NAMED_MATRIX);
    this->write_int(FIFF_MNE_NROW, &mat->nrow);
    this->write_int(FIFF_MNE_NCOL, &mat->ncol);
    if (mat->row_names.size() > 0)
       this->write_name_list(FIFF_MNE_ROW_NAMES,mat->row_names);
    if (mat->col_names.size() > 0)
       this->write_name_list(FIFF_MNE_COL_NAMES,mat->col_names);
    this->write_float_matrix(kind,mat->data);
    this->end_block(FIFFB_MNE_NAMED_MATRIX);
}


//*************************************************************************************************************

void FiffFile::write_proj(QList<FiffProj*>& projs)
{
    if (projs.size() <= 0)
        return;

    this->start_block(FIFFB_PROJ);

    for(qint32 k = 0; k < projs.size(); ++k)
    {
        this->start_block(FIFFB_PROJ_ITEM);
        this->write_string(FIFF_NAME,projs[k]->desc);
        this->write_int(FIFF_PROJ_ITEM_KIND,&projs[k]->kind);
        if (projs[k]->kind == FIFFV_PROJ_ITEM_FIELD)
        {
            float fValue = 0.0f;
            this->write_float(FIFF_PROJ_ITEM_TIME, &fValue);
        }

        this->write_int(FIFF_NCHAN, &projs[k]->data->ncol);
        this->write_int(FIFF_PROJ_ITEM_NVEC, &projs[k]->data->nrow);
        qint32 bValue = (qint32)projs[k]->active;
        this->write_int(FIFF_MNE_PROJ_ITEM_ACTIVE, &bValue);
        this->write_name_list(FIFF_PROJ_ITEM_CH_NAME_LIST, projs[k]->data->col_names);
        this->write_float_matrix(FIFF_PROJ_ITEM_VECTORS, projs[k]->data->data);
        this->end_block(FIFFB_PROJ_ITEM);
    }
    this->end_block(FIFFB_PROJ);
}


//*************************************************************************************************************

bool FiffFile::write_raw_buffer(MatrixXf* buf, MatrixXf* cals)
{
    if (buf->rows() != cals->cols())
    {
        printf("buffer and calibration sizes do not match\n");
        return false;
    }

    SparseMatrix<float> inv_calsMat(cals->cols(), cals->cols());

    for(qint32 i = 0; i < cals->cols(); ++i)
        inv_calsMat.insert(i, i) = 1.0/(*cals)(0,i);

    MatrixXf tmp = inv_calsMat*(*buf);
    this->write_float(FIFF_DATA_BUFFER,tmp.data(),tmp.rows()*tmp.cols());
    return true;
}


//*************************************************************************************************************

void FiffFile::write_string(fiff_int_t kind, QString& data)
{
    fiff_int_t datasize = data.size();//size(data,2);

    QDataStream out(this);   // we will serialize the data into the file
    out.setByteOrder(QDataStream::BigEndian);

    out << (qint32)kind;
    out << (qint32)FIFFT_STRING;
    out << (qint32)datasize;
    out << (qint32)FIFFV_NEXT_SEQ;

    out.writeRawData(data.toUtf8().constData(),datasize);
}
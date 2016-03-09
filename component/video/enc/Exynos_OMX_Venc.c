/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @file        Exynos_OMX_Venc.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Yunji Kim (yunji.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"
#include "ExynosVideoApi.h"
#include "csc.h"

#ifdef USE_METADATABUFFERTYPE
#include "Exynos_OSAL_Android.h"
#endif

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_ENC"
#define EXYNOS_LOG_OFF
//#define EXYNOS_TRACE_ON
#include "Exynos_OSAL_Log.h"

void Exynos_UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    exynosInputPort->portDefinition.nBufferSize = ALIGN(exynosInputPort->portDefinition.format.video.nFrameWidth, 16) *
                                                      exynosInputPort->portDefinition.format.video.nFrameHeight * 3 / 2;

    if ((exynosOutputPort->portDefinition.format.video.nFrameWidth !=
            exynosInputPort->portDefinition.format.video.nFrameWidth) ||
        (exynosOutputPort->portDefinition.format.video.nFrameHeight !=
            exynosInputPort->portDefinition.format.video.nFrameHeight)) {
        OMX_U32 width = 0, height = 0;

        exynosOutputPort->portDefinition.format.video.nFrameWidth =
            exynosInputPort->portDefinition.format.video.nFrameWidth;
        exynosOutputPort->portDefinition.format.video.nFrameHeight =
            exynosInputPort->portDefinition.format.video.nFrameHeight;
        width = exynosOutputPort->portDefinition.format.video.nStride =
            exynosInputPort->portDefinition.format.video.nStride;
        height = exynosOutputPort->portDefinition.format.video.nSliceHeight =
            exynosInputPort->portDefinition.format.video.nSliceHeight;

        if (width && height)
            exynosOutputPort->portDefinition.nBufferSize = (width * height * 3) / 2;
    }

    return;
}

void Exynos_Input_SetSupportFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret        = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc  = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    if ((pVideoEnc == NULL) || (pInputPort == NULL))
        return ;

    if (pInputPort->supportFormat != NULL) {
        OMX_BOOL ret = OMX_FALSE;
        int nLastIndex = INPUT_PORT_SUPPORTFORMAT_DEFAULT_NUM;
        int i;

        /* default supported formats */
        pInputPort->supportFormat[0] = OMX_COLOR_FormatYUV420Planar;
        pInputPort->supportFormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;
        pInputPort->supportFormat[2] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;
        pInputPort->supportFormat[3] = OMX_COLOR_Format32bitARGB8888;
        pInputPort->supportFormat[4] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format32bitABGR8888;
#ifdef USE_ANDROIDOPAQUE
        pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_FormatAndroidOpaque;
#endif

        /* add extra formats, if It is supported by H/W. (CSC doesn't exist) */
        /* OMX_SEC_COLOR_FormatNV12Tiled */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;

        /* OMX_SEC_COLOR_FormatYVU420Planar */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent,
                                            (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;

        /* OMX_COLOR_Format32bitBGRA8888 */
        ret = pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, OMX_COLOR_Format32bitBGRA8888);
        if (ret == OMX_TRUE)
            pInputPort->supportFormat[nLastIndex++] = OMX_COLOR_Format32bitBGRA8888;

        for (i = 0; i < nLastIndex; i++)
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Support Format[%d] : 0x%x", i, pInputPort->supportFormat[i]);
    }

    return ;
}

OMX_COLOR_FORMATTYPE Exynos_Input_GetActualColorFormat(EXYNOS_OMX_BASECOMPONENT *pExynosComponent)
{
    OMX_COLOR_FORMATTYPE             ret                = OMX_COLOR_FormatUnused;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    OMX_COLOR_FORMATTYPE             eColorFormat       = pInputPort->portDefinition.format.video.eColorFormat;

#ifdef USE_ANDROIDOPAQUE
    if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)
        eColorFormat = pVideoEnc->ANBColorFormat;
#endif

    if (pVideoEnc->exynos_codec_checkFormatSupport(pExynosComponent, eColorFormat) == OMX_TRUE) {
        ret = eColorFormat;
        goto EXIT;
    }

    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV420Planar:       /* converted to NV12 using CSC */
    case OMX_COLOR_Format32bitARGB8888:      /* converted to NV12 using CSC */
    case OMX_SEC_COLOR_Format32bitABGR8888:  /* converted to NV12 using CSC */
        ret = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatNV12Tiled:
        ret = eColorFormat;
        break;
    default:
        ret = OMX_COLOR_FormatUnused;
        break;
    }

EXIT:
    return ret;
}

void Exynos_Free_CodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    CODEC_ENC_BUFFER               **ppCodecBuffer      = NULL;

    int nBufferCnt = 0, nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoEnc->pMFCEncInputBuffer[0]);
        nBufferCnt = MFC_INPUT_BUFFER_NUM_MAX;
    } else {
        ppCodecBuffer = &(pVideoEnc->pMFCEncOutputBuffer[0]);
        nBufferCnt = MFC_OUTPUT_BUFFER_NUM_MAX;
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);
    for (i = 0; i < nBufferCnt; i++) {
        if (ppCodecBuffer[i] != NULL) {
            for (j = 0; j < nPlaneCnt; j++) {
                if (ppCodecBuffer[i]->pVirAddr[j] != NULL)
                    Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
            }

            Exynos_OSAL_Free(ppCodecBuffer[i]);
            ppCodecBuffer[i] = NULL;
        }
    }

    FunctionOut();
}

OMX_ERRORTYPE Exynos_Allocate_CodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex,
    int                  nBufferCnt,
    OMX_U32              nPlaneSize[MAX_BUFFER_PLANE])
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    MEMORY_TYPE                      eMemoryType        = CACHED_MEMORY;
    CODEC_ENC_BUFFER               **ppCodecBuffer      = NULL;

    int nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer = &(pVideoEnc->pMFCEncInputBuffer[0]);
    } else {
        ppCodecBuffer = &(pVideoEnc->pMFCEncOutputBuffer[0]);
#ifdef USE_CSC_HW
        eMemoryType = NORMAL_MEMORY;
#endif
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);

    if (pVideoEnc->bDRMPlayerMode == OMX_TRUE)
        eMemoryType = SECURE_MEMORY;

    for (i = 0; i < nBufferCnt; i++) {
        ppCodecBuffer[i] = (CODEC_ENC_BUFFER *)Exynos_OSAL_Malloc(sizeof(CODEC_ENC_BUFFER));
        if (ppCodecBuffer[i] == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Alloc codec buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        Exynos_OSAL_Memset(ppCodecBuffer[i], 0, sizeof(CODEC_ENC_BUFFER));

        for (j = 0; j < nPlaneCnt; j++) {
            ppCodecBuffer[i]->pVirAddr[j] =
                (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nPlaneSize[j], eMemoryType);
            if (ppCodecBuffer[i]->pVirAddr[j] == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Alloc plane");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            ppCodecBuffer[i]->fd[j] =
                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, ppCodecBuffer[i]->pVirAddr[j]);
            ppCodecBuffer[i]->bufferSize[j] = nPlaneSize[j];
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "PORT[%d]: pMFCCodecBuffer[%d]->pVirAddr[%d]: 0x%x", nPortIndex, i, j, ppCodecBuffer[i]->pVirAddr[j]);
        }

        ppCodecBuffer[i]->dataSize = 0;
    }

    return OMX_ErrorNone;

EXIT:
    Exynos_Free_CodecBuffers(pOMXComponent, nPortIndex);

    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Check_BufferProcess_State(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nPortIndex)
{
    OMX_BOOL ret = OMX_FALSE;

    if ((pExynosComponent->currentState == OMX_StateExecuting) &&
        (pExynosComponent->pExynosPort[nPortIndex].portState == OMX_StateIdle) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateIdleToExecuting)) {
        ret = OMX_TRUE;
    } else {
        ret = OMX_FALSE;
    }

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferToData(
    CODEC_ENC_BUFFER    *pCodecBuffer,
    EXYNOS_OMX_DATA     *pData,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int i;

    if (nPortIndex > OUTPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    pData->allocSize     = 0;
    pData->usedDataLen   = 0;
    pData->nFlags        = 0;
    pData->timeStamp     = 0;
    pData->pPrivate      = pCodecBuffer;
    pData->bufferHeader  = NULL;

    for (i = 0; i < MAX_BUFFER_PLANE; i++) {
        pData->multiPlaneBuffer.dataBuffer[i] = pCodecBuffer->pVirAddr[i];
        pData->multiPlaneBuffer.fd[i] = pCodecBuffer->fd[i];
        pData->allocSize += pCodecBuffer->bufferSize[i];
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        pData->dataLen       = pCodecBuffer->dataSize;
        pData->remainDataLen = pCodecBuffer->dataSize;
    } else {
        pData->dataLen       = 0;
        pData->remainDataLen = 0;
    }

EXIT:
    return ret;
}

void Exynos_Wait_ProcessPause(EXYNOS_OMX_BASECOMPONENT *pExynosComponent, OMX_U32 nPortIndex)
{
    EXYNOS_OMX_BASEPORT *exynosOMXInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT *exynosOMXOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT *exynosOMXPort = NULL;

    FunctionIn();

    exynosOMXPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (((pExynosComponent->currentState == OMX_StatePause) ||
        (pExynosComponent->currentState == OMX_StateIdle) ||
        (pExynosComponent->transientState == EXYNOS_OMX_TransStateLoadedToIdle) ||
        (pExynosComponent->transientState == EXYNOS_OMX_TransStateExecutingToIdle)) &&
        (pExynosComponent->transientState != EXYNOS_OMX_TransStateIdleToLoaded) &&
        (!CHECK_PORT_BEING_FLUSHED(exynosOMXPort))) {
        Exynos_OSAL_SignalWait(pExynosComponent->pExynosPort[nPortIndex].pauseEvent, DEF_MAX_WAIT_TIME);
        Exynos_OSAL_SignalReset(pExynosComponent->pExynosPort[nPortIndex].pauseEvent);
    }

    FunctionOut();

    return;
}

OMX_BOOL Exynos_CSC_InputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *srcInputData)
{
    OMX_BOOL                       ret              = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc        = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *exynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER         *inputUseBuffer   = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    CODEC_ENC_BUFFER              *codecInputBuffer = (CODEC_ENC_BUFFER *)srcInputData->pPrivate;
    OMX_COLOR_FORMATTYPE           eColorFormat     = exynosInputPort->portDefinition.format.video.eColorFormat;
    OMX_COLOR_FORMATTYPE           inputColorFormat = OMX_COLOR_FormatUnused;

    FunctionIn();

    void *pInputBuf                 = (void *)inputUseBuffer->bufferHeader->pBuffer;
    void *pSrcBuf[MAX_BUFFER_PLANE] = {NULL, };
    void *pDstBuf[MAX_BUFFER_PLANE] = {NULL, };

    ExynosVideoPlane planes[MAX_BUFFER_PLANE];
    OMX_U32          nDataSize[MAX_BUFFER_PLANE]  = {0, 0, 0};
    OMX_U32          nAllocSize[MAX_BUFFER_PLANE] = {0, 0, 0};
    OMX_PTR          ppBuf[MAX_BUFFER_PLANE]      = {NULL, NULL, NULL};

    OMX_U32 nFrameWidth = 0, nFrameHeight = 0;
    OMX_U32 nImageWidth = 0, nImageHeight = 0, stride = 0;
    int i, nPlaneCnt;

    CSC_ERRORCODE   cscRet      = CSC_ErrorNone;
    CSC_METHOD      csc_method  = CSC_METHOD_SW;
    CSC_MEMTYPE     csc_memType = CSC_MEMORY_USERPTR;
    unsigned int srcCacheable = 1, dstCacheable = 1;
    unsigned int csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_SINGLE);
    unsigned int csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_MULTIPLE);

    nFrameWidth  = ALIGN(exynosInputPort->portDefinition.format.video.nFrameWidth, 16);
    nImageWidth  = exynosInputPort->portDefinition.format.video.nFrameWidth;
    nFrameHeight = nImageHeight = exynosInputPort->portDefinition.format.video.nFrameHeight;

    pDstBuf[0] = srcInputData->multiPlaneBuffer.dataBuffer[0];
    pDstBuf[1] = srcInputData->multiPlaneBuffer.dataBuffer[1];
    pDstBuf[2] = srcInputData->multiPlaneBuffer.dataBuffer[2];

    csc_get_method(pVideoEnc->csc_handle, &csc_method);

    inputColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
    Exynos_OSAL_GetPlaneSize(inputColorFormat, nImageWidth, nImageHeight, nDataSize, nAllocSize);
    codecInputBuffer->dataSize = 0;
    nPlaneCnt = Exynos_GetPlaneFromPort(exynosInputPort);
    for (i = 0; i < nPlaneCnt; i++)
        codecInputBuffer->dataSize += nDataSize[i];

#ifdef USE_METADATABUFFERTYPE
    if (exynosInputPort->bStoreMetaData == OMX_TRUE) {
        /* ARGB8888 converted to YUV420SemiPlanar */
        csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_Format32bitARGB8888, PLANE_SINGLE);
        csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_MULTIPLE);

        if (OMX_ErrorNone != Exynos_OSAL_GetInfoFromMetaData((OMX_BYTE)inputUseBuffer->bufferHeader->pBuffer, ppBuf)) {
            ret = OMX_FALSE;
            goto EXIT;
        }

#ifdef USE_ANDROIDOPAQUE
        if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque) {  /* portdef format */
            /* kMetadataBufferTypeGrallocSource */
            csc_src_color_format  = Exynos_OSAL_OMX2HALPixelFormat(pVideoEnc->ANBColorFormat, PLANE_SINGLE);
            if (OMX_ErrorNone != Exynos_OSAL_LockANBHandle((OMX_U32)(uintptr_t)ppBuf[0], nImageWidth, nImageHeight, OMX_COLOR_FormatAndroidOpaque, &stride, planes)) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: Exynos_OSAL_LockANBHandle() failed", __FUNCTION__);
                ret = OMX_FALSE;
                goto EXIT;
            }

            if (pVideoEnc->csc_set_format == OMX_FALSE) {
#ifdef USE_GSC_RGB_ENCODER
                if (csc_method != CSC_METHOD_HW)
#else
                if ((csc_method != CSC_METHOD_HW) &&
                    (pVideoEnc->ANBColorFormat != (OMX_COLOR_FORMATTYPE)OMX_COLOR_Format32bitARGB8888))
#endif
                {
                    cscRet = csc_set_method(pVideoEnc->csc_handle, CSC_METHOD_HW);
                    if (cscRet != CSC_ErrorNone) {
                        ret = OMX_FALSE;
                        goto EXIT;
                    }
                    csc_method = CSC_METHOD_HW;
                }
            }

            pSrcBuf[0] = (void *)planes[0].addr;
            pSrcBuf[1] = NULL;
            pSrcBuf[2] = NULL;

#ifdef USE_DMA_BUF
            if (csc_method == CSC_METHOD_HW) {
                srcCacheable = 0;
                csc_memType  = CSC_MEMORY_DMABUF;
                pSrcBuf[0]   = (void *)(intptr_t)planes[0].fd;
                pSrcBuf[1]   = NULL;
                pSrcBuf[2]   = NULL;
            }
#endif
        }
#endif  // USE_ANDROIDOPAQUE
    } else
#endif  // USE_USE_METADATABUFFERTYPE
    {
        Exynos_OSAL_GetPlaneSize(eColorFormat, nFrameWidth, nFrameHeight, nDataSize, nAllocSize);
        pSrcBuf[0]  = (void *)((char *)pInputBuf);
        pSrcBuf[1]  = (void *)((char *)pInputBuf + nDataSize[0]);
        pSrcBuf[2]  = (void *)((char *)pInputBuf + nDataSize[0] + nDataSize[1]);

        if (eColorFormat == inputColorFormat) {
            switch ((int)eColorFormat) {
            case OMX_COLOR_FormatYUV420SemiPlanar:
            case OMX_SEC_COLOR_FormatNV12Tiled:
            case OMX_SEC_COLOR_FormatNV21Linear:
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[0], pSrcBuf[0], nDataSize[0]);
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[1], pSrcBuf[1], nDataSize[1]);
                ret = OMX_TRUE;
                break;
            case OMX_COLOR_FormatYUV420Planar:
            case OMX_SEC_COLOR_FormatYVU420Planar:
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[0], pSrcBuf[0], nDataSize[0]);
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[1], pSrcBuf[1], nDataSize[1]);
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[2], pSrcBuf[2], nDataSize[2]);
                ret = OMX_TRUE;
                break;
            case OMX_COLOR_Format32bitBGRA8888:
            case OMX_SEC_COLOR_Format32bitABGR8888:
            case OMX_COLOR_Format32bitARGB8888:
                Exynos_OSAL_Memcpy(srcInputData->multiPlaneBuffer.dataBuffer[0], (char *)pInputBuf, codecInputBuffer->dataSize);
                ret = OMX_TRUE;
                break;
            default:
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unspported color format : %x. can't support CSC", __FUNCTION__, eColorFormat);
                codecInputBuffer->dataSize = 0;
                ret = OMX_FALSE;
                break;
            }
            goto EXIT;
        } else {
            switch ((int)eColorFormat) {  /* portdef format */
            case OMX_COLOR_FormatYUV420Planar:
                /* YUV420Planar converted to YUV420Semiplanar (interleaved UV plane) as per MFC spec.*/
                csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420Planar, PLANE_SINGLE);
                csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_MULTIPLE);
                break;
            case OMX_COLOR_FormatYUV420SemiPlanar:
            case OMX_SEC_COLOR_FormatNV12Tiled:
            case OMX_SEC_COLOR_FormatNV21Linear:
                /* Just copied to MFC input buffer */
                csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_SINGLE);
                csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_MULTIPLE);
                break;
            case OMX_COLOR_Format32bitARGB8888:
                csc_src_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_Format32bitARGB8888, PLANE_SINGLE);
                csc_dst_color_format = Exynos_OSAL_OMX2HALPixelFormat((unsigned int)OMX_COLOR_FormatYUV420SemiPlanar, PLANE_MULTIPLE);
                break;
            default:
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unspported color format : %x. can't support CSC", __FUNCTION__, eColorFormat);
                codecInputBuffer->dataSize = 0;
                ret = OMX_FALSE;
                goto EXIT;
                break;
            }

#ifdef USE_DMA_BUF
            if (csc_method == CSC_METHOD_HW) {
                srcCacheable = 0;
                csc_memType  = CSC_MEMORY_DMABUF;
                pSrcBuf[0]   = (void *)(intptr_t)Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, (char *)pInputBuf);
                pSrcBuf[1]   = NULL;
                pSrcBuf[2]   = NULL;
            }
#endif
        }
    }

#ifdef USE_DMA_BUF
    if (csc_method == CSC_METHOD_HW) {
        dstCacheable = 0;
        csc_memType = CSC_MEMORY_DMABUF;
        pDstBuf[0] = (void *)(intptr_t)srcInputData->multiPlaneBuffer.fd[0];
        pDstBuf[1] = (void *)(intptr_t)srcInputData->multiPlaneBuffer.fd[1];
        pDstBuf[2] = (void *)(intptr_t)srcInputData->multiPlaneBuffer.fd[2];
    }
#endif

    if (pVideoEnc->csc_set_format == OMX_FALSE) {
        csc_set_src_format(
            pVideoEnc->csc_handle,  /* handle */
            nFrameWidth,            /* width */
            nFrameHeight,           /* height */
            0,                      /* crop_left */
            0,                      /* crop_right */
            nImageWidth,            /* crop_width */
            nImageHeight,           /* crop_height */
            csc_src_color_format,   /* color_format */
            srcCacheable);          /* cacheable */

        csc_set_dst_format(
            pVideoEnc->csc_handle,  /* handle */
            nFrameWidth,            /* width */
            nFrameHeight,           /* height */
            0,                      /* crop_left */
            0,                      /* crop_right */
            nImageWidth,            /* crop_width */
            nImageHeight,           /* crop_height */
            csc_dst_color_format,   /* color_format */
            dstCacheable);          /* cacheable */

        csc_set_eq_property(
            pVideoEnc->csc_handle,          /* handle */
            CSC_EQ_MODE_USER,               /* user select */
            CSC_EQ_RANGE_NARROW,            /* narrow */
            CSC_EQ_COLORSPACE_SMPTE170M);   /* bt.601 */

        pVideoEnc->csc_set_format = OMX_TRUE;
    }

    csc_set_src_buffer(
        pVideoEnc->csc_handle,  /* handle */
        pSrcBuf,
        csc_memType);           /* YUV Addr or FD */
    csc_set_dst_buffer(
        pVideoEnc->csc_handle,  /* handle */
        pDstBuf,
        csc_memType);           /* YUV Addr or FD */
    cscRet = csc_convert(pVideoEnc->csc_handle);
    if (cscRet != CSC_ErrorNone)
        ret = OMX_FALSE;
    else
        ret = OMX_TRUE;

#ifdef USE_METADATABUFFERTYPE
    if (exynosInputPort->bStoreMetaData == OMX_TRUE) {
#ifdef USE_ANDROIDOPAQUE
        Exynos_OSAL_UnlockANBHandle((OMX_U32)(uintptr_t)ppBuf[0]);
#endif
    }
#endif

EXIT:
    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Preprocessor_InputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *srcInputData)
{
    OMX_BOOL                         ret                = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *inputUseBuffer     = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    OMX_U32                          nFrameWidth        = exynosInputPort->portDefinition.format.video.nFrameWidth;
    OMX_U32                          nFrameHeight       = exynosInputPort->portDefinition.format.video.nFrameHeight;
    OMX_COLOR_FORMATTYPE             eColorFormat       = exynosInputPort->portDefinition.format.video.eColorFormat;

    OMX_U32  copySize = 0;
    OMX_BYTE checkInputStream = NULL;
    OMX_U32  checkInputStreamLen = 0;
    OMX_BOOL flagEOS = OMX_FALSE;

    FunctionIn();

    if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
        if ((srcInputData->multiPlaneBuffer.dataBuffer[0] == NULL) ||
            (srcInputData->pPrivate == NULL)) {
            ret = OMX_FALSE;
            goto EXIT;
        }
    }

    if (inputUseBuffer->dataValid == OMX_TRUE) {
        if (exynosInputPort->bufferProcessType & BUFFER_SHARE) {
            Exynos_Shared_BufferToData(inputUseBuffer, srcInputData, ONE_PLANE);
#ifdef USE_METADATABUFFERTYPE
            if (exynosInputPort->bStoreMetaData == OMX_TRUE) {
                OMX_COLOR_FORMATTYPE inputColorFormat = OMX_COLOR_FormatUnused;

                OMX_PTR ppBuf[MAX_BUFFER_PLANE]     = {NULL, NULL, NULL};
                OMX_U32 nDataSize[MAX_BUFFER_PLANE] = {0, 0, 0};
                OMX_U32 nAllocSize[MAX_BUFFER_PLANE] = {0, 0, 0};
                int     plane = 0, nPlaneCnt;

                inputColorFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
                Exynos_OSAL_GetPlaneSize(inputColorFormat, nFrameWidth, nFrameHeight, nDataSize, nAllocSize);
                nPlaneCnt = Exynos_GetPlaneFromPort(exynosInputPort);
                if (pVideoEnc->nInbufSpareSize > 0) {
                    for (plane = 0; plane < nPlaneCnt; plane++)
                        nAllocSize[plane] += pVideoEnc->nInbufSpareSize;
                }

                for (plane = 0; plane < MAX_BUFFER_PLANE; plane++) {
                    srcInputData->multiPlaneBuffer.fd[plane] = -1;
                    srcInputData->multiPlaneBuffer.dataBuffer[plane] = NULL;
                }

                if (inputUseBuffer->dataLen <= 0) {
                    if (!(inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
                        Exynos_InputBufferReturn(pOMXComponent, inputUseBuffer);

                        /* reset dataBuffer */
                        Exynos_ResetDataBuffer(inputUseBuffer);
                    } else {
                        /* Make EOS Buffer for MFC Processing scheme */
                        /* Use ION Allocator */
                        for (plane = 0; plane < nPlaneCnt; plane++) {
                            srcInputData->multiPlaneBuffer.dataBuffer[plane] =
                                (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nAllocSize[plane], NORMAL_MEMORY);
                            srcInputData->multiPlaneBuffer.fd[plane] =
                                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, srcInputData->multiPlaneBuffer.dataBuffer[plane]);
                        }
                    }
                } else {
                    Exynos_OSAL_GetInfoFromMetaData((OMX_BYTE)inputUseBuffer->bufferHeader->pBuffer, ppBuf);
#ifdef USE_DMA_BUF
#ifdef USE_ANDROIDOPAQUE
                    if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque) {
                        ExynosVideoPlane planes[MAX_BUFFER_PLANE];
                        OMX_U32 stride = 0;

                        Exynos_OSAL_LockANBHandle((OMX_U32)(uintptr_t)ppBuf[0], nFrameWidth, nFrameHeight, OMX_COLOR_FormatAndroidOpaque, &stride, planes);

                        if (stride == ALIGN(exynosInputPort->portDefinition.format.video.nFrameWidth, 16))
                            exynosInputPort->portDefinition.format.video.nStride = stride;

                        for (plane = 0; plane < nPlaneCnt; plane++) {
                            srcInputData->multiPlaneBuffer.fd[plane]         = planes[plane].fd;
                            srcInputData->multiPlaneBuffer.dataBuffer[plane] = planes[plane].addr;
                        }
                    } else
#endif
                    {
                        /* kMetadataBufferTypeCameraSource */
                        for (plane = 0; plane < nPlaneCnt; plane++) {
                            srcInputData->multiPlaneBuffer.fd[plane] = (int)(intptr_t)ppBuf[plane];
                        }
                    }

                    for (plane = 0; plane < nPlaneCnt; plane++) {
                        if ((srcInputData->multiPlaneBuffer.fd[plane] != -1) &&
                            (srcInputData->multiPlaneBuffer.dataBuffer[plane] == NULL)) {
                            srcInputData->multiPlaneBuffer.dataBuffer[plane] =
                                Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, srcInputData->multiPlaneBuffer.fd[plane]);
                            if(srcInputData->multiPlaneBuffer.dataBuffer[plane] == NULL) {
                                srcInputData->multiPlaneBuffer.dataBuffer[plane] =
                                    Exynos_OSAL_SharedMemory_Map(pVideoEnc->hSharedMemory, nAllocSize[plane], srcInputData->multiPlaneBuffer.fd[plane]);
                            }
                        }
                    }
#else
#ifdef USE_ANDROIDOPAQUE
                    if (eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_COLOR_FormatAndroidOpaque share don't implemented in UserPtr mode.");
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventError, OMX_ErrorNotImplemented, 0, NULL);
                        ret = OMX_FALSE;
                        goto EXIT;
                    } else
#endif
                    {
                        /* kMetadataBufferTypeCameraSource */
                        for (plane = 0; plane < MAX_BUFFER_PLANE; plane++) {
                            srcInputData->multiPlaneBuffer.dataBuffer[plane] = (int)ppBuf[plane];
                        }
                    }
#endif
                    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "%s:%d YAddr: 0x%zx CbCrAddr: 0x%zx", __FUNCTION__, __LINE__, ppBuf[0], ppBuf[0]);
                }
            }
#endif
            /* reset dataBuffer */
            Exynos_ResetDataBuffer(inputUseBuffer);
        } else if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
            checkInputStream = inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
            checkInputStreamLen = inputUseBuffer->remainDataLen;

            pExynosComponent->bUseFlagEOF = OMX_TRUE;

            if (checkInputStreamLen == 0) {
                inputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                flagEOS = OMX_TRUE;
            }

            copySize = checkInputStreamLen;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "exynos_checkInputFrame : OMX_TRUE");

            if (((srcInputData->allocSize) - (srcInputData->dataLen)) >= copySize) {
                if ((copySize > 0) || (inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
                    ret = OMX_TRUE;
                    if (copySize > 0)
                        ret = Exynos_CSC_InputData(pOMXComponent, srcInputData);
                    if (ret) {
                        inputUseBuffer->dataLen -= copySize;
                        inputUseBuffer->remainDataLen -= copySize;
                        inputUseBuffer->usedDataLen += copySize;

                        srcInputData->dataLen += copySize;
                        srcInputData->remainDataLen += copySize;

                        srcInputData->timeStamp = inputUseBuffer->timeStamp;
                        srcInputData->nFlags = inputUseBuffer->nFlags;
                        srcInputData->bufferHeader = inputUseBuffer->bufferHeader;
                    } else {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Exynos_CSC_InputData() failure");
                        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                                pExynosComponent->callbackData,
                                                                OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                    }
                } else {
                    ret = OMX_FALSE;
                }
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "input codec buffer is smaller than decoded input data size Out Length");
                pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                        pExynosComponent->callbackData,
                                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                ret = OMX_FALSE;
            }

#ifdef USE_METADATABUFFERTYPE
#ifdef USE_ANDROIDOPAQUE
            if (((exynosInputPort->bStoreMetaData == OMX_TRUE) && (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)) ||
                (exynosInputPort->bStoreMetaData == OMX_FALSE))
#else
            if (exynosInputPort->bStoreMetaData == OMX_FALSE)
#endif
            {
                Exynos_InputBufferReturn(pOMXComponent, inputUseBuffer);
            } else {
                inputUseBuffer->dataValid = OMX_TRUE;
            }
#else
            Exynos_InputBufferReturn(pOMXComponent, inputUseBuffer);
#endif
        }

        if ((srcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "bSaveFlagEOS : OMX_TRUE");
            pExynosComponent->bSaveFlagEOS = OMX_TRUE;
            if (srcInputData->dataLen != 0)
                pExynosComponent->bBehaviorEOS = OMX_TRUE;
        }

        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) &&
            (srcInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG) != OMX_BUFFERFLAG_CODECCONFIG) {
            pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pExynosComponent->checkTimeStamp.startTimeStamp = srcInputData->timeStamp;
            pExynosComponent->checkTimeStamp.nStartFlags = srcInputData->nFlags;
            pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "first frame timestamp after seeking %lld us (%.2f secs)",
                            srcInputData->timeStamp, srcInputData->timeStamp / 1E6);
        }

        ret = OMX_TRUE;
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_BOOL Exynos_Postprocess_OutputData(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *dstOutputData)
{
    OMX_BOOL                  ret = OMX_FALSE;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *outputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    OMX_U32                   copySize = 0;

    FunctionIn();

    if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if (Exynos_Shared_DataToBuffer(dstOutputData, outputUseBuffer, OMX_FALSE) == OMX_ErrorNone)
            outputUseBuffer->dataValid = OMX_TRUE;
    }

    if (outputUseBuffer->dataValid == OMX_TRUE) {
        if (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE) {
            if (pExynosComponent->checkTimeStamp.startTimeStamp == dstOutputData->timeStamp){
                pExynosComponent->checkTimeStamp.startTimeStamp = -19761123;
                pExynosComponent->checkTimeStamp.nStartFlags = 0x0;
                pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
                pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "garbage frame drop after flush");
                ret = OMX_TRUE;
                goto EXIT;
            }
        } else if (pExynosComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            ret = OMX_TRUE;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "input buffer has not come after flush.");
            goto EXIT;
        }

        if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
            if (dstOutputData->remainDataLen <= (outputUseBuffer->allocSize - outputUseBuffer->dataLen)) {
                copySize = dstOutputData->remainDataLen;
                if (copySize > 0)
                    Exynos_OSAL_Memcpy((outputUseBuffer->bufferHeader->pBuffer + outputUseBuffer->dataLen),
                                       ((char *)dstOutputData->multiPlaneBuffer.dataBuffer[0] + dstOutputData->usedDataLen),
                                       copySize);
                outputUseBuffer->dataLen += copySize;
                outputUseBuffer->remainDataLen += copySize;
                outputUseBuffer->nFlags = dstOutputData->nFlags;
                outputUseBuffer->timeStamp = dstOutputData->timeStamp;

                ret = OMX_TRUE;

                if ((outputUseBuffer->remainDataLen > 0) ||
                    (outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
                    Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                }
            } else {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "output buffer is smaller than encoded data size Out Length");
                pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                        pExynosComponent->callbackData,
                                                        OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                ret = OMX_FALSE;
            }
        } else if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
            if ((outputUseBuffer->remainDataLen > 0) ||
                ((outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                if (exynosOutputPort->bStoreMetaData == OMX_TRUE)
                    Exynos_OSAL_SetDataLengthToMetaData(outputUseBuffer->bufferHeader->pBuffer, outputUseBuffer->remainDataLen);
                Exynos_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            } else {
                Exynos_OMX_FillThisBuffer(pOMXComponent, outputUseBuffer->bufferHeader);
            }
        }
    } else {
        ret = OMX_FALSE;
    }

EXIT:
    FunctionOut();

    return ret;
}

#ifdef USE_METADATABUFFERTYPE
OMX_ERRORTYPE Exynos_OMX_ExtensionSetup(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *exynosInputPort    = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER           *srcInputUseBuffer  = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA                 *pSrcInputData      = &exynosInputPort->processData;
    OMX_COLOR_FORMATTYPE             eColorFormat       = exynosInputPort->portDefinition.format.video.eColorFormat;
    OMX_COLOR_FORMATTYPE             eActualFormat      = OMX_COLOR_FormatUnused;

    int i = 0;
    OMX_PTR ppBuf[MAX_BUFFER_PLANE];

#ifdef USE_METADATABUFFERTYPE
    if (exynosInputPort->bStoreMetaData == OMX_TRUE) {
        Exynos_OSAL_GetInfoFromMetaData((OMX_BYTE)srcInputUseBuffer->bufferHeader->pBuffer, ppBuf);
#ifdef USE_ANDROIDOPAQUE
        if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque) {
            OMX_COLOR_FORMATTYPE eActualFormat = OMX_COLOR_FormatUnused;

            /* kMetadataBufferTypeGrallocSource */
            pVideoEnc->ANBColorFormat = Exynos_OSAL_GetANBColorFormat((int)(intptr_t)ppBuf[0]);

            eActualFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
            if (eActualFormat == OMX_COLOR_FormatUnused) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported color format : ANB color is 0x%x", __func__, pVideoEnc->ANBColorFormat);
                ret = OMX_ErrorNotImplemented;
                goto EXIT;
            }

            if (pVideoEnc->ANBColorFormat == eActualFormat) {
                exynosInputPort->bufferProcessType = BUFFER_SHARE;
                Exynos_SetPlaneToPort(exynosInputPort, Exynos_OSAL_GetPlaneCount(pVideoEnc->ANBColorFormat));
            } else {
                exynosInputPort->bufferProcessType = BUFFER_COPY;
            }
        } else {
            Exynos_SetPlaneToPort(exynosInputPort, Exynos_OSAL_GetPlaneCount(eColorFormat));
        }
#endif
    }
#endif

    if ((exynosInputPort->bufferProcessType & BUFFER_COPY) == BUFFER_COPY) {
        OMX_U32 nDataSize[MAX_BUFFER_PLANE] = {0, 0, 0};
        OMX_U32 nAllocSize[MAX_BUFFER_PLANE] = {0, 0, 0};
        OMX_U32 nFrameWidth  = exynosInputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 nFrameHeight = exynosInputPort->portDefinition.format.video.nFrameHeight;

        eActualFormat = Exynos_Input_GetActualColorFormat(pExynosComponent);
        if (eActualFormat == OMX_COLOR_FormatUnused) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "[%s] unsupported color format : 0x%x", __func__, eActualFormat);
            ret = OMX_ErrorNotImplemented;
            goto EXIT;
        }

        Exynos_SetPlaneToPort(exynosInputPort, Exynos_OSAL_GetPlaneCount(eActualFormat));
        Exynos_OSAL_GetPlaneSize(eActualFormat, nFrameWidth, nFrameHeight, nDataSize, nAllocSize);

        if (pVideoEnc->nInbufSpareSize > 0) {
            for (i = 0; i < Exynos_GetPlaneFromPort(exynosInputPort); i++)
                nAllocSize[i] += pVideoEnc->nInbufSpareSize;
        }

        Exynos_OSAL_SemaphoreCreate(&exynosInputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&exynosInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);

        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX, MFC_INPUT_BUFFER_NUM_MAX, nAllocSize);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)
            Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, pVideoEnc->pMFCEncInputBuffer[i]);
    } else if (exynosInputPort->bufferProcessType == BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

EXIT:

    return ret;
}
#endif

OMX_ERRORTYPE Exynos_OMX_SrcInputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *srcInputUseBuffer = &exynosInputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA          *pSrcInputData = &exynosInputPort->processData;
    OMX_BOOL               bCheckInputData = OMX_FALSE;
    OMX_BOOL               bValidCodecData = OMX_FALSE;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        Exynos_Wait_ProcessPause(pExynosComponent, INPUT_PORT_INDEX);

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort))
                break;
            if (exynosInputPort->portState != OMX_StateIdle)
                break;

            Exynos_OSAL_MutexLock(srcInputUseBuffer->bufferMutex);
            if (pVideoEnc->bFirstInput == OMX_FALSE) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    if ((pSrcInputData->multiPlaneBuffer.dataBuffer[0] == NULL) || (pSrcInputData->pPrivate == NULL)) {
                        Exynos_CodecBufferDequeue(pExynosComponent, INPUT_PORT_INDEX, &codecBuffer);
                        if (codecBuffer != NULL) {
                            Exynos_CodecBufferToData(codecBuffer, pSrcInputData, INPUT_PORT_INDEX);
                        }
                        Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        break;
                    }
                }

                if (srcInputUseBuffer->dataValid == OMX_TRUE) {
                    bCheckInputData = Exynos_Preprocessor_InputData(pOMXComponent, pSrcInputData);
                } else {
                    bCheckInputData = OMX_FALSE;
                }
            }
            if ((bCheckInputData == OMX_FALSE) &&
                (!CHECK_PORT_BEING_FLUSHED(exynosInputPort))) {
                ret = Exynos_InputBufferGetQueue(pExynosComponent);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    break;
                }

                if ((pVideoEnc->bFirstInput == OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosInputPort))) {
                    ret = Exynos_OMX_ExtensionSetup(hComponent);
                    if (ret != OMX_ErrorNone) {
                        (*(pExynosComponent->pCallbacks->EventHandler)) (pOMXComponent,
                                    pExynosComponent->callbackData,
                                    (OMX_U32)OMX_EventError,
                                    (OMX_U32)OMX_ErrorNotImplemented,
                                    INPUT_PORT_INDEX, NULL);
                        Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        break;
                    }
                    pVideoEnc->bFirstInput = OMX_FALSE;
                }

                Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                break;
            }

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort)) {
                Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                break;
            }

            ret = pVideoEnc->exynos_codec_srcInputProcess(pOMXComponent, pSrcInputData);
            Exynos_ResetCodecData(pSrcInputData);
            Exynos_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
            if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorCodecInit)
                pVideoEnc->bExitBufferProcessThread = OMX_TRUE;
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_SrcOutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *srcOutputUseBuffer = &exynosInputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA           srcOutputData;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);

        while (!pVideoEnc->bExitBufferProcessThread) {
            if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                if (Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX) == OMX_FALSE)
                    break;
            }
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosInputPort))
                break;

            Exynos_OSAL_MutexLock(srcOutputUseBuffer->bufferMutex);
            ret = pVideoEnc->exynos_codec_srcOutputProcess(pOMXComponent, &srcOutputData);

            if (ret == OMX_ErrorNone) {
                if (exynosInputPort->bufferProcessType & BUFFER_COPY) {
                    OMX_PTR codecBuffer;
                    codecBuffer = srcOutputData.pPrivate;
                    if (codecBuffer != NULL)
                        Exynos_CodecBufferEnqueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
                }
                if (exynosInputPort->bufferProcessType & BUFFER_SHARE) {
                    OMX_BOOL bNeedUnlock = OMX_FALSE;
                    OMX_COLOR_FORMATTYPE eColorFormat = exynosInputPort->portDefinition.format.video.eColorFormat;
#ifdef USE_ANDROIDOPAQUE
                    if (eColorFormat == (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque)
                        bNeedUnlock = OMX_TRUE;
#endif
                    Exynos_Shared_DataToBuffer(&srcOutputData, srcOutputUseBuffer, bNeedUnlock);
                    Exynos_InputBufferReturn(pOMXComponent, srcOutputUseBuffer);
                }
                Exynos_ResetCodecData(&srcOutputData);
            }
            Exynos_OSAL_MutexUnlock(srcOutputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_DstInputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstInputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.inputDataBuffer;
    EXYNOS_OMX_DATA           dstInputData;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if ((CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) ||
                (!CHECK_PORT_POPULATED(exynosOutputPort)))
                break;
            if (exynosOutputPort->portState != OMX_StateIdle)
                break;

            Exynos_OSAL_MutexLock(dstInputUseBuffer->bufferMutex);
            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                CODEC_ENC_BUFFER *pCodecBuffer = NULL;
                ret = Exynos_CodecBufferDequeue(pExynosComponent, OUTPUT_PORT_INDEX, (OMX_PTR *)&pCodecBuffer);
                if (ret != OMX_ErrorNone) {
                    Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                    break;
                }
                Exynos_CodecBufferToData(pCodecBuffer, &dstInputData, OUTPUT_PORT_INDEX);
            }

            if (exynosOutputPort->bufferProcessType & BUFFER_SHARE) {
                if ((dstInputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                        break;
                    }
                    Exynos_Shared_BufferToData(dstInputUseBuffer, &dstInputData, ONE_PLANE);
                    if (exynosOutputPort->bStoreMetaData == OMX_TRUE) {
                        OMX_PTR dataBuffer = NULL;

                        OMX_PTR ppBuf[MAX_BUFFER_PLANE] = {NULL, NULL, NULL};
                        OMX_U32 nAllocSize = 0;

                        Exynos_OSAL_GetInfoFromMetaData((OMX_BYTE)dstInputUseBuffer->bufferHeader->pBuffer, ppBuf);

                        dataBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory, (int)(intptr_t)ppBuf[0]);
                        if (dataBuffer == NULL) {
                            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Wrong dst-input Metadata buffer", __LINE__);
                            ret = OMX_ErrorUndefined;
                            break;
                        }
                        dstInputData.multiPlaneBuffer.fd[0] = (int)(intptr_t)ppBuf[0];
                        dstInputData.multiPlaneBuffer.dataBuffer[0] = dataBuffer;
                    } else {
                        if (pVideoEnc->bDRMPlayerMode == OMX_TRUE) {
                            OMX_PTR dataBuffer = NULL;
                            dataBuffer = Exynos_OSAL_SharedMemory_IONToVirt(pVideoEnc->hSharedMemory,
                                                        (int)(intptr_t)dstInputData.multiPlaneBuffer.dataBuffer[0]);
                            if (dataBuffer == NULL) {
                                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Wrong dst-input Secure buffer", __LINE__);
                                ret = OMX_ErrorUndefined;
                                break;
                            }
                            dstInputData.multiPlaneBuffer.fd[0] =
                                (int)(intptr_t)dstInputData.multiPlaneBuffer.dataBuffer[0];
                            dstInputData.multiPlaneBuffer.dataBuffer[0] = dataBuffer;
                        } else {
                            dstInputData.multiPlaneBuffer.fd[0] =
                                Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory,
                                            dstInputData.multiPlaneBuffer.dataBuffer[0]);
                        }
                    }
                    Exynos_ResetDataBuffer(dstInputUseBuffer);
                }
            }

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort)) {
                Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
                break;
            }
            ret = pVideoEnc->exynos_codec_dstInputProcess(pOMXComponent, &dstInputData);

            Exynos_ResetCodecData(&dstInputData);
            Exynos_OSAL_MutexUnlock(dstInputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_DstOutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *exynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_OMX_DATABUFFER    *dstOutputUseBuffer = &exynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    EXYNOS_OMX_DATA          *pDstOutputData = &exynosOutputPort->processData;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Exynos_OSAL_SleepMillisec(0);
        Exynos_Wait_ProcessPause(pExynosComponent, OUTPUT_PORT_INDEX);

        while ((Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {
            Exynos_OSAL_SleepMillisec(0);

            if (CHECK_PORT_BEING_FLUSHED(exynosOutputPort))
                break;

            Exynos_OSAL_MutexLock(dstOutputUseBuffer->bufferMutex);
            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(exynosOutputPort))) {
                    ret = Exynos_OutputBufferGetQueue(pExynosComponent);
                    if (ret != OMX_ErrorNone) {
                        Exynos_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
                        break;
                    }
                }
            }

            if ((dstOutputUseBuffer->dataValid == OMX_TRUE) ||
                (exynosOutputPort->bufferProcessType & BUFFER_SHARE))
                ret = pVideoEnc->exynos_codec_dstOutputProcess(pOMXComponent, pDstOutputData);

            if (((ret == OMX_ErrorNone) && (dstOutputUseBuffer->dataValid == OMX_TRUE)) ||
                (exynosOutputPort->bufferProcessType & BUFFER_SHARE)) {
                Exynos_Postprocess_OutputData(pOMXComponent, pDstOutputData);
            }

            if (exynosOutputPort->bufferProcessType & BUFFER_COPY) {
                if (pDstOutputData->pPrivate != NULL) {
                    Exynos_CodecBufferEnqueue(pExynosComponent, OUTPUT_PORT_INDEX, pDstOutputData->pPrivate);
                    pDstOutputData->pPrivate = NULL;
                }
            }

            /* reset outputData */
            Exynos_ResetCodecData(pDstOutputData);
            Exynos_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
        }
    }

EXIT:

    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_SrcInputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_MESSAGE       *message = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Exynos_OMX_SrcInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_SrcOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_MESSAGE       *message = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Exynos_OMX_SrcOutputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstInputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_MESSAGE       *message = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Exynos_OMX_DstInputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Exynos_OMX_DstOutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_OMX_MESSAGE       *message = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Exynos_OMX_DstOutputBufferProcess(pOMXComponent);

    Exynos_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferProcess_Create(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_FALSE;

    ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hDstOutputThread,
                 Exynos_OMX_DstOutputProcessThread,
                 pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hSrcOutputThread,
                     Exynos_OMX_SrcOutputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hDstInputThread,
                     Exynos_OMX_DstInputProcessThread,
                     pOMXComponent);
    if (ret == OMX_ErrorNone)
        ret = Exynos_OSAL_ThreadCreate(&pVideoEnc->hSrcInputThread,
                     Exynos_OMX_SrcInputProcessThread,
                     pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferProcess_Terminate(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
    OMX_S32                countValue = 0;
    unsigned int           i = 0;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_TRUE;

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].codecSemID);
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hSrcInputThread);
    pVideoEnc->hSrcInputThread = NULL;

    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bufferSemID);
    Exynos_OSAL_Get_SemaphoreCount(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID, &countValue);
    if (countValue == 0)
        Exynos_OSAL_SemaphorePost(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].codecSemID);
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hDstInputThread);
    pVideoEnc->hDstInputThread = NULL;

    pVideoEnc->exynos_codec_stop(pOMXComponent, INPUT_PORT_INDEX);
    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, INPUT_PORT_INDEX);
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[INPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hSrcOutputThread);
    pVideoEnc->hSrcOutputThread = NULL;

    pVideoEnc->exynos_codec_stop(pOMXComponent, OUTPUT_PORT_INDEX);
    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, INPUT_PORT_INDEX);
    Exynos_OSAL_SignalSet(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].pauseEvent);
    Exynos_OSAL_ThreadTerminate(pVideoEnc->hDstOutputThread);
    pVideoEnc->hDstOutputThread = NULL;

    pExynosComponent->checkTimeStamp.needSetStartTimeStamp      = OMX_FALSE;
    pExynosComponent->checkTimeStamp.needCheckStartTimeStamp    = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = Exynos_OMX_BaseComponent_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = Exynos_OMX_Port_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = Exynos_OSAL_Malloc(sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    if (pVideoEnc == NULL) {
        Exynos_OMX_Port_Destructor(pOMXComponent);
        Exynos_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    Exynos_OSAL_Memset(pVideoEnc, 0, sizeof(EXYNOS_OMX_VIDEOENC_COMPONENT));
    pExynosComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoEnc;

    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    pVideoEnc->bFirstInput  = OMX_TRUE;
    pVideoEnc->bFirstOutput = OMX_FALSE;
    pVideoEnc->configChange = OMX_FALSE;
    pVideoEnc->bQosChanged  = OMX_FALSE;
    pVideoEnc->nQosRatio       = 0;
    pVideoEnc->nInbufSpareSize = 0;
    pVideoEnc->qpRange.videoMinQP = 0;
    pVideoEnc->qpRange.videoMaxQP = 0;
    pVideoEnc->quantization.nQpI = 4; // I frame quantization parameter
    pVideoEnc->quantization.nQpP = 5; // P frame quantization parameter
    pVideoEnc->quantization.nQpB = 5; // B frame quantization parameter

    pExynosComponent->bMultiThreadProcess = OMX_TRUE;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * INPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_INPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = 0;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 64000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;
    pVideoEnc->eControlRate[INPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;

#ifdef USE_METADATABUFFERTYPE
    pExynosPort->bStoreMetaData = OMX_FALSE;
#endif

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->supportFormat = Exynos_OSAL_Malloc(sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX);
    if (pExynosPort->supportFormat == NULL) {
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosPort->supportFormat, 0, (sizeof(OMX_COLOR_FORMATTYPE) * OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX));

    pExynosPort->portDefinition.nBufferCountActual = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferCountMin = MAX_VIDEO_OUTPUTBUFFER_NUM;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.eDomain = OMX_PortDomainVideo;

    pExynosPort->portDefinition.format.video.cMIMEType = Exynos_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    if (pExynosPort->portDefinition.format.video.cMIMEType == NULL) {
        Exynos_OMX_VideoEncodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pExynosPort->portDefinition.format.video.nFrameWidth = 0;
    pExynosPort->portDefinition.format.video.nFrameHeight= 0;
    pExynosPort->portDefinition.format.video.nStride = 0;
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.format.video.nBitrate = 64000;
    pExynosPort->portDefinition.format.video.xFramerate = (15 << 16);
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.format.video.pNativeWindow = NULL;
    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;

    pOMXComponent->UseBuffer              = &Exynos_OMX_UseBuffer;
    pOMXComponent->AllocateBuffer         = &Exynos_OMX_AllocateBuffer;
    pOMXComponent->FreeBuffer             = &Exynos_OMX_FreeBuffer;
    pOMXComponent->ComponentTunnelRequest = &Exynos_OMX_ComponentTunnelRequest;

    pExynosComponent->exynos_AllocateTunnelBuffer = &Exynos_OMX_AllocateTunnelBuffer;
    pExynosComponent->exynos_FreeTunnelBuffer     = &Exynos_OMX_FreeTunnelBuffer;
    pExynosComponent->exynos_BufferProcessCreate    = &Exynos_OMX_BufferProcess_Create;
    pExynosComponent->exynos_BufferProcessTerminate = &Exynos_OMX_BufferProcess_Terminate;
    pExynosComponent->exynos_BufferFlush          = &Exynos_OMX_BufferFlush;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeComponentDeinit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    int                            i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_Free(pVideoEnc);
    pExynosComponent->hComponentHandle = pVideoEnc = NULL;

    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    if (pExynosPort->processData.extInfo != NULL) {
        Exynos_OSAL_Free(pExynosPort->processData.extInfo);
        pExynosPort->processData.extInfo = NULL;
    }

    for(i = 0; i < ALL_PORT_NUM; i++) {
        pExynosPort = &pExynosComponent->pExynosPort[i];
        Exynos_OSAL_Free(pExynosPort->portDefinition.format.video.cMIMEType);
        pExynosPort->portDefinition.format.video.cMIMEType = NULL;

        Exynos_OSAL_Free(pExynosPort->supportFormat);
        pExynosPort->supportFormat = NULL;
    }

    ret = Exynos_OMX_Port_Destructor(pOMXComponent);

    ret = Exynos_OMX_BaseComponent_Destructor(hComponent);

EXIT:
    FunctionOut();

    return ret;
}

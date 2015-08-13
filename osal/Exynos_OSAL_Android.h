/*
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
 * @file        Exynos_OSAL_Android.h
 * @brief
 * @author      Seungbeom Kim (sbcrux.kim@samsung.com)
 * @author      Hyeyeon Chung (hyeon.chung@samsung.com)
 * @author      Yunji Kim (yunji.kim@samsung.com)
 * @author      Jinsung Yang (jsgood.yang@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef Exynos_OSAL_ANDROID
#define Exynos_OSAL_ANDROID

#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Index.h"
#include "Exynos_OSAL_SharedMemory.h"

typedef struct _EXYNOS_OMX_SHARED_BUFFER {
    OMX_S32 BufferFd;
    OMX_S32 BufferFd1;
    OMX_S32 BufferFd2;
    unsigned long *pIonHandle;
    unsigned long *pIonHandle1;
    unsigned long *pIonHandle2;
    OMX_U32 cnt;
} EXYNOS_OMX_SHARED_BUFFER;

typedef struct _EXYNOS_OMX_REF_HANDLE {
    OMX_HANDLETYPE           hMutex;
    OMX_PTR                  pGrallocModule;
    EXYNOS_OMX_SHARED_BUFFER SharedBuffer[MAX_BUFFER_REF];
} EXYNOS_OMX_REF_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    kMetadataBufferTypeEncodeOutput = 4,

    // Add more here...

} VendorMetadataBufferType;

OMX_ERRORTYPE Exynos_OSAL_GetAndroidParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_INDEXTYPE nIndex,
                                          OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE Exynos_OSAL_SetAndroidParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_INDEXTYPE nIndex,
                                          OMX_IN OMX_PTR ComponentParameterStructure);

OMX_COLOR_FORMATTYPE Exynos_OSAL_GetANBColorFormat(OMX_IN OMX_U32 handle);

OMX_ERRORTYPE Exynos_OSAL_LockMetaData(OMX_IN OMX_PTR pBuffer,
                                       OMX_IN OMX_U32 width,
                                       OMX_IN OMX_U32 height,
                                       OMX_IN OMX_COLOR_FORMATTYPE format,
                                       OMX_OUT OMX_U32 *pStride,
                                       OMX_OUT OMX_PTR planes);

OMX_ERRORTYPE Exynos_OSAL_UnlockMetaData(OMX_IN OMX_PTR pBuffer);

OMX_ERRORTYPE Exynos_OSAL_LockANBHandle(OMX_IN OMX_U32 pBuffer,
                                        OMX_IN OMX_U32 width,
                                        OMX_IN OMX_U32 height,
                                        OMX_IN OMX_COLOR_FORMATTYPE format,
                                        OMX_OUT OMX_U32 *pStride,
                                        OMX_OUT OMX_PTR planes);

#ifdef USE_ANB_REF
OMX_HANDLETYPE Exynos_OSAL_RefANB_Create();
OMX_ERRORTYPE Exynos_OSAL_RefANB_Reset(OMX_HANDLETYPE hREF);
OMX_ERRORTYPE Exynos_OSAL_RefANB_Terminate(OMX_HANDLETYPE hREF);
OMX_ERRORTYPE Exynos_OSAL_RefANB_Increase(OMX_HANDLETYPE hREF, OMX_PTR pBuffer);
OMX_ERRORTYPE Exynos_OSAL_RefANB_Decrease(OMX_HANDLETYPE hREF, OMX_S32 BufferFd);
#endif

OMX_ERRORTYPE Exynos_OSAL_UnlockANBHandle(OMX_IN OMX_U32 pBuffer);

OMX_ERRORTYPE Exynos_OSAL_GetInfoFromMetaData(OMX_IN OMX_BYTE pBuffer,
                                              OMX_OUT OMX_PTR *pOutBuffer);

OMX_PTR Exynos_OSAL_AllocMetaDataBuffer(OMX_HANDLETYPE hSharedMemory, EXYNOS_CODEC_TYPE codecType, OMX_U32 nPortIndex, OMX_U32 nSizeBytes, MEMORY_TYPE eMemoryType);
OMX_ERRORTYPE Exynos_OSAL_FreeMetaDataBuffer(OMX_HANDLETYPE hSharedMemory, EXYNOS_CODEC_TYPE codecType, OMX_U32 nPortIndex, OMX_PTR pTempBuffer);
OMX_ERRORTYPE Exynos_OSAL_SetDataLengthToMetaData(OMX_IN OMX_BYTE pBuffer, OMX_IN OMX_U32 dataLength);

OMX_ERRORTYPE Exynos_OSAL_CheckANB(OMX_IN EXYNOS_OMX_DATA *pBuffer,
                                   OMX_OUT OMX_BOOL *bIsANBEnabled);

OMX_ERRORTYPE Exynos_OSAL_SetPrependSPSPPSToIDR(OMX_PTR pComponentParameterStructure,
                                                OMX_PTR pbPrependSpsPpsToIdr);

OMX_ERRORTYPE Exynos_OSAL_SetAndroidParameter(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE useAndroidNativeBuffer(EXYNOS_OMX_BASEPORT *pExynosPort, OMX_BUFFERHEADERTYPE **ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8 *pBuffer);
#ifdef __cplusplus
}
#endif

#endif

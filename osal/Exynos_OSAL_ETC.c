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
 * @file        Exynos_OSAL_ETC.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <system/graphics.h>

#include "Exynos_OSAL_Memory.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Log.h"
#include "Exynos_OMX_Macros.h"

#include "ExynosVideoApi.h"
#include "exynos_format.h"

static struct timeval perfStart[PERF_ID_MAX+1], perfStop[PERF_ID_MAX+1];
static unsigned long perfTime[PERF_ID_MAX+1], totalPerfTime[PERF_ID_MAX+1];
static unsigned int perfFrameCount[PERF_ID_MAX+1], perfOver30ms[PERF_ID_MAX+1];

#ifndef HAVE_GETLINE
ssize_t getline(char **ppLine, size_t *pLen, FILE *pStream)
{
    char *pCurrentPointer = NULL;
    size_t const chunk = 512;

    size_t defaultBufferSize = chunk + 1;
    size_t retSize = 0;

    if (*ppLine == NULL) {
        *ppLine = (char *)malloc(defaultBufferSize);
        if (*ppLine == NULL) {
            retSize = -1;
            goto EXIT;
        }
        *pLen = defaultBufferSize;
    }
    else {
        if (*pLen < defaultBufferSize) {
            *ppLine = (char *)realloc(*ppLine, defaultBufferSize);
            if (*ppLine == NULL) {
                retSize = -1;
                goto EXIT;
            }
            *pLen = defaultBufferSize;
        }
    }

    while (1) {
        size_t i;
        size_t j = 0;
        size_t readByte = 0;

        pCurrentPointer = *ppLine + readByte;

        i = fread(pCurrentPointer, 1, chunk, pStream);
        if (i < chunk && ferror(pStream)) {
            retSize = -1;
            goto EXIT;
        }
        while (j < i) {
            ++j;
            if (*pCurrentPointer++ == (char)'\n') {
                *pCurrentPointer = '\0';
                if (j != i) {
                    if (fseek(pStream, j - i, SEEK_CUR)) {
                        retSize = -1;
                        goto EXIT;
                }
                    if (feof(pStream))
                        clearerr(pStream);
                }
                readByte += j;
                retSize = readByte;
                goto EXIT;
            }
        }

        readByte += j;
        if (feof(pStream)) {
            if (readByte) {
                retSize = readByte;
                goto EXIT;
            }
            if (!i) {
                retSize = -1;
                goto EXIT;
            }
        }

        i = ((readByte + (chunk * 2)) / chunk) * chunk;
        if (i != *pLen) {
            *ppLine = (char *)realloc(*ppLine, i);
            if (*ppLine == NULL) {
                retSize = -1;
                goto EXIT;
        }
            *pLen = i;
        }
    }

EXIT:
    return retSize;
}
#endif /* HAVE_GETLINE */

size_t Exynos_OSAL_Strcpy(OMX_PTR dest, OMX_PTR src)
{
    return strlcpy(dest, src, (size_t)(strlen((const char *)src) + 1));
}

size_t Exynos_OSAL_Strncpy(OMX_PTR dest, OMX_PTR src, size_t num)
{
    return strlcpy(dest, src, (size_t)(num + 1));
}

OMX_S32 Exynos_OSAL_Strcmp(OMX_PTR str1, OMX_PTR str2)
{
    return strcmp(str1, str2);
}

OMX_S32 Exynos_OSAL_Strncmp(OMX_PTR str1, OMX_PTR str2, size_t num)
{
    return strncmp(str1, str2, num);
}

size_t Exynos_OSAL_Strcat(OMX_PTR dest, OMX_PTR src)
{
    return strlcat(dest, src, (size_t)(strlen((const char *)dest) + strlen((const char *)src) + 1));
}

size_t Exynos_OSAL_Strncat(OMX_PTR dest, OMX_PTR src, size_t num)
{
    /* caution : num should be a size of dest buffer */
    return strlcat(dest, src, (size_t)(strlen((const char *)dest) + strlen((const char *)src) + 1));
}

size_t Exynos_OSAL_Strlen(const char *str)
{
    return strlen(str);
}

static OMX_U32 MeasureTime(struct timeval *start, struct timeval *stop)
{
    unsigned long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;
    if (stop->tv_usec >= start->tv_usec) {
        usec = stop->tv_usec - start->tv_usec;
    } else {
        usec = stop->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }

    time = sec * 1000000 + (usec);

    return time;
}

void Exynos_OSAL_PerfInit(PERF_ID_TYPE id)
{
    memset(&perfStart[id], 0, sizeof(perfStart[id]));
    memset(&perfStop[id], 0, sizeof(perfStop[id]));
    perfTime[id] = 0;
    totalPerfTime[id] = 0;
    perfFrameCount[id] = 0;
    perfOver30ms[id] = 0;
}

void Exynos_OSAL_PerfStart(PERF_ID_TYPE id)
{
    gettimeofday(&perfStart[id], NULL);
}

void Exynos_OSAL_PerfStop(PERF_ID_TYPE id)
{
    gettimeofday(&perfStop[id], NULL);

    perfTime[id] = MeasureTime(&perfStart[id], &perfStop[id]);
    totalPerfTime[id] += perfTime[id];
    perfFrameCount[id]++;

    if (perfTime[id] > 30000)
        perfOver30ms[id]++;
}

OMX_U32 Exynos_OSAL_PerfFrame(PERF_ID_TYPE id)
{
    return perfTime[id];
}

OMX_U32 Exynos_OSAL_PerfTotal(PERF_ID_TYPE id)
{
    return totalPerfTime[id];
}

OMX_U32 Exynos_OSAL_PerfFrameCount(PERF_ID_TYPE id)
{
    return perfFrameCount[id];
}

int Exynos_OSAL_PerfOver30ms(PERF_ID_TYPE id)
{
    return perfOver30ms[id];
}

void Exynos_OSAL_PerfPrint(OMX_STRING prefix, PERF_ID_TYPE id)
{
    OMX_U32 perfTotal;
    int frameCount;

    frameCount = Exynos_OSAL_PerfFrameCount(id);
    perfTotal = Exynos_OSAL_PerfTotal(id);

    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "%s Frame Count: %d", prefix, frameCount);
    Exynos_OSAL_Log(EXYNOS_LOG_INFO, "%s Avg Time: %.2f ms, Over 30ms: %d",
                prefix, (float)perfTotal / (float)(frameCount * 1000),
                Exynos_OSAL_PerfOver30ms(id));
}

unsigned int Exynos_OSAL_GetPlaneCount(
    OMX_COLOR_FORMATTYPE omx_format)
{
    unsigned int plane_cnt = 0;
    switch ((int)omx_format) {
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_SEC_COLOR_FormatYVU420Planar:
        plane_cnt = 3;
        break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatNV12Tiled:
        plane_cnt = 2;
        break;
    case OMX_COLOR_Format32bitARGB8888:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_SEC_COLOR_Format32bitABGR8888:
        plane_cnt = 1;
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: unsupported color format(%x).", __func__, omx_format);
        plane_cnt = 0;
        break;
    }

    return plane_cnt;
}

void Exynos_OSAL_GetPlaneSize(
    OMX_COLOR_FORMATTYPE    eColorFormat,
    OMX_U32                 nWidth,
    OMX_U32                 nHeight,
    OMX_U32                 nDataSize[MAX_BUFFER_PLANE],
    OMX_U32                 nAllocSize[MAX_BUFFER_PLANE])
{
    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar:
        nDataSize[0] = nWidth * nHeight;
        nDataSize[1] = nDataSize[0] >> 2;
        nDataSize[2] = nDataSize[1];

        nAllocSize[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16), 256);
        nAllocSize[1] = ALIGN(ALIGN(nWidth >> 1, 16) * (ALIGN(nHeight, 16) >> 1), 256);
        nAllocSize[2] = nAllocSize[1];
        break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear:
    case (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled:
        nDataSize[0] = nWidth * nHeight;
        nDataSize[1] = nDataSize[0] >> 1;

        nAllocSize[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16), 256);
        nAllocSize[1] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16) >> 1, 256);
        break;
    case OMX_COLOR_Format32bitARGB8888:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_SEC_COLOR_Format32bitABGR8888:
        nDataSize[0] = nWidth * nHeight * 4;

        nAllocSize[0] = ALIGN(ALIGN(nWidth, 16) * ALIGN(nHeight, 16) * 4, 256);
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: unsupported color format(%x).", __func__, eColorFormat);
        break;
    }
}

int Exynos_OSAL_OMX2VideoFormat(
    OMX_COLOR_FORMATTYPE eColorFormat)
{
    ExynosVideoColorFormatType nVideoFormat = VIDEO_COLORFORMAT_UNKNOWN;

    switch ((int)eColorFormat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
        nVideoFormat = VIDEO_COLORFORMAT_NV12;
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
    case OMX_SEC_COLOR_FormatNV21LPhysicalAddress:
        nVideoFormat = VIDEO_COLORFORMAT_NV21;
        break;
    case OMX_SEC_COLOR_FormatNV12Tiled:
    case OMX_SEC_COLOR_FormatNV12LPhysicalAddress:
        nVideoFormat = VIDEO_COLORFORMAT_NV12_TILED;
        break;
    case OMX_COLOR_FormatYUV420Planar:
        nVideoFormat = VIDEO_COLORFORMAT_I420;
        break;
    case OMX_SEC_COLOR_FormatYVU420Planar:
        nVideoFormat = VIDEO_COLORFORMAT_YV12;
        break;
    case OMX_COLOR_Format32bitBGRA8888:
        nVideoFormat = VIDEO_COLORFORMAT_ARGB8888;
        break;
    case OMX_COLOR_Format32bitARGB8888:
        nVideoFormat = VIDEO_COLORFORMAT_BGRA8888;
        break;
    case OMX_SEC_COLOR_Format32bitABGR8888:
        nVideoFormat = VIDEO_COLORFORMAT_RGBA8888;
        break;
    default:
        break;
    }

    return (int)nVideoFormat;
}

OMX_COLOR_FORMATTYPE Exynos_OSAL_Video2OMXFormat(
    int nVideoFormat)
{
    OMX_COLOR_FORMATTYPE eOMXFormat = OMX_COLOR_FormatUnused;

    switch (nVideoFormat) {
    case VIDEO_COLORFORMAT_NV12:
        eOMXFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case VIDEO_COLORFORMAT_NV21:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;
        break;
    case VIDEO_COLORFORMAT_NV12_TILED:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;
        break;
    case VIDEO_COLORFORMAT_I420:
        eOMXFormat = OMX_COLOR_FormatYUV420Planar;
        break;
    case VIDEO_COLORFORMAT_YV12:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;
        break;
    case VIDEO_COLORFORMAT_ARGB8888:
        eOMXFormat = OMX_COLOR_Format32bitBGRA8888;
        break;
    case VIDEO_COLORFORMAT_BGRA8888:
        eOMXFormat = OMX_COLOR_Format32bitARGB8888;
        break;
    case VIDEO_COLORFORMAT_RGBA8888:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format32bitABGR8888;
        break;
    default:
        break;
    }

    return eOMXFormat;
}

OMX_COLOR_FORMATTYPE Exynos_OSAL_HAL2OMXColorFormat(
    unsigned int nHALFormat)
{
    OMX_COLOR_FORMATTYPE eOMXFormat = OMX_COLOR_FormatUnused;

    switch (nHALFormat) {
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        eOMXFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV12Tiled;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatNV21Linear;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
        eOMXFormat = OMX_COLOR_FormatYUV420Planar;
        break;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_FormatYVU420Planar;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        eOMXFormat = OMX_COLOR_FormatYCbYCr;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        eOMXFormat = OMX_COLOR_Format32bitARGB8888;
        break;
    case HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888:
        eOMXFormat = OMX_COLOR_Format32bitBGRA8888;
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        eOMXFormat = (OMX_COLOR_FORMATTYPE)OMX_SEC_COLOR_Format32bitABGR8888;
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "HAL format is unsupported(0x%x)", nHALFormat);
        eOMXFormat = OMX_COLOR_FormatUnused;
        break;
    }

    return eOMXFormat;
}

unsigned int Exynos_OSAL_OMX2HALPixelFormat(
    OMX_COLOR_FORMATTYPE eOMXFormat,
    PLANE_TYPE           ePlaneType)
{
    unsigned int nHALFormat = 0;

    if (ePlaneType == PLANE_SINGLE) {  /* configured by single FD */
        switch ((int)eOMXFormat) {
        /* YUV formats */
        case OMX_COLOR_FormatYUV420SemiPlanar: // gralloc ?
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP;
            break;
        case OMX_SEC_COLOR_FormatNV21Linear:
            nHALFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            break;
        case OMX_COLOR_FormatYUV420Planar:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P;
            break;
        case OMX_SEC_COLOR_FormatYVU420Planar:
            nHALFormat = HAL_PIXEL_FORMAT_YV12;
            break;
        case OMX_COLOR_FormatYCbYCr:
            nHALFormat = HAL_PIXEL_FORMAT_YCbCr_422_I;
            break;
        /* RGB formats */
        case OMX_COLOR_Format32bitARGB8888:
            nHALFormat = HAL_PIXEL_FORMAT_BGRA_8888;
            break;
        case OMX_COLOR_Format32bitBGRA8888:
             nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_ARGB_8888;
             break;
        case OMX_SEC_COLOR_Format32bitABGR8888:
            nHALFormat = HAL_PIXEL_FORMAT_RGBA_8888;
            break;
        default:
            break;
        }
    } else {  /* configured by multiple FD */
        switch ((int)eOMXFormat) {
        case OMX_COLOR_FormatYUV420SemiPlanar:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M;
            break;
        case OMX_SEC_COLOR_FormatNV12Tiled:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED;
            break;
        case OMX_SEC_COLOR_FormatNV21Linear:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
            break;
        case OMX_COLOR_FormatYUV420Planar:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M;
            break;
        case OMX_SEC_COLOR_FormatYVU420Planar:
            nHALFormat = HAL_PIXEL_FORMAT_EXYNOS_YV12_M;
            break;
        default:
            break;
        }
    }

    return nHALFormat;
}

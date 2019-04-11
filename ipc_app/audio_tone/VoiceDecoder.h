//
//  SinVoice Project
//  VoiceDecoder.h
//
//  Created by gujicheng on 14-8-21.
//  Copyright (c) 2014 gujicheng. All rights reserved.
//

#ifndef __VoiceDecoder_H__
#define __VoiceDecoder_H__

#include "ESType.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __VoiceDecoderCallback
{
    ESVoid (*onDecoderStart)(ESVoid* cbParam);
    ESVoid (*onDecoderToken)(ESVoid* cbParam, ESInt32 index);
    ESVoid (*onDecoderEnd)(ESVoid* cbParam, ESInt32 result);
} VoiceDecoderCallback;

typedef struct __VoiceDecoder VoiceDecoder;

VoiceDecoder* VoiceDecoder_create(const ESChar* companyId, const ESChar* appId,const VoiceDecoderCallback* callback, ESVoid* cbParam);

ESBool VoiceDecoder_start(VoiceDecoder* pThis);

ESBool VoiceDecoder_putData(VoiceDecoder* pThis, const ESInt16* data, ESUint32 dataCount);

ESBool VoiceDecoder_stop(VoiceDecoder* pThis);

ESVoid VoiceDecoder_destroy(VoiceDecoder* pThis);

#ifdef __cplusplus
}
#endif

#endif /* __VoiceDecoder_H__ */


#include "ipc_rtp.h"
#include "ipc_debug.h"
#include "ipc_malloc.h"


int rtp_aac_alloc(unsigned char* _pu8Frame, unsigned int _u32FrameSize,
                      unsigned short* _pu16SeqNumber, unsigned int _u32Timestamp,
                      RTP_SPLIT_S* _pRetRtpSplit)
{
    CHECK(_pu8Frame != NULL, -1, "invalid parameter with: %#x\n", _pu8Frame);
	CHECK(_u32FrameSize > AAC_HEADER+1, -1, "invalid parameter with: %#x\n", _u32FrameSize);
    CHECK(_pRetRtpSplit != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit);
	CHECK(_pu8Frame[0] == 0xFF && _pu8Frame[1] == 0xF9 && _pu8Frame[6] == 0xFC,
              -1, "audio frame no ADTS, %02x%02x%02x\n",
              (unsigned int)_pu8Frame[0], (unsigned int)_pu8Frame[1], (unsigned int)_pu8Frame[6]);
    
#define RTP_AAC_HEADER_SIZE (sizeof(RTP_FIXED_HEADER) + 4) 
    CHECK(RTP_AAC_HEADER_SIZE == 16, -1, "RTP_AAC_HEADER_SIZE %u\n", RTP_AAC_HEADER_SIZE);

    _pRetRtpSplit->u32SegmentCount = 1;
    _pRetRtpSplit->u32BufSize = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_AAC_HEADER_SIZE + _u32FrameSize;
    _pRetRtpSplit->pu8Buf = (unsigned char*)mem_malloc(_pRetRtpSplit->u32BufSize);
    _pRetRtpSplit->ppu8Segment = (unsigned char**)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned char**));
    _pRetRtpSplit->pU32SegmentSize = (unsigned int*)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned int*));

    RTSP_INTERLEAVED_FRAME stRtspInterleavedFrame;
    RTP_FIXED_HEADER stRtpFixedHeader;
	AU_HEADER stAuHeader;

    memset(&stRtspInterleavedFrame, 0, sizeof(RTSP_INTERLEAVED_FRAME));
    memset(&stRtpFixedHeader, 0, sizeof(RTP_FIXED_HEADER));
	memset(&stAuHeader, 0, sizeof(AU_HEADER));

    RTSP_INTERLEAVED_FRAME* rtsp_intereaved = &stRtspInterleavedFrame;
    RTP_FIXED_HEADER* rtp_hdr = &stRtpFixedHeader;
	AU_HEADER* au_hdr = &stAuHeader;

	_u32FrameSize = _u32FrameSize - AAC_HEADER;
	_pu8Frame = _pu8Frame + AAC_HEADER;
	rtp_hdr->marker = 1;

	au_hdr->au_header[0] = 0;
	au_hdr->au_header[1] = 16;
	au_hdr->au_header[2] = (_u32FrameSize & 0x1fe0) >> 5;
	au_hdr->au_header[3] = (_u32FrameSize & 0x1f) << 3;		

    rtsp_intereaved->flag = '$';
    rtsp_intereaved->channel = 2;

    rtp_hdr->version = 2;
    rtp_hdr->payload = AAC;
    rtp_hdr->ssrc    = htonl(10);

    unsigned char* pu8BufCurr = _pRetRtpSplit->pu8Buf;

    _pRetRtpSplit->ppu8Segment[0] = pu8BufCurr;
    _pRetRtpSplit->pU32SegmentSize[0] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_AAC_HEADER_SIZE + _u32FrameSize;

	rtsp_intereaved->size = htons(RTP_AAC_HEADER_SIZE + _u32FrameSize);
		
    rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
    rtp_hdr->timestamp = htonl(_u32Timestamp);

    memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
    pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
    memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
    pu8BufCurr += sizeof(RTP_FIXED_HEADER);
	memcpy(pu8BufCurr, au_hdr, sizeof(AU_HEADER));
	pu8BufCurr += sizeof(AU_HEADER);
    memcpy(pu8BufCurr, _pu8Frame, _u32FrameSize);
    pu8BufCurr += _u32FrameSize;

    return 0;
}

int rtp_h264_split(unsigned char* _pu8Frame, unsigned int _u32FrameSize)
{
    CHECK(_pu8Frame[0] == 0x00 && _pu8Frame[1] == 0x00 && _pu8Frame[2] == 0x00 && _pu8Frame[3] == 0x01, -1, "invalid parameter\n");

    unsigned int i;
    for(i = 4; i < _u32FrameSize - 4; i++)
    {
        if(_pu8Frame[i+0] == 0x00 && _pu8Frame[i+1] == 0x00 && _pu8Frame[i+2] == 0x00 && _pu8Frame[i+3] == 0x01)
        {
            return i;
        }
    }
    return -1;
}

int rtp_h264_alloc(unsigned char* _pu8Frame, unsigned int _u32FrameSize,
                       unsigned short* _pu16SeqNumber, unsigned int _u32Timestamp,
                       RTP_SPLIT_S* _pRetRtpSplit)
{
    CHECK(_pu8Frame != NULL, -1, "invalid parameter with: %#x\n", _pu8Frame);
    CHECK(_u32FrameSize > 5, -1, "invalid parameter with: %#x\n", _u32FrameSize);
    CHECK(_pRetRtpSplit != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit);

    CHECK(_pu8Frame[0] == 0x00 && _pu8Frame[1] == 0x00 && _pu8Frame[2] == 0x00 && _pu8Frame[3] == 0x01,
              -1, "h264 frame is invalid, %02x%02x%02x%02x\n",
              (unsigned int)_pu8Frame[0], (unsigned int)_pu8Frame[1], (unsigned int)_pu8Frame[2], (unsigned int)_pu8Frame[3]);
    CHECK(_pu8Frame[4] == 0x67 || _pu8Frame[4] == 0x68 || _pu8Frame[4] == 0x06 || _pu8Frame[4] == 0x65 || _pu8Frame[4] == 0x61 || _pu8Frame[4] == 0x41,
              -1, "nal %02x is invalid\n", (unsigned int)_pu8Frame[4]);

#define RTP_HEADER_SIZE (sizeof(RTP_FIXED_HEADER) + sizeof(NALU_HEADER))
#define RTP_HEADER_SIZE2 (sizeof(RTP_FIXED_HEADER) + sizeof(FU_INDICATOR) + sizeof(FU_HEADER))
    CHECK(RTP_HEADER_SIZE == 13, -1, "RTP_HEADER_SIZE %u\n", RTP_HEADER_SIZE);
    CHECK(RTP_HEADER_SIZE2 == 14, -1, "RTP_HEADER_SIZE2 %u\n", RTP_HEADER_SIZE2);

    unsigned char u8Nal = _pu8Frame[4];

    _pu8Frame += 5;
    _u32FrameSize -= 5;
    if(_u32FrameSize <= UDP_MAX_SIZE)
    {
        _pRetRtpSplit->u32SegmentCount = 1;
        _pRetRtpSplit->u32BufSize = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE + _u32FrameSize;
        _pRetRtpSplit->pu8Buf = (unsigned char*)mem_malloc(_pRetRtpSplit->u32BufSize);
        _pRetRtpSplit->ppu8Segment = (unsigned char**)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned char**));
        _pRetRtpSplit->pU32SegmentSize = (unsigned int*)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned int*));
    }
    else
    {
        int packetNum = _u32FrameSize/UDP_MAX_SIZE;
        if ((_u32FrameSize % UDP_MAX_SIZE) != 0)
        {
            packetNum ++;
        }

        int lastPackSize = _u32FrameSize - (packetNum-1) * UDP_MAX_SIZE;

        _pRetRtpSplit->u32SegmentCount = packetNum;
        _pRetRtpSplit->u32BufSize = (sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE2 + UDP_MAX_SIZE) * (packetNum - 1)
                                    + (sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE2 + lastPackSize);
        _pRetRtpSplit->pu8Buf = (unsigned char*)mem_malloc(_pRetRtpSplit->u32BufSize);
        _pRetRtpSplit->ppu8Segment = (unsigned char**)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned char**));
        _pRetRtpSplit->pU32SegmentSize = (unsigned int*)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned int*));
    }

    NALU_t stNalut;
    RTSP_INTERLEAVED_FRAME stRtspInterleavedFrame;
    RTP_FIXED_HEADER stRtpFixedHeader;
    NALU_HEADER stNaluHeader;
    FU_INDICATOR stFuIndicator;
    FU_HEADER stFuHeader;

    memset(&stRtspInterleavedFrame, 0, sizeof(RTSP_INTERLEAVED_FRAME));
    memset(&stRtpFixedHeader, 0, sizeof(RTP_FIXED_HEADER));
    memset(&stNaluHeader, 0, sizeof(NALU_HEADER));
    memset(&stFuIndicator, 0, sizeof(FU_INDICATOR));
    memset(&stFuHeader, 0, sizeof(FU_HEADER));

    NALU_t* n = &stNalut;
    RTSP_INTERLEAVED_FRAME* rtsp_intereaved = &stRtspInterleavedFrame;
    RTP_FIXED_HEADER* rtp_hdr = &stRtpFixedHeader;
    NALU_HEADER* nalu_hdr = &stNaluHeader;
    FU_INDICATOR* fu_ind = &stFuIndicator;
    FU_HEADER* fu_hdr = &stFuHeader;

    stNalut.buf = (char*)_pu8Frame;
    stNalut.len = _u32FrameSize;
    stNalut.forbidden_bit = u8Nal & 0x80;
    stNalut.nal_reference_idc = u8Nal & 0x60;
    stNalut.nal_unit_type = u8Nal & 0x1f;


    rtsp_intereaved->flag = '$';
    rtsp_intereaved->channel = 0;

    rtp_hdr->version = 2;
    rtp_hdr->marker  = 0;
    rtp_hdr->payload = H264;
    rtp_hdr->ssrc    = htonl(10);

    unsigned char* pu8BufCurr = _pRetRtpSplit->pu8Buf;
    if(n->len <= UDP_MAX_SIZE)
    {
        _pRetRtpSplit->ppu8Segment[0] = pu8BufCurr;
        _pRetRtpSplit->pU32SegmentSize[0] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE + n->len;

        rtsp_intereaved->size = htons(RTP_HEADER_SIZE + n->len);

        rtp_hdr->marker=1;
        rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
        rtp_hdr->timestamp = htonl(_u32Timestamp);

        nalu_hdr->F = n->forbidden_bit;
        nalu_hdr->NRI = n->nal_reference_idc >> 5;
        nalu_hdr->TYPE = n->nal_unit_type;

        memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
        pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
        memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
        pu8BufCurr += sizeof(RTP_FIXED_HEADER);
        memcpy(pu8BufCurr, nalu_hdr, sizeof(NALU_HEADER));
        pu8BufCurr += sizeof(NALU_HEADER);
        memcpy(pu8BufCurr, n->buf, n->len);
        pu8BufCurr += n->len;
    }
    else
    {
        int packetNum = n->len/UDP_MAX_SIZE;
        if ((n->len % UDP_MAX_SIZE) != 0)
        {
            packetNum ++;
        }

        int lastPackSize = n->len - (packetNum-1) * UDP_MAX_SIZE;
        int packetIndex = 1 ;

        _pRetRtpSplit->ppu8Segment[packetIndex - 1] = pu8BufCurr;
        _pRetRtpSplit->pU32SegmentSize[packetIndex - 1] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE2 + UDP_MAX_SIZE;

        rtsp_intereaved->size = htons(RTP_HEADER_SIZE2 + UDP_MAX_SIZE);

        rtp_hdr->marker=0;
        rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
        rtp_hdr->timestamp = htonl(_u32Timestamp);

        fu_ind->F = n->forbidden_bit;
        fu_ind->NRI = n->nal_reference_idc>>5;
        fu_ind->TYPE = 28;

        fu_hdr->S = 1;
        fu_hdr->E = 0;
        fu_hdr->R = 0;
        fu_hdr->TYPE = n->nal_unit_type;

        memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
        pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
        memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
        pu8BufCurr += sizeof(RTP_FIXED_HEADER);
        memcpy(pu8BufCurr, fu_ind, sizeof(FU_INDICATOR));
        pu8BufCurr += sizeof(FU_INDICATOR);
        memcpy(pu8BufCurr, fu_hdr, sizeof(FU_HEADER));
        pu8BufCurr += sizeof(FU_HEADER);
        memcpy(pu8BufCurr, n->buf, UDP_MAX_SIZE);
        pu8BufCurr += UDP_MAX_SIZE;

        for(packetIndex=2; packetIndex<packetNum; packetIndex++)
        {
            _pRetRtpSplit->ppu8Segment[packetIndex - 1] = pu8BufCurr;
            _pRetRtpSplit->pU32SegmentSize[packetIndex - 1] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE2 + UDP_MAX_SIZE;

            rtsp_intereaved->size = htons(RTP_HEADER_SIZE2 + UDP_MAX_SIZE);

            rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
            rtp_hdr->marker=0;

            fu_ind->F = n->forbidden_bit;
            fu_ind->NRI = n->nal_reference_idc >> 5;
            fu_ind->TYPE = 28;

            fu_hdr->S = 0;
            fu_hdr->E = 0;
            fu_hdr->R = 0;
            fu_hdr->TYPE = n->nal_unit_type;

            memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
            pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
            memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
            pu8BufCurr += sizeof(RTP_FIXED_HEADER);
            memcpy(pu8BufCurr, fu_ind, sizeof(FU_INDICATOR));
            pu8BufCurr += sizeof(FU_INDICATOR);
            memcpy(pu8BufCurr, fu_hdr, sizeof(FU_HEADER));
            pu8BufCurr += sizeof(FU_HEADER);
            memcpy(pu8BufCurr, n->buf + (packetIndex - 1) * UDP_MAX_SIZE, UDP_MAX_SIZE);
            pu8BufCurr += UDP_MAX_SIZE;
        }

        _pRetRtpSplit->ppu8Segment[packetIndex - 1] = pu8BufCurr;
        _pRetRtpSplit->pU32SegmentSize[packetIndex - 1] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_HEADER_SIZE2 + lastPackSize;

        rtsp_intereaved->size = htons(RTP_HEADER_SIZE2 + lastPackSize);

        rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
        rtp_hdr->marker=1;

        fu_ind->F = n->forbidden_bit;
        fu_ind->NRI = n->nal_reference_idc >> 5;
        fu_ind->TYPE = 28;

        fu_hdr->S = 0;
        fu_hdr->E = 1;
        fu_hdr->R = 0;
        fu_hdr->TYPE = n->nal_unit_type;

        memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
        pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
        memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
        pu8BufCurr += sizeof(RTP_FIXED_HEADER);
        memcpy(pu8BufCurr, fu_ind, sizeof(FU_INDICATOR));
        pu8BufCurr += sizeof(FU_INDICATOR);
        memcpy(pu8BufCurr, fu_hdr, sizeof(FU_HEADER));
        pu8BufCurr += sizeof(FU_HEADER);
        memcpy(pu8BufCurr, n->buf + (packetIndex - 1) * UDP_MAX_SIZE, lastPackSize);
        pu8BufCurr += lastPackSize;
    }


    unsigned int u32 = pu8BufCurr - _pRetRtpSplit->pu8Buf;
    CHECK(u32 == _pRetRtpSplit->u32BufSize, -1, "u32=%u, u32BufSize=%u\n", u32, _pRetRtpSplit->u32BufSize);

    return 0;
}

int rtp_free(RTP_SPLIT_S* _pRetRtpSplit)
{
    CHECK(_pRetRtpSplit != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit);
    CHECK(_pRetRtpSplit->pu8Buf != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit->pu8Buf);
    CHECK(_pRetRtpSplit->ppu8Segment != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit->ppu8Segment);
    CHECK(_pRetRtpSplit->pU32SegmentSize != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit->pU32SegmentSize);

    mem_free(_pRetRtpSplit->pu8Buf);
    mem_free(_pRetRtpSplit->ppu8Segment);
    mem_free(_pRetRtpSplit->pU32SegmentSize);

    return 0;
}

int rtp_pcm_alloc(unsigned char* _pu8Frame, unsigned int _u32FrameSize,
                      unsigned short* _pu16SeqNumber, unsigned int _u32Timestamp,
                      RTP_SPLIT_S* _pRetRtpSplit)
{
    CHECK(_pu8Frame != NULL, -1, "invalid parameter with: %#x\n", _pu8Frame);
    CHECK(_pRetRtpSplit != NULL, -1, "invalid parameter with: %#x\n", _pRetRtpSplit);
    
#define RTP_PCM_HEADER_SIZE (sizeof(RTP_FIXED_HEADER)) 
    CHECK(RTP_PCM_HEADER_SIZE == 12, -1, "RTP_PCM_HEADER_SIZE %u\n", RTP_PCM_HEADER_SIZE);

    _pRetRtpSplit->u32SegmentCount = 1;
    _pRetRtpSplit->u32BufSize = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_PCM_HEADER_SIZE + _u32FrameSize;
    _pRetRtpSplit->pu8Buf = (unsigned char*)mem_malloc(_pRetRtpSplit->u32BufSize);
    _pRetRtpSplit->ppu8Segment = (unsigned char**)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned char**));
    _pRetRtpSplit->pU32SegmentSize = (unsigned int*)mem_malloc(_pRetRtpSplit->u32SegmentCount * sizeof(unsigned int*));

    RTSP_INTERLEAVED_FRAME stRtspInterleavedFrame;
    RTP_FIXED_HEADER stRtpFixedHeader;

    memset(&stRtspInterleavedFrame, 0, sizeof(RTSP_INTERLEAVED_FRAME));
    memset(&stRtpFixedHeader, 0, sizeof(RTP_FIXED_HEADER));

    RTSP_INTERLEAVED_FRAME* rtsp_intereaved = &stRtspInterleavedFrame;
    RTP_FIXED_HEADER* rtp_hdr = &stRtpFixedHeader;

    rtsp_intereaved->flag = '$';
    rtsp_intereaved->channel = 2;

    rtp_hdr->version = 2;
    rtp_hdr->marker  = 1;
    rtp_hdr->payload = PCMA;
    rtp_hdr->ssrc    = htonl(10);

    unsigned char* pu8BufCurr = _pRetRtpSplit->pu8Buf;

    _pRetRtpSplit->ppu8Segment[0] = pu8BufCurr;
    _pRetRtpSplit->pU32SegmentSize[0] = sizeof(RTSP_INTERLEAVED_FRAME) + RTP_PCM_HEADER_SIZE + _u32FrameSize;

	rtsp_intereaved->size = htons(RTP_PCM_HEADER_SIZE + _u32FrameSize);
	
    rtp_hdr->seq_no = htons((*_pu16SeqNumber)++);
    rtp_hdr->timestamp = htonl(_u32Timestamp);

    memcpy(pu8BufCurr, rtsp_intereaved, sizeof(RTSP_INTERLEAVED_FRAME));
    pu8BufCurr += sizeof(RTSP_INTERLEAVED_FRAME);
    memcpy(pu8BufCurr, rtp_hdr, sizeof(RTP_FIXED_HEADER));
    pu8BufCurr += sizeof(RTP_FIXED_HEADER);
    memcpy(pu8BufCurr, _pu8Frame, _u32FrameSize);
    pu8BufCurr += _u32FrameSize;

    return 0;
}



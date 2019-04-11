#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <netinet/in.h>

#include "VoiceDecoder.h"

#define ES_NULL 0
#define ES_FALSE 0
#define ES_TRUE 1

#define AUDIO_FRAMES    256

#define TONE_DEVICE             "hw:0"

char audioBuffers[5120000];
int iRead = 0, iPut = 0;
int iPutData = 1;

ESVoid onDecoderStart(ESVoid* cbParam)
{
    // when decoder find start of token, this function will be called by decoder.
    printf("%s\n", "Decoder start");
}

ESVoid onDecoderToken(ESVoid* cbParam, ESInt32 index)
{
    // when decoder find token which freqIndex is index, this function will be called by decoder.
    printf("%c \n", (char)index);
}

ESVoid onDecoderEnd(ESVoid* cbParam, ESInt32 result)
{
    // when decoder recognise finish, this function will be called by decoder.
    iPutData = 0;
    if (result < 0)
    {
        printf("%s \n", "Decoder end, dtc failed!");
    }
    else
    {
        printf("%s \n", "Decoder end, dtc success!");
    }
}

void *doDecoder(void* buffer)
{
    FILE* pAudioFIle;
    pAudioFIle = fopen("/tmp/det_audio0.pcm", "wb");
    VoiceDecoderCallback decoderCallback = { onDecoderStart, onDecoderToken, onDecoderEnd };
    VoiceDecoder* decoder = VoiceDecoder_create("com.sinvoice.for_demo", "SinVoiceDemo", &decoderCallback, ES_NULL);
    if ( ES_NULL != decoder ) {
        if ( VoiceDecoder_start(decoder) ) {
            ESInt16* data = ES_NULL;
            ESUint32 dataLen = 0;

            // we should read record pcm data, len of data is dataLen.
            // 用户自己实现readDataFromRecordBuffer函数
            //ESBool hasData = readDataFromRecordBuffer(&data, &dataLen);
            while ( iPutData == 1 ) {
                // put record data to decoder.
                if(iRead > iPut)
                {
                    printf("iRead: %d iPut: %d \n", iRead, iPut);
                    VoiceDecoder_putData(decoder, (short*)buffer + iPut * 256, 256);
                    //fwrite(buffer + iPut * 512, 512, 1, pAudioFIle);
                    iPut++;
                    if (iPut == 10000)
                    {
                        iPut = 0;
                    }
                } 
                else
                {
                    usleep(1000);
                }               
                //hasData = readDataFromRecordBuffer(&data, &dataLen);
            }

            VoiceDecoder_stop(decoder);
        }

        VoiceDecoder_destroy(decoder);
    }
}

int main(int argc, char *argv[])
{
    int err;
    char *buffer;
    int buffer_frames = AUDIO_FRAMES;
    unsigned int rate = 44100;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;


    if ((err = snd_pcm_open (&capture_handle, TONE_DEVICE, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n",
                TONE_DEVICE, snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "audio interface opened\n");

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "hw_params allocated\n");

    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "hw_params initialized\n");

    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf (stderr, "cannot set access type (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "hw_params access setted\n");

    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
        fprintf (stderr, "cannot set sample format (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "hw_params format setted\n");

    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
        fprintf (stderr, "cannot set sample rate (%s)\n",
                snd_strerror (err));
        exit (1);
    }   
    fprintf(stdout, "hw_params rate setted\n");
 
    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 1)) < 0) {
        fprintf (stderr, "cannot set channel count (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "hw_params channels setted\n");
    
    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot set parameters (%s)\n",
                snd_strerror (err));
        exit (1);
    } 
    fprintf(stdout, "hw_params setted\n");
    
    snd_pcm_hw_params_free (hw_params);
    fprintf(stdout, "hw_params freed\n");

    if ((err = snd_pcm_prepare (capture_handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                snd_strerror (err));
        exit (1);
    }
    fprintf(stdout, "audio interface prepared\n");

    pthread_t id;
    int ret = pthread_create(&id, NULL, &doDecoder, &audioBuffers);

    int buffer_len = buffer_frames * snd_pcm_format_width(format) / 8 * 1;
    buffer = (char *)malloc(buffer_len);
    fprintf(stdout, "buffer allocated %d \n", buffer_len);

    //FILE* pAudioFIle;
    //pAudioFIle = fopen("/tmp/det_audio0.pcm", "wb");

    /*VoiceDecoderCallback decoderCallback = { onDecoderStart, onDecoderToken, onDecoderEnd };
    VoiceDecoder* decoder = VoiceDecoder_create("com.sinvoice.for_demo", "SinVoiceDemo", &decoderCallback, ES_NULL);
    if ( ES_NULL == decoder )
    {       
        printf("%s\n", "decoder create failed!");
        exit(1);
    }
    if ( 0 == VoiceDecoder_start(decoder) )
    {
        printf("%s\n", "decoder start failed!");
        VoiceDecoder_destroy(decoder);
        exit(1);
    }

    printf("==== Starting Recording ====\n");
    FILE* pInputfile;
    pInputfile = fopen("/media/mmcblk0p1/det_audio.pcm", "rb");
    fseek(pInputfile, 0, SEEK_END);
    int iLength = ftell(pInputfile);
    fseek(pInputfile, 0, SEEK_SET);
    printf("%s:%d\n", "filelength", iLength);
    buffer = (char*)malloc(iLength);
    printf("read \n");
    fread(buffer, iLength, sizeof(char), pInputfile);
    printf("%d\n", sizeof(char));

    FILE* pAudioFIle;
    pAudioFIle = fopen("/tmp/det_audio_bak.pcm", "wb");
    fwrite(buffer, iLength/2, sizeof(char), pAudioFIle);
    fclose(pAudioFIle);
    printf("%s\n", "save file success!");
    int iDes = 0;
    while((iLength/2 - iDes) > 256)
    {
        if(1 == VoiceDecoder_putData(decoder, (short*)buffer + iDes, 256))
        {
            //printf("%d:%s\n",iDes,  "putdata success!");
        }
        iDes += 256;
    }*/
    //printf("%d:%s\n",iDes,  "putdata success!");
    /*if(1 == VoiceDecoder_putData(decoder, buffer, iLength/2))
    {
        printf("%s\n", "putdata success!");
    }
    fclose(pInputfile);*/
    //sleep(5000);
    int i = 0;
    while(1)
    {
 start:
           while(buffer_frames > 0)
           {
                err = snd_pcm_readi (capture_handle, buffer, buffer_frames);
                if(err <= 0) {
                    if(err == -EPIPE) 
                    {
                        printf("@@@@@@ overrun!\n");
                        snd_pcm_prepare(capture_handle);
                    }
                    else if(err == -EAGAIN) 
                    {
                        usleep(100);
                        printf("@@@@@ usleep 100....\n");
                        //  snd_pcm_wait(audio->pcm, 1000);
                    }
                    else 
                    {
                        fprintf (stderr, "@@@@@ read from audio interface failed (%d) (%s)\n",
                        err, snd_strerror (err));

                        exit(1);
                    }
                }
                else 
                {
                    //printf("err * sizeof(short)....%d \n", err * sizeof(short));
                    buffer += err * sizeof(short);
                    buffer_frames -= err;
                }        
            }

            if (buffer_frames == 0)
            {
                //printf("AUDIO_FRAMES * sizeof %d \n", AUDIO_FRAMES * sizeof(short));
                buffer_frames = AUDIO_FRAMES;
                buffer -= AUDIO_FRAMES * sizeof(short);
            }
            //printf("copy\n");
            memcpy(audioBuffers + iRead*buffer_len, buffer, buffer_len);
            iRead++;
            if (iRead == 10000)
            {
                iRead = 0;
            }
            // put record data to decoder.
            //printf("%s\n", "startputdata");
            //VoiceDecoder_putData(decoder, (short *)buffer, buffer_len);
            //fwrite(audioBuffers[i-1], buffer_len, 1, pAudioFIle);
    }
out:
    //VoiceDecoder_stop(decoder);
    //VoiceDecoder_destroy(decoder);
    snd_pcm_drain(capture_handle);  
    snd_pcm_close(capture_handle);
    capture_handle = NULL;
    free(buffer);
    buffer = 0;
    printf("%s\n", "end");
    //fclose(pAudioFIle);

    exit(0);
}
#define _CRT_SECURE_NO_WARNINGS
#include "portaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>       //抓時間
#include <string>
#include <conio.h>
#include <windows.h>
#include <vector>       //切割

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
//#define SAMPLE_RATE  (44100)
#define SAMPLE_RATE  (8000)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS     (5)          //預設節目為一分鐘
int minute = 1;
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE   (1)

int time_table[3];     //切割過的時間
int mode = 0,else_s=0;
 /* Select sample format. */
#if 0
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE* recordedSamples;
}
paTestData;

DWORD WINAPI gtThread(LPVOID lpParameter)
{
    time_t now;
    now = time(0);
    printf("現在時間為: %d/%d/%d %02d:%02d\n",1900 + localtime(&now)->tm_year,1 + localtime(&now)->tm_mon , localtime(&now)->tm_mday,
         localtime(&now)->tm_hour, localtime(&now)->tm_min);

    mode = 0;
    int fix_sec = 60 - localtime(&now)->tm_sec;
    int start_h = time_table[0] - localtime(&now)->tm_hour;
    int start_m= time_table[1] - localtime(&now)->tm_min;
    //printf("%d  %d\n", start_h, start_m); 
    while (start_m < 0)    //時間調準
    {
        start_m += 60;
        start_h -= 1;
    }
    if (start_h < 0)
    {
        mode = 2;
    }   
    while (mode == 0)
    {
        /*時間剛好*/
        if (start_h == 0 && start_m == 0)
        {
            printf("start rec\n");
            mode = 1;
            break;
        }
        else
        {
            if(start_h>0)                                                       //顯示剩餘時間
                printf("\r剩餘 %d 小時 %d 分鐘後開始錄音", start_h, start_m);
            else
                printf("\r剩餘 %d 分鐘後開始錄音    ", start_m);
            start_m--;              
            Sleep(fix_sec*1000);                                          //60秒抓一次時間(第一次到下一分鐘不一定為60s)                                  
            fix_sec = 60;
            time_t now = time(0);
            if (start_m < 0)         /*時間校正*/
            {
                start_m += 60;
                start_h -= 1; 
            }
        }
    }
    return 0;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    paTestData* data = (paTestData*)userData;
    const SAMPLE* rptr = (const SAMPLE*)inputBuffer;
    SAMPLE* wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void)outputBuffer; /* Prevent unused variable warnings. */
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (framesLeft < framesPerBuffer)
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL)
    {
        for (i = 0; i < framesToCalc; i++)
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for (i = 0; i < framesToCalc; i++)
        {
            *wptr++ = *rptr++;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

/*******************************************************************/
//int main(void);
int main(void)
{
    int count,check=0;
    int wr_count = 1;
    printf("錄影機啟動中......\n");
    printf("錄影機錄影節目一次, 以分鐘為準, 最少為一分鐘, 最多為60分鐘\n");
    printf("請輸入預錄節目之開始時間及錄製時間(24小時制 請以空格分開):\n");
    scanf("%d %d %d",&time_table[0],&time_table[1],&time_table[2]);      //小時  分鐘  時間長度
    printf("預設錄影開始時間為 %d 時 %d 分.\n", time_table[0], time_table[1]);
    minute = time_table[2];
    HANDLE get_time = CreateThread(0, 0, gtThread, 0, 0, 0);

    PaStreamParameters  inputParameters,
        outputParameters;
    PaStream* stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;
    int a = 0;

    char *WR_file_name=(char*)malloc(8*sizeof(char));
    WR_file_name[0] = (time_table[0] / 10) + '0';
    WR_file_name[1] = (time_table[0] % 10) + '0';
    WR_file_name[2] = (time_table[1] / 10) + '0';
    WR_file_name[3] = (time_table[1] % 10) + '0';
    WR_file_name[4] = '.';
    WR_file_name[5] = 'w';
    WR_file_name[6] = 'a';
    WR_file_name[7] = 'v';
    WR_file_name[8] = '\0';
    //printf("patest_record.c\n"); 
    fflush(stdout);
    int seconds = NUM_SECONDS*minute;
    data.maxFrameIndex = totalFrames = seconds * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE*)malloc(numBytes); /* From now on, recordedSamples is initialised. */
    if (data.recordedSamples == NULL)
    {
        printf("Could not allocate record array.\n");
        goto done;
    }
    for (i = 0; i < numSamples; i++) data.recordedSamples[i] = 0;

    err = Pa_Initialize();
    if (err != paNoError) goto done;
    
    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        NULL,                  /* &outputParameters, */
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
        recordCallback,
        &data);
    if (err != paNoError) goto done;

    while (mode!=4)
    {
        switch (mode)
        {
            case 0:                     //時間未到
            {
                Sleep(1000);
            }break;
            case 1:                     //時間到了
            {
                err = Pa_StartStream(stream);
                if (err != paNoError) goto done;
                printf("\n=== Now recording!! Please speak into the microphone. ===\n"); fflush(stdout);

                while ((err = Pa_IsStreamActive(stream)) == 1)
                {
                    Pa_Sleep(1000);
                    printf("index = %d\n", data.frameIndex); fflush(stdout);
                }
                if (err < 0) goto done;
                mode = 3;
            }break;
            case 2:                        //超過時間
            {
                printf("已超過時間\n");
                mode = 4;
            }break;
            case 3:                         //write to .wav
            {
                FILE* fid;
                printf("%s", WR_file_name);
                fid = fopen(WR_file_name, "wb");

                if (fid == NULL)
                {
                    printf("Could not open file.");
                }
                else
                {
                    char skr;
                    int chunk1size = 0x10;
                    int PCM = 0x01;
                    int num_channel = 2;
                    int sam_rate = 8000;
                    int byterate = (int)SAMPLE_RATE * NUM_CHANNELS * sizeof(SAMPLE);
                    int block = NUM_CHANNELS * sizeof(SAMPLE);
                    int bitsample = sizeof(SAMPLE) * 8;
                    int chunk2size = (int)SAMPLE_RATE * seconds * NUM_CHANNELS * sizeof(SAMPLE);
                    int chunksize = 36 + chunk2size;

                    /*  Header  */
                    fwrite("RIFF", 4, 1, fid);                          //"RIFF"                0~3
                    fwrite(&chunksize, 4, 1, fid);                         //chunkSize             4~7 
                    fwrite("WAVE", 4, 1, fid);                          //格式(.wave)           8~11
                    fwrite("fmt ", 4, 1, fid);                          //"fmt" ""(有空格歐)    12~15
                    fwrite(&chunk1size, 4, 1, fid);                     //16                    16~19
                    fwrite(&PCM, sizeof(skr) * 2, 1, fid);                //PCM                   20~21
                    fwrite(&num_channel, sizeof(skr) * 2, 1, fid);        //聲道數                22~23
                    fwrite(&sam_rate, sizeof(skr) * 4, 1, fid);         //samplerate            24~27
                    fwrite(&byterate, sizeof(skr) * 4, 1, fid);           //byterate              28~31
                    fwrite(&block, sizeof(skr) * 2, 1, fid);              //block                 32~33
                    fwrite(&bitsample, sizeof(skr) * 2, 1, fid);          //rate                  34~35
                    fwrite("data", 4, 1, fid);                          //adta                  36~39
                    fwrite(&chunk2size, 4, 1, fid);                     //chunk2Size            40~43 

                    /*data*/
                    fwrite(data.recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), totalFrames, fid);
                    fclose(fid);
                    printf("Wrote data to '%s'\n", WR_file_name);
                }
                mode = 4;
            }break;
            case 4:
            {
            }break;
        }
    }

    CloseHandle(get_time);
    err = Pa_CloseStream(stream);
    if (err != paNoError) goto done;


done:
    Pa_Terminate();
    if (data.recordedSamples)       /* Sure it is NULL or valid. */
        free(data.recordedSamples);
    if (err != paNoError)
    {
        fprintf(stderr, "An error occurred while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}
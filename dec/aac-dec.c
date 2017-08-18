#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "aacdecoder_lib.h"

#define DECODER_BUFFSIZE        2048 * 8
#define READ_BUFFSIZE           1048 * 8
#define TAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

struct RIFF_HEADER
{
    unsigned int szRiffID;     // 'R','I','F','F'
    unsigned int dwRiffSize;
    unsigned int szRiffFormat; // 'W','A','V','E'
};

struct WAVE_FORMAT
{
    unsigned short wFormatTag;
    unsigned short wChannels;
    unsigned int dwSamplesPerSec;
    unsigned int dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;
};

struct FMT_BLOCK
{
    unsigned int szFmtID;    // 'f','m','t',' '
    unsigned int dwFmtSize;     // 16
    struct WAVE_FORMAT wavFormat;
};

struct DATA_BLOCK
{
    unsigned int szDataID;   // 'd','a','t','a'
    unsigned int dwDataSize;
};

struct wav_type
{
    struct RIFF_HEADER rhead;
    struct FMT_BLOCK fmtb;
    struct DATA_BLOCK datb;
};

unsigned char decoder_buffer[DECODER_BUFFSIZE] = {0};
unsigned char aacbuf[READ_BUFFSIZE] = {0};
struct wav_type wav_head;

void usage(const char* name)
{
    fprintf(stderr, "%s [-m transport] in.aac out.wav\n", name);
    fprintf(stderr, "Supported transport:\n");
    fprintf(stderr, "\t2\tADTS\n");
}

int wav_head_init(void)
{
    wav_head.rhead.szRiffID = TAG('R','I','F','F');
    wav_head.rhead.dwRiffSize = 44-8;
    wav_head.rhead.szRiffFormat = TAG('W','A','V','E');
    wav_head.fmtb.szFmtID = TAG('f','m','t',' ');
    wav_head.fmtb.dwFmtSize = 16;
    wav_head.fmtb.wavFormat.wFormatTag = 1;
    wav_head.datb.szDataID = TAG('d','a','t','a');
    return 0;
}

int get_wav_head(int ch, int srate, unsigned int pcm_total)
{
    wav_head.rhead.dwRiffSize += pcm_total;
    wav_head.datb.dwDataSize = pcm_total;
    wav_head.fmtb.wavFormat.wChannels = ch;
    wav_head.fmtb.wavFormat.dwSamplesPerSec = srate;
    wav_head.fmtb.wavFormat.dwAvgBytesPerSec = ch * srate * 2;
    wav_head.fmtb.wavFormat.wBlockAlign = ch * 2;
    wav_head.fmtb.wavFormat.wBitsPerSample = 16;
    return 0;
}

int fdk_decode_audio(HANDLE_AACDECODER phandle, INT_PCM *output_buf, int *output_size, unsigned char *buffer, int size)
{
    int ret = 0;
    UINT pkt_size = size;
    CStreamInfo *aac_stream_info = NULL;
    UINT valid_size = size;
    UCHAR *input_buf[1] = {buffer};
    if (size > 0)
    {
        /* step 1 -> fill aac_data_buf to decoder's internal buf */
        ret = aacDecoder_Fill(phandle, input_buf, &pkt_size, &valid_size);
        if (ret != AAC_DEC_OK)
        {
            *output_size  = 0;
            return 0;
        }
    }

    /* step 2 -> call decoder function */
    ret = aacDecoder_DecodeFrame(phandle, output_buf, DECODER_BUFFSIZE/sizeof(INT_PCM), 0);
    if (ret == AAC_DEC_NOT_ENOUGH_BITS)
    {
        *output_size  = 0;
    }
    if (ret != AAC_DEC_OK)
    {
        *output_size  = 0;
        return 0;
    }

    aac_stream_info = aacDecoder_GetStreamInfo(phandle);
    if (aac_stream_info == NULL)
    {
        *output_size = DECODER_BUFFSIZE/sizeof(INT_PCM);
    }
    else
    {
        *output_size = aac_stream_info->frameSize * aac_stream_info->numChannels * 2;
    }
    /* return aac decode size */
    return (size - valid_size);
}

int main(int argc, char *argv[])
{
    TRANSPORT_TYPE transportFmt = TT_MP4_ADTS;
    HANDLE_AACDECODER dec_handle = NULL;
    CStreamInfo *aac_stream_info = NULL;
    int ch;
    const char *infile, *outfile;
    unsigned int rlen = 0;
    unsigned int package_len = 0, pcm_total = 0;
    int aac_used = 0, pcm_size = 0;
    unsigned char * paacbuf = (unsigned char *)&aacbuf;
    void *wav_h = (void*)&wav_head;
    FILE *inf = 0;
    FILE *outf = 0;
    while ((ch = getopt(argc, argv, "m:")) != -1)
    {
        switch (ch)
        {
            case 'm':
                transportFmt = (TRANSPORT_TYPE)atoi(optarg);
                break;
            case '?':
            default:
                usage(argv[0]);
                return 1;
        }
    }
    if (argc - optind < 2) {
        usage(argv[0]);
        return 1;
    }
    
    infile = argv[optind];
    outfile = argv[optind + 1];

    if ((inf = fopen(infile, "rb")) == 0)
    {
        printf("infile open faild!\n");
        return 1;
    }

    if ((outf = fopen(outfile, "wb")) == 0)
    {
        printf("outfile open faild!\n");
        fclose(inf);
        return 1;
    }

	dec_handle = aacDecoder_Open(transportFmt, 1);

	if (dec_handle == NULL)
	{
		printf("aacDecoder open faild!\n");
        fclose(inf);
        fclose(outf);
		exit(0);
	}
    wav_head_init();

    fwrite((unsigned char*)wav_h,1,sizeof(wav_head),outf);
    while (1)
    {
        rlen = fread(aacbuf, 1, READ_BUFFSIZE, inf);
        paacbuf = (unsigned char *)&aacbuf;
        package_len = rlen;
        while(rlen > 0)
        {
            if (package_len > 0)
            {
                package_len = package_len - aac_used;
            }
            aac_used = fdk_decode_audio(dec_handle, (INT_PCM *)decoder_buffer, &pcm_size, paacbuf, package_len);
            paacbuf = paacbuf + aac_used;
            if ( pcm_size > 0 && package_len < rlen )
            {
                fwrite(decoder_buffer,1,pcm_size,outf);
                pcm_total += pcm_size;
            }
            else if ( pcm_size == 0 )
            {
                break;
            }
        }
        if (rlen < READ_BUFFSIZE)
        {
            break;
        }
    }
    
    aac_stream_info = aacDecoder_GetStreamInfo(dec_handle);
    get_wav_head(aac_stream_info->numChannels, aac_stream_info->aacSampleRate, pcm_total);
    fseek(outf, 0, SEEK_SET);
    fwrite((unsigned char*)wav_h,1,sizeof(wav_head),outf);
    
	aacDecoder_Close(dec_handle);
    fclose(inf);
    fclose(outf);
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "wavreader.h"

void usage(const char *name)
{
	fprintf(stderr, "%s in.wav out.pcm\n", name);
}

void swap_word(unsigned char *buf, int size)
{
	float db = 15;
	int i = 0;
	short wd = 0;
	int _wd = 0;
	for (i = 0; i < size; i+=2)
	{
		wd=(short)((buf[i+1]<<8) + buf[i]);
		_wd = (int)wd * db;
		if (_wd < -32768)
		{
			_wd = -32768;
		}
		else if (_wd > 32767)
		{
			_wd = 32767;
		}
		wd = (short)_wd;
		buf[i+1] = (wd&0xff00) >> 8;
		buf[i] = (wd&0x00ff);
	}
}

int main(int argc, char const *argv[])
{
	const char *infile, *outfile;
	FILE *out;
	void *wav;
	unsigned char buffer[2048] = {0};
	int len = 0;
	int format, sample_rate, channels, bits_per_sample;
	if (argc < 2)
	{
		usage(argv[0]);
		return 1; 
	}

	infile = argv[1];
	outfile = argv[2];
	fprintf(stderr, "%s %s %s\n", argv[0], infile, outfile);
	wav = wav_read_open(infile);
	if (!wav_get_header(wav, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
	    fprintf(stderr, "Bad wav file %s\n", infile);
	    return 1;
    }
    printf("%d %d %d %d\n", channels, format, sample_rate, bits_per_sample);
	out = fopen(outfile, "wb");
	len = wav_read_data(wav, buffer, 2048);
	while(len == 2048)
	{
		swap_word(buffer, len);
		fwrite(buffer, 1, len, out);
		len = wav_read_data(wav, buffer, 2048);
	}
	if (len > 0)
	{
		swap_word(buffer, len);
		fwrite(buffer, 1, len, out);
	}
	fclose(out);
	wav_read_close(wav);
	return 0;
}

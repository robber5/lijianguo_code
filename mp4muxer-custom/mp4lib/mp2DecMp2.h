#ifndef __MP2DEC_MP2_H__
#define __MP2DEC_MP2_H__

#ifdef __cplusplus
extern "C" {
#endif

	/* max frame size, in samples */
#define MPA_FRAME_SIZE 1152 
	/* max compressed frame size */
#define MPA_MAX_CODED_FRAME_SIZE 1792
	
#define MPA_MAX_CHANNELS 2
	
#define SBLIMIT 32 /* number of subbands */
	
#define MPA_STEREO  0
#define MPA_JSTEREO 1
#define MPA_DUAL    2
#define MPA_MONO    3
#define M_PI    3.14159265358979323846
	
#define FF_AA_INT     2
	
#define int64_t_C(c)     (c ## L)
	
	
typedef struct tagAVCodecContext
{
	int frame_size;
	void *priv_data;
	int antialias_algo;
	int channels;
	int bit_rate;
	int sub_id;
	int sample_rate;
	int parse_only;
}AVCodecContext;


#define FRAC_BITS   15   /* fractional bits for sb_samples and dct */
#define WFRAC_BITS  14   /* fractional bits for window */

#define FRAC_ONE    (1 << FRAC_BITS)

#define MULL(a,b) (((signed long long)(a) * (signed long long)(b)) >> FRAC_BITS)
#define MUL64(a,b) ((signed long long)(a) * (signed long long)(b))
#define FIX(a)   ((int)((a) * FRAC_ONE))
/* WARNING: only correct for posititive numbers */
#define FIXR(a)   ((int)((a) * FRAC_ONE + 0.5))
#define FRAC_RND(a) (((a) + (FRAC_ONE/2)) >> FRAC_BITS)

#if FRAC_BITS <= 15
typedef signed short MPA_INT;
#else
typedef signed int MPA_INT;
#endif

#define HEADER_SIZE 4
#define BACKSTEP_SIZE 512

struct GranuleDef;

typedef struct MPADecodeContext 
{
    unsigned char inbuf1[2][MPA_MAX_CODED_FRAME_SIZE + BACKSTEP_SIZE];	/* input buffer */
    int inbuf_index;
    unsigned char *inbuf_ptr, *inbuf;
    int frame_size;
    int free_format_frame_size; /* frame size in case of free format
                                   (zero if currently unknown) */
    /* next header (used in free format parsing) */
    unsigned int free_format_next_header; 
    int error_protection;
    int layer;
    int sample_rate;
    int sample_rate_index; /* between 0 and 8 */
    int bit_rate;
    int old_frame_size;
    GetBitContext gb;
    int nb_channels;
    int mode;
    int mode_ext;
    int lsf;
    MPA_INT synth_buf[MPA_MAX_CHANNELS][512 * 2];
    int synth_buf_offset[MPA_MAX_CHANNELS];
    signed int sb_samples[MPA_MAX_CHANNELS][36][SBLIMIT];
    signed int mdct_buf[MPA_MAX_CHANNELS][SBLIMIT * 18]; /* previous samples, for layer 3 MDCT */
    void (*compute_antialias)(struct MPADecodeContext *s, struct GranuleDef *g);
} MPADecodeContext;

/* layer 3 "granule" */
typedef struct GranuleDef 
{
    unsigned char scfsi;
    int part2_3_length;
    int big_values;
    int global_gain;
    int scalefac_compress;
    unsigned char block_type;
    unsigned char switch_point;
    int table_select[3];
    int subblock_gain[3];
    unsigned char scalefac_scale;
    unsigned char count1table_select;
    int region_size[3]; /* number of huffman codes in each region */
    int preflag;
    int short_start, long_end; /* long/short band indexes */
    unsigned char scale_factors[40];
    signed int sb_hybrid[SBLIMIT * 18]; /* 576 samples */
} GranuleDef;

#define MODE_EXT_MS_STEREO 2
#define MODE_EXT_I_STEREO  1

#ifdef __cplusplus
}
#endif

#endif

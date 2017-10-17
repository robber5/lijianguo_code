#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mp2DecTypedef.h"
#include "mp2DecBits.h"
#include "mp2DecMp2.h"
#include "libMp2Dec.h"

static const unsigned short mpa_bitrate_tab[2][3][15] = {
    { {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
      {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
      {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } },
    { {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
      {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
      {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
    }
};

static const unsigned short mpa_freq_tab[3] = { 44100, 48000, 32000 };

/*******************************************************/
/* half mpeg encoding window (full precision) */
static const signed int mpa_enwindow[257] = {
     0,    -1,    -1,    -1,    -1,    -1,    -1,    -2,
    -2,    -2,    -2,    -3,    -3,    -4,    -4,    -5,
    -5,    -6,    -7,    -7,    -8,    -9,   -10,   -11,
   -13,   -14,   -16,   -17,   -19,   -21,   -24,   -26,
   -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53,
   -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,
  -104,  -111,  -117,  -125,  -132,  -139,  -147,  -154,
  -161,  -169,  -176,  -183,  -190,  -196,  -202,  -208,
   213,   218,   222,   225,   227,   228,   228,   227,
   224,   221,   215,   208,   200,   189,   177,   163,
   146,   127,   106,    83,    57,    29,    -2,   -36,
   -72,  -111,  -153,  -197,  -244,  -294,  -347,  -401,
  -459,  -519,  -581,  -645,  -711,  -779,  -848,  -919,
  -991, -1064, -1137, -1210, -1283, -1356, -1428, -1498,
 -1567, -1634, -1698, -1759, -1817, -1870, -1919, -1962,
 -2001, -2032, -2057, -2075, -2085, -2087, -2080, -2063,
  2037,  2000,  1952,  1893,  1822,  1739,  1644,  1535,
  1414,  1280,  1131,   970,   794,   605,   402,   185,
   -45,  -288,  -545,  -814, -1095, -1388, -1692, -2006,
 -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
 -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597,
 -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585,
 -9727, -9838, -9916, -9959, -9966, -9935, -9863, -9750,
 -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134,
  6574,  5959,  5288,  4561,  3776,  2935,  2037,  1082,
    70,  -998, -2122, -3300, -4533, -5818, -7154, -8540,
 -9975,-11455,-12980,-14548,-16155,-17799,-19478,-21189,
-22929,-24694,-26482,-28289,-30112,-31947,-33791,-35640,
-37489,-39336,-41176,-43006,-44821,-46617,-48390,-50137,
-51853,-53534,-55178,-56778,-58333,-59838,-61289,-62684,
-64019,-65290,-66494,-67629,-68692,-69679,-70590,-71420,
-72169,-72835,-73415,-73908,-74313,-74630,-74856,-74992,
 75038,
};

/*******************************************************/
/* layer 2 tables */

static const int sblimit_table[5] = { 27 , 30 , 8, 12 , 30 };

static const int quant_steps[17] = {
    3,     5,    7,    9,    15,
    31,    63,  127,  255,   511,
    1023,  2047, 4095, 8191, 16383,
    32767, 65535
};

/* we use a negative value if grouped */
static const int quant_bits[17] = {
    -5,  -7,  3, -10, 4, 
     5,  6,  7,  8,  9,
    10, 11, 12, 13, 14,
    15, 16 
};

/* encoding tables which give the quantization index. Note how it is
   possible to store them efficiently ! */
static const unsigned char alloc_table_0[] = {
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
};

static const unsigned char alloc_table_1[] = {
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 3,  0,  1,  2,  3,  4,  5, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
 2,  0,  1, 16, 
};

static const unsigned char alloc_table_2[] = {
 4,  0,  1,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
 4,  0,  1,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
};

static const unsigned char alloc_table_3[] = {
 4,  0,  1,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
 4,  0,  1,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
};

static const unsigned char alloc_table_4[] = {
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
 4,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 3,  0,  1,  3,  4,  5,  6,  7, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
 2,  0,  1,  3, 
};

static const unsigned char *alloc_tables[5] = 
{ alloc_table_0, alloc_table_1, alloc_table_2, alloc_table_3, alloc_table_4, };

/* band size tables */
static const unsigned char band_size_long[9][22] = {
{ 4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 10,
  12, 16, 20, 24, 28, 34, 42, 50, 54, 76, 158, }, /* 44100 */
{ 4, 4, 4, 4, 4, 4, 6, 6, 6, 8, 10,
  12, 16, 18, 22, 28, 34, 40, 46, 54, 54, 192, }, /* 48000 */
{ 4, 4, 4, 4, 4, 4, 6, 6, 8, 10, 12,
  16, 20, 24, 30, 38, 46, 56, 68, 84, 102, 26, }, /* 32000 */
{ 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16,
  20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, }, /* 22050 */
{ 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16,
  18, 22, 26, 32, 38, 46, 52, 64, 70, 76, 36, }, /* 24000 */
{ 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16,
  20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, }, /* 16000 */
{ 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16,
  20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, }, /* 11025 */
{ 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16,
  20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, }, /* 12000 */
{ 12, 12, 12, 12, 12, 12, 16, 20, 24, 28, 32,
  40, 48, 56, 64, 76, 90, 2, 2, 2, 2, 2, }, /* 8000 */
};

static const unsigned char band_size_short[9][13] = {
{ 4, 4, 4, 4, 6, 8, 10, 12, 14, 18, 22, 30, 56, }, /* 44100 */
{ 4, 4, 4, 4, 6, 6, 10, 12, 14, 16, 20, 26, 66, }, /* 48000 */
{ 4, 4, 4, 4, 6, 8, 12, 16, 20, 26, 34, 42, 12, }, /* 32000 */
{ 4, 4, 4, 6, 6, 8, 10, 14, 18, 26, 32, 42, 18, }, /* 22050 */
{ 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 32, 44, 12, }, /* 24000 */
{ 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18, }, /* 16000 */
{ 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18, }, /* 11025 */
{ 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18, }, /* 12000 */
{ 8, 8, 8, 12, 16, 20, 24, 28, 36, 2, 2, 2, 26, }, /* 8000 */
};

static const unsigned char mpa_pretab[2][22] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0 },
};

/* table for alias reduction (XXX: store it as integer !) */
static const float ci_table[8] = {
    -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037,
};



//static unsigned short band_index_long[9][23];
/* XXX: free when all decoders are closed */
#define TABLE_4_3_SIZE (8191 + 16)
//static signed char  *table_4_3_exp;
#if FRAC_BITS <= 15
//static unsigned short *table_4_3_value;
#else
static unsigned int *table_4_3_value;
#endif
/* intensity stereo coef table */
//static signed int is_table[2][16];
//static signed int is_table_lsf[2][2][16];
static signed int csa_table[8][4];
static float csa_table_float[8][4];
//static signed int mdct_win[8][36];

/* lower 2 bits: modulo 3, higher bits: shift */
static unsigned short scale_factor_modshift[64];
/* [i][j]:  2^(-j/3) * FRAC_ONE * 2^(i+2) / (2^(i+2) - 1) */
static signed int scale_factor_mult[15][3];
/* mult table for layer 2 group quantization */

#define SCALE_GEN(v) \
{ FIXR(1.0 * (v)), FIXR(0.7937005259 * (v)), FIXR(0.6299605249 * (v)) }

static signed int scale_factor_mult2[3][3] = {
    SCALE_GEN(4.0 / 3.0), /* 3 steps */
    SCALE_GEN(4.0 / 5.0), /* 5 steps */
    SCALE_GEN(4.0 / 9.0), /* 9 steps */
};

/* 2^(n/4) */
/*static unsigned int scale_factor_mult3[4] = {
    FIXR(1.0),
    FIXR(1.18920711500272106671),
    FIXR(1.41421356237309504880),
    FIXR(1.68179283050742908605),
};*/

static MPA_INT window[512];
    
/* layer 1 unscaling */
/* n = number of bits of the mantissa minus 1 */
static int l1_unscale(int n, int mant, int scale_factor)
{
    int shift, mod;
    signed long long val;

    shift = scale_factor_modshift[scale_factor];
    mod = shift & 3;
    shift >>= 2;
    val = MUL64(mant + (-1 << n) + 1, scale_factor_mult[n-1][mod]);
    shift += n;
    /* NOTE: at this point, 1 <= shift >= 21 + 15 */
    return (int)((val + (1L << (shift - 1))) >> shift);
}

static int l2_unscale_group(int steps, int mant, int scale_factor)
{
    int shift, mod, val;

    shift = scale_factor_modshift[scale_factor];
    mod = shift & 3;
    shift >>= 2;

    val = (mant - (steps >> 1)) * scale_factor_mult2[steps >> 2][mod];
    /* NOTE: at this point, 0 <= shift <= 21 */
    if (shift > 0)
        val = (val + (1 << (shift - 1))) >> shift;
    return val;
}


/* all integer n^(4/3) computation code */
#define DEV_ORDER 13


//static int dev_4_3_coefs[DEV_ORDER];



HMP2DEC MP2_decode_init()
{
	AVCodecContext *avctx;
    MPADecodeContext *s;
    static int init=0;
    int i;
	
	avctx = (AVCodecContext*)malloc(sizeof(AVCodecContext));
	if(avctx == NULL)
	{
		return NULL;
	}
	memset(avctx, 0, sizeof(AVCodecContext));
	avctx->priv_data = malloc(sizeof(MPADecodeContext));
	if(avctx->priv_data == NULL)
	{
		free(avctx);
		return NULL;
	}
	memset(avctx->priv_data, 0, sizeof(MPADecodeContext));

	s = (MPADecodeContext *)avctx->priv_data;
    if (!init && !avctx->parse_only) 
	{
        /* scale factors table for layer 1/2 */
        for(i=0;i<64;i++) {
            int shift, mod;
            /* 1.0 (i = 3) is normalized to 2 ^ FRAC_BITS */
            shift = (i / 3);
            mod = i % 3;
            scale_factor_modshift[i] = mod | (shift << 2);
        }

        /* scale factor multiply for layer 1 */
        for(i=0;i<15;i++) 
		{
            int n, norm;
            n = i + 2;
            norm = ((int64_t_C(1) << n) * FRAC_ONE) / ((1 << n) - 1);
            scale_factor_mult[i][0] = MULL(FIXR(1.0 * 2.0), norm);
            scale_factor_mult[i][1] = MULL(FIXR(0.7937005259 * 2.0), norm);
            scale_factor_mult[i][2] = MULL(FIXR(0.6299605249 * 2.0), norm);
        }
        
        /* window */
        /* max = 18760, max sum over all 16 coefs : 44736 */
        for(i=0;i<257;i++) {
            int v;
            v = mpa_enwindow[i];
#if WFRAC_BITS < 16
            v = (v + (1 << (16 - WFRAC_BITS - 1))) >> (16 - WFRAC_BITS);
#endif
            window[i] = v;
            if ((i & 63) != 0)
                v = -v;
            if (i != 0)
                window[512 - i] = v;
        }
        

        for(i=0;i<8;i++) 
		{
            float ci, cs, ca;
            ci = ci_table[i];
            cs = 1.0 / sqrt(1.0 + ci * ci);
            ca = cs * ci;
            csa_table[i][0] = FIX(cs);
            csa_table[i][1] = FIX(ca);
            csa_table[i][2] = FIX(ca) + FIX(cs);
            csa_table[i][3] = FIX(ca) - FIX(cs); 
            csa_table_float[i][0] = cs;
            csa_table_float[i][1] = ca;
            csa_table_float[i][2] = ca + cs;
            csa_table_float[i][3] = ca - cs; 
        }

       init = 1;
    }

    s->inbuf_index = 0;
    s->inbuf = &s->inbuf1[s->inbuf_index][BACKSTEP_SIZE];
    s->inbuf_ptr = s->inbuf;

    return (HMP2DEC)avctx;
}


#define COS0_0  FIXR(0.50060299823519630134)
#define COS0_1  FIXR(0.50547095989754365998)
#define COS0_2  FIXR(0.51544730992262454697)
#define COS0_3  FIXR(0.53104259108978417447)
#define COS0_4  FIXR(0.55310389603444452782)
#define COS0_5  FIXR(0.58293496820613387367)
#define COS0_6  FIXR(0.62250412303566481615)
#define COS0_7  FIXR(0.67480834145500574602)
#define COS0_8  FIXR(0.74453627100229844977)
#define COS0_9  FIXR(0.83934964541552703873)
#define COS0_10 FIXR(0.97256823786196069369)
#define COS0_11 FIXR(1.16943993343288495515)
#define COS0_12 FIXR(1.48416461631416627724)
#define COS0_13 FIXR(2.05778100995341155085)
#define COS0_14 FIXR(3.40760841846871878570)
#define COS0_15 FIXR(10.19000812354805681150)

#define COS1_0 FIXR(0.50241928618815570551)
#define COS1_1 FIXR(0.52249861493968888062)
#define COS1_2 FIXR(0.56694403481635770368)
#define COS1_3 FIXR(0.64682178335999012954)
#define COS1_4 FIXR(0.78815462345125022473)
#define COS1_5 FIXR(1.06067768599034747134)
#define COS1_6 FIXR(1.72244709823833392782)
#define COS1_7 FIXR(5.10114861868916385802)

#define COS2_0 FIXR(0.50979557910415916894)
#define COS2_1 FIXR(0.60134488693504528054)
#define COS2_2 FIXR(0.89997622313641570463)
#define COS2_3 FIXR(2.56291544774150617881)

#define COS3_0 FIXR(0.54119610014619698439)
#define COS3_1 FIXR(1.30656296487637652785)

#define COS4_0 FIXR(0.70710678118654752439)

/* butterfly operator */
#define BF(a, b, c)\
{\
    tmp0 = tab[a] + tab[b];\
    tmp1 = tab[a] - tab[b];\
    tab[a] = tmp0;\
    tab[b] = MULL(tmp1, c);\
}

#define BF1(a, b, c, d)\
{\
    BF(a, b, COS4_0);\
    BF(c, d, -COS4_0);\
    tab[c] += tab[d];\
}

#define BF2(a, b, c, d)\
{\
    BF(a, b, COS4_0);\
    BF(c, d, -COS4_0);\
    tab[c] += tab[d];\
    tab[a] += tab[c];\
    tab[c] += tab[b];\
    tab[b] += tab[d];\
}

#define ADD(a, b) tab[a] += tab[b]

/* DCT32 without 1/sqrt(2) coef zero scaling. */
static void dct32(signed int *out, signed int *tab)
{
    int tmp0, tmp1;

    /* pass 1 */
    BF(0, 31, COS0_0);
    BF(1, 30, COS0_1);
    BF(2, 29, COS0_2);
    BF(3, 28, COS0_3);
    BF(4, 27, COS0_4);
    BF(5, 26, COS0_5);
    BF(6, 25, COS0_6);
    BF(7, 24, COS0_7);
    BF(8, 23, COS0_8);
    BF(9, 22, COS0_9);
    BF(10, 21, COS0_10);
    BF(11, 20, COS0_11);
    BF(12, 19, COS0_12);
    BF(13, 18, COS0_13);
    BF(14, 17, COS0_14);
    BF(15, 16, COS0_15);

    /* pass 2 */
    BF(0, 15, COS1_0);
    BF(1, 14, COS1_1);
    BF(2, 13, COS1_2);
    BF(3, 12, COS1_3);
    BF(4, 11, COS1_4);
    BF(5, 10, COS1_5);
    BF(6,  9, COS1_6);
    BF(7,  8, COS1_7);
    
    BF(16, 31, -COS1_0);
    BF(17, 30, -COS1_1);
    BF(18, 29, -COS1_2);
    BF(19, 28, -COS1_3);
    BF(20, 27, -COS1_4);
    BF(21, 26, -COS1_5);
    BF(22, 25, -COS1_6);
    BF(23, 24, -COS1_7);
    
    /* pass 3 */
    BF(0, 7, COS2_0);
    BF(1, 6, COS2_1);
    BF(2, 5, COS2_2);
    BF(3, 4, COS2_3);
    
    BF(8, 15, -COS2_0);
    BF(9, 14, -COS2_1);
    BF(10, 13, -COS2_2);
    BF(11, 12, -COS2_3);
    
    BF(16, 23, COS2_0);
    BF(17, 22, COS2_1);
    BF(18, 21, COS2_2);
    BF(19, 20, COS2_3);
    
    BF(24, 31, -COS2_0);
    BF(25, 30, -COS2_1);
    BF(26, 29, -COS2_2);
    BF(27, 28, -COS2_3);

    /* pass 4 */
    BF(0, 3, COS3_0);
    BF(1, 2, COS3_1);
    
    BF(4, 7, -COS3_0);
    BF(5, 6, -COS3_1);
    
    BF(8, 11, COS3_0);
    BF(9, 10, COS3_1);
    
    BF(12, 15, -COS3_0);
    BF(13, 14, -COS3_1);
    
    BF(16, 19, COS3_0);
    BF(17, 18, COS3_1);
    
    BF(20, 23, -COS3_0);
    BF(21, 22, -COS3_1);
    
    BF(24, 27, COS3_0);
    BF(25, 26, COS3_1);
    
    BF(28, 31, -COS3_0);
    BF(29, 30, -COS3_1);
    
    /* pass 5 */
    BF1(0, 1, 2, 3);
    BF2(4, 5, 6, 7);
    BF1(8, 9, 10, 11);
    BF2(12, 13, 14, 15);
    BF1(16, 17, 18, 19);
    BF2(20, 21, 22, 23);
    BF1(24, 25, 26, 27);
    BF2(28, 29, 30, 31);
    
    /* pass 6 */
    
    ADD( 8, 12);
    ADD(12, 10);
    ADD(10, 14);
    ADD(14,  9);
    ADD( 9, 13);
    ADD(13, 11);
    ADD(11, 15);

    out[ 0] = tab[0];
    out[16] = tab[1];
    out[ 8] = tab[2];
    out[24] = tab[3];
    out[ 4] = tab[4];
    out[20] = tab[5];
    out[12] = tab[6];
    out[28] = tab[7];
    out[ 2] = tab[8];
    out[18] = tab[9];
    out[10] = tab[10];
    out[26] = tab[11];
    out[ 6] = tab[12];
    out[22] = tab[13];
    out[14] = tab[14];
    out[30] = tab[15];
    
    ADD(24, 28);
    ADD(28, 26);
    ADD(26, 30);
    ADD(30, 25);
    ADD(25, 29);
    ADD(29, 27);
    ADD(27, 31);

    out[ 1] = tab[16] + tab[24];
    out[17] = tab[17] + tab[25];
    out[ 9] = tab[18] + tab[26];
    out[25] = tab[19] + tab[27];
    out[ 5] = tab[20] + tab[28];
    out[21] = tab[21] + tab[29];
    out[13] = tab[22] + tab[30];
    out[29] = tab[23] + tab[31];
    out[ 3] = tab[24] + tab[20];
    out[19] = tab[25] + tab[21];
    out[11] = tab[26] + tab[22];
    out[27] = tab[27] + tab[23];
    out[ 7] = tab[28] + tab[18];
    out[23] = tab[29] + tab[19];
    out[15] = tab[30] + tab[17];
    out[31] = tab[31];
}

#define OUT_SHIFT (WFRAC_BITS + FRAC_BITS - 15)

#if FRAC_BITS <= 15

static int round_sample(int sum)
{
    int sum1;
    sum1 = (sum + (1 << (OUT_SHIFT - 1))) >> OUT_SHIFT;
    if (sum1 < -32768)
        sum1 = -32768;
    else if (sum1 > 32767)
        sum1 = 32767;
    return sum1;
}



/* signed 16x16 -> 32 multiply add accumulate */
#define MACS(rt, ra, rb) rt += (ra) * (rb)

/* signed 16x16 -> 32 multiply */
#define MULS(ra, rb) ((ra) * (rb))


#else

static int round_sample(signed long long sum) 
{
    int sum1;
    sum1 = (int)((sum + (int64_t_C(1) << (OUT_SHIFT - 1))) >> OUT_SHIFT);
    if (sum1 < -32768)
        sum1 = -32768;
    else if (sum1 > 32767)
        sum1 = 32767;
    return sum1;
}

#define MULS(ra, rb) MUL64(ra, rb)

#endif

#define SUM8(sum, op, w, p) \
{                                               \
    sum op MULS((w)[0 * 64], p[0 * 64]);\
    sum op MULS((w)[1 * 64], p[1 * 64]);\
    sum op MULS((w)[2 * 64], p[2 * 64]);\
    sum op MULS((w)[3 * 64], p[3 * 64]);\
    sum op MULS((w)[4 * 64], p[4 * 64]);\
    sum op MULS((w)[5 * 64], p[5 * 64]);\
    sum op MULS((w)[6 * 64], p[6 * 64]);\
    sum op MULS((w)[7 * 64], p[7 * 64]);\
}

#define SUM8P2(sum1, op1, sum2, op2, w1, w2, p) \
{                                               \
    int tmp;\
    tmp = p[0 * 64];\
    sum1 op1 MULS((w1)[0 * 64], tmp);\
    sum2 op2 MULS((w2)[0 * 64], tmp);\
    tmp = p[1 * 64];\
    sum1 op1 MULS((w1)[1 * 64], tmp);\
    sum2 op2 MULS((w2)[1 * 64], tmp);\
    tmp = p[2 * 64];\
    sum1 op1 MULS((w1)[2 * 64], tmp);\
    sum2 op2 MULS((w2)[2 * 64], tmp);\
    tmp = p[3 * 64];\
    sum1 op1 MULS((w1)[3 * 64], tmp);\
    sum2 op2 MULS((w2)[3 * 64], tmp);\
    tmp = p[4 * 64];\
    sum1 op1 MULS((w1)[4 * 64], tmp);\
    sum2 op2 MULS((w2)[4 * 64], tmp);\
    tmp = p[5 * 64];\
    sum1 op1 MULS((w1)[5 * 64], tmp);\
    sum2 op2 MULS((w2)[5 * 64], tmp);\
    tmp = p[6 * 64];\
    sum1 op1 MULS((w1)[6 * 64], tmp);\
    sum2 op2 MULS((w2)[6 * 64], tmp);\
    tmp = p[7 * 64];\
    sum1 op1 MULS((w1)[7 * 64], tmp);\
    sum2 op2 MULS((w2)[7 * 64], tmp);\
}


/* 32 sub band synthesis filter. Input: 32 sub band samples, Output:
   32 samples. */
/* XXX: optimize by avoiding ring buffer usage */
static void synth_filter(MPADecodeContext *s1,
                         int ch, signed short *samples, int incr, 
                         signed int sb_samples[SBLIMIT])
{
    signed int tmp[32];
    register MPA_INT *synth_buf;
    register const MPA_INT *w, *w2, *p;
    int j, offset, v;
    signed short *samples2;
#if FRAC_BITS <= 15
    int sum, sum2;
#else
    signed long long sum, sum2;
#endif
    
    dct32(tmp, sb_samples);
    
    offset = s1->synth_buf_offset[ch];
    synth_buf = s1->synth_buf[ch] + offset;

    for(j=0;j<32;j++) {
        v = tmp[j];
#if FRAC_BITS <= 15
        /* NOTE: can cause a loss in precision if very high amplitude
           sound */
        if (v > 32767)
            v = 32767;
        else if (v < -32768)
            v = -32768;
#endif
        synth_buf[j] = v;
    }
    /* copy to avoid wrap */
    memcpy(synth_buf + 512, synth_buf, 32 * sizeof(MPA_INT));

    samples2 = samples + 31 * incr;
    w = window;
    w2 = window + 31;

    sum = 0;
    p = synth_buf + 16;
    SUM8(sum, +=, w, p);
    p = synth_buf + 48;
    SUM8(sum, -=, w + 32, p);
    *samples = round_sample(sum);
    samples += incr;
    w++;

    /* we calculate two samples at the same time to avoid one memory
       access per two sample */
    for(j=1;j<16;j++) {
        sum = 0;
        sum2 = 0;
        p = synth_buf + 16 + j;
        SUM8P2(sum, +=, sum2, -=, w, w2, p);
        p = synth_buf + 48 - j;
        SUM8P2(sum, -=, sum2, -=, w + 32, w2 + 32, p);

        *samples = round_sample(sum);
        samples += incr;
        *samples2 = round_sample(sum2);
        samples2 -= incr;
        w++;
        w2--;
    }
    
    p = synth_buf + 32;
    sum = 0;
    SUM8(sum, -=, w + 32, p);
    *samples = round_sample(sum);

    offset = (offset - 32) & 511;
    s1->synth_buf_offset[ch] = offset;
}

/* fast header check for resync */
static int check_header(unsigned int header)
{
    /* header */
    if ((header & 0xffe00000) != 0xffe00000)
	return -1;
    /* layer check */
    if (((header >> 17) & 3) == 0)
	return -1;
    /* bit rate */
    if (((header >> 12) & 0xf) == 0xf)
	return -1;
    /* frequency */
    if (((header >> 10) & 3) == 3)
	return -1;
    return 0;
}

/* header + layer + bitrate + freq + lsf/mpeg25 */
#define SAME_HEADER_MASK \
   (0xffe00000 | (3 << 17) | (0xf << 12) | (3 << 10) | (3 << 19))

/* header decoding. MUST check the header before because no
   consistency check is done there. Return 1 if free format found and
   that the frame size must be computed externally */
static int decode_header(MPADecodeContext *s, unsigned int header)
{
    int sample_rate, frame_size, mpeg25, padding;
    int sample_rate_index, bitrate_index;
    if (header & (1<<20)) {
        s->lsf = (header & (1<<19)) ? 0 : 1;
        mpeg25 = 0;
    } else {
        s->lsf = 1;
        mpeg25 = 1;
    }
    
    s->layer = 4 - ((header >> 17) & 3);
    /* extract frequency */
    sample_rate_index = (header >> 10) & 3;
    sample_rate = mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
    sample_rate_index += 3 * (s->lsf + mpeg25);
    s->sample_rate_index = sample_rate_index;
    s->error_protection = ((header >> 16) & 1) ^ 1;
    s->sample_rate = sample_rate;

    bitrate_index = (header >> 12) & 0xf;
    padding = (header >> 9) & 1;
    s->mode = (header >> 6) & 3;
    s->mode_ext = (header >> 4) & 3;

    if (s->mode == MPA_MONO)
        s->nb_channels = 1;
    else
        s->nb_channels = 2;
    
    if (bitrate_index != 0) {
        frame_size = mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
        s->bit_rate = frame_size * 1000;
        switch(s->layer) {
        case 1:
            frame_size = (frame_size * 12000) / sample_rate;
            frame_size = (frame_size + padding) * 4;
            break;
        case 2:
            frame_size = (frame_size * 144000) / sample_rate;
            frame_size += padding;
            break;
        default:
        case 3:
            frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
            frame_size += padding;
            break;
        }
        s->frame_size = frame_size;
    } else {
        /* if no frame size computed, signal it */
        if (!s->free_format_frame_size)
            return 1;
        /* free format: compute bitrate and real frame size from the
           frame size we extracted by reading the bitstream */
        s->frame_size = s->free_format_frame_size;
        switch(s->layer) {
        case 1:
            s->frame_size += padding  * 4;
            s->bit_rate = (s->frame_size * sample_rate) / 48000;
            break;
        case 2:
            s->frame_size += padding;
            s->bit_rate = (s->frame_size * sample_rate) / 144000;
            break;
        default:
        case 3:
            s->frame_size += padding;
            s->bit_rate = (s->frame_size * (sample_rate << s->lsf)) / 144000;
            break;
        }
    }
    
    return 0;
}

/* useful helper to get mpeg audio stream infos. Return -1 if error in
   header, otherwise the coded frame size in bytes */
static int mpa_decode_header(AVCodecContext *avctx, unsigned int head)
{
	unsigned int nHead = SWAP32(head);
    MPADecodeContext s1, *s = &s1;
    memset( s, 0, sizeof(MPADecodeContext) );

    if (check_header(nHead) != 0)
        return -1;

    if (decode_header(s, nHead) != 0) {
        return -1;
    }

    if(s->layer==2) 
	{
        avctx->frame_size = 1152;
	}
	else
	{
		return -1;
	}

    avctx->sample_rate = s->sample_rate;
    avctx->channels = s->nb_channels;
    avctx->bit_rate = s->bit_rate;
    avctx->sub_id = s->layer;
    return s->frame_size;
}

/* bitrate is in kb/s */
static int l2_select_table(int bitrate, int nb_channels, int freq, int lsf)
{
    int ch_bitrate, table;
    
    ch_bitrate = bitrate / nb_channels;
    if (!lsf) {
        if ((freq == 48000 && ch_bitrate >= 56) ||
            (ch_bitrate >= 56 && ch_bitrate <= 80)) 
            table = 0;
        else if (freq != 48000 && ch_bitrate >= 96) 
            table = 1;
        else if (freq != 32000 && ch_bitrate <= 48) 
            table = 2;
        else 
            table = 3;
    } else {
        table = 4;
    }
    return table;
}

static int mp_decode_layer2(MPADecodeContext *s)
{
    int sblimit; /* number of used subbands */
    const unsigned char *alloc_table;
    int table, bit_alloc_bits, i, j, ch, bound, v;
    unsigned char bit_alloc[MPA_MAX_CHANNELS][SBLIMIT];
    unsigned char scale_code[MPA_MAX_CHANNELS][SBLIMIT];
    unsigned char scale_factors[MPA_MAX_CHANNELS][SBLIMIT][3], *sf;
    int scale, qindex, bits, steps, k, l, m, b;

    /* select decoding table */
    table = l2_select_table(s->bit_rate / 1000, s->nb_channels, 
                            s->sample_rate, s->lsf);
    sblimit = sblimit_table[table];
    alloc_table = alloc_tables[table];

    if (s->mode == MPA_JSTEREO) 
        bound = (s->mode_ext + 1) * 4;
    else
        bound = sblimit;

    //printf("bound=%d sblimit=%d\n", bound, sblimit);

    /* sanity check */
    if( bound > sblimit ) bound = sblimit;

    /* parse bit allocation */
    j = 0;
    for(i=0;i<bound;i++) {
        bit_alloc_bits = alloc_table[j];
        for(ch=0;ch<s->nb_channels;ch++) {
            bit_alloc[ch][i] = get_bits(&s->gb, bit_alloc_bits);
        }
        j += 1 << bit_alloc_bits;
    }
    for(i=bound;i<sblimit;i++) {
        bit_alloc_bits = alloc_table[j];
        v = get_bits(&s->gb, bit_alloc_bits);
        bit_alloc[0][i] = v;
        bit_alloc[1][i] = v;
        j += 1 << bit_alloc_bits;
    }

    /* scale codes */
    for(i=0;i<sblimit;i++) {
        for(ch=0;ch<s->nb_channels;ch++) {
            if (bit_alloc[ch][i]) 
                scale_code[ch][i] = get_bits(&s->gb, 2);
        }
    }
    
    /* scale factors */
    for(i=0;i<sblimit;i++) {
        for(ch=0;ch<s->nb_channels;ch++) {
            if (bit_alloc[ch][i]) {
                sf = scale_factors[ch][i];
                switch(scale_code[ch][i]) {
                default:
                case 0:
                    sf[0] = get_bits(&s->gb, 6);
                    sf[1] = get_bits(&s->gb, 6);
                    sf[2] = get_bits(&s->gb, 6);
                    break;
                case 2:
                    sf[0] = get_bits(&s->gb, 6);
                    sf[1] = sf[0];
                    sf[2] = sf[0];
                    break;
                case 1:
                    sf[0] = get_bits(&s->gb, 6);
                    sf[2] = get_bits(&s->gb, 6);
                    sf[1] = sf[0];
                    break;
                case 3:
                    sf[0] = get_bits(&s->gb, 6);
                    sf[2] = get_bits(&s->gb, 6);
                    sf[1] = sf[2];
                    break;
                }
            }
        }
    }

   /* samples */
    for(k=0;k<3;k++) {
        for(l=0;l<12;l+=3) {
            j = 0;
            for(i=0;i<bound;i++) {
                bit_alloc_bits = alloc_table[j];
                for(ch=0;ch<s->nb_channels;ch++) {
                    b = bit_alloc[ch][i];
                    if (b) {
                        scale = scale_factors[ch][i][k];
                        qindex = alloc_table[j+b];
                        bits = quant_bits[qindex];
                        if (bits < 0) {
                            /* 3 values at the same time */
                            v = get_bits(&s->gb, -bits);
                            steps = quant_steps[qindex];
                            s->sb_samples[ch][k * 12 + l + 0][i] = 
                                l2_unscale_group(steps, v % steps, scale);
                            v = v / steps;
                            s->sb_samples[ch][k * 12 + l + 1][i] = 
                                l2_unscale_group(steps, v % steps, scale);
                            v = v / steps;
                            s->sb_samples[ch][k * 12 + l + 2][i] = 
                                l2_unscale_group(steps, v, scale);
                        } else {
                            for(m=0;m<3;m++) {
                                v = get_bits(&s->gb, bits);
                                v = l1_unscale(bits - 1, v, scale);
                                s->sb_samples[ch][k * 12 + l + m][i] = v;
                            }
                        }
                    } else {
                        s->sb_samples[ch][k * 12 + l + 0][i] = 0;
                        s->sb_samples[ch][k * 12 + l + 1][i] = 0;
                        s->sb_samples[ch][k * 12 + l + 2][i] = 0;
                    }
                }
                /* next subband in alloc table */
                j += 1 << bit_alloc_bits; 
            }
            /* XXX: find a way to avoid this duplication of code */
            for(i=bound;i<sblimit;i++) {
                bit_alloc_bits = alloc_table[j];
                b = bit_alloc[0][i];
                if (b) {
                    int mant, scale0, scale1;
                    scale0 = scale_factors[0][i][k];
                    scale1 = scale_factors[1][i][k];
                    qindex = alloc_table[j+b];
                    bits = quant_bits[qindex];
                    if (bits < 0) {
                        /* 3 values at the same time */
                        v = get_bits(&s->gb, -bits);
                        steps = quant_steps[qindex];
                        mant = v % steps;
                        v = v / steps;
                        s->sb_samples[0][k * 12 + l + 0][i] = 
                            l2_unscale_group(steps, mant, scale0);
                        s->sb_samples[1][k * 12 + l + 0][i] = 
                            l2_unscale_group(steps, mant, scale1);
                        mant = v % steps;
                        v = v / steps;
                        s->sb_samples[0][k * 12 + l + 1][i] = 
                            l2_unscale_group(steps, mant, scale0);
                        s->sb_samples[1][k * 12 + l + 1][i] = 
                            l2_unscale_group(steps, mant, scale1);
                        s->sb_samples[0][k * 12 + l + 2][i] = 
                            l2_unscale_group(steps, v, scale0);
                        s->sb_samples[1][k * 12 + l + 2][i] = 
                            l2_unscale_group(steps, v, scale1);
                    } else {
                        for(m=0;m<3;m++) {
                            mant = get_bits(&s->gb, bits);
                            s->sb_samples[0][k * 12 + l + m][i] = 
                                l1_unscale(bits - 1, mant, scale0);
                            s->sb_samples[1][k * 12 + l + m][i] = 
                                l1_unscale(bits - 1, mant, scale1);
                        }
                    }
                } else {
                    s->sb_samples[0][k * 12 + l + 0][i] = 0;
                    s->sb_samples[0][k * 12 + l + 1][i] = 0;
                    s->sb_samples[0][k * 12 + l + 2][i] = 0;
                    s->sb_samples[1][k * 12 + l + 0][i] = 0;
                    s->sb_samples[1][k * 12 + l + 1][i] = 0;
                    s->sb_samples[1][k * 12 + l + 2][i] = 0;
                }
                /* next subband in alloc table */
                j += 1 << bit_alloc_bits; 
            }
            /* fill remaining samples to zero */
            for(i=sblimit;i<SBLIMIT;i++) {
                for(ch=0;ch<s->nb_channels;ch++) {
                    s->sb_samples[ch][k * 12 + l + 0][i] = 0;
                    s->sb_samples[ch][k * 12 + l + 1][i] = 0;
                    s->sb_samples[ch][k * 12 + l + 2][i] = 0;
                }
            }
        }
    }
    return 3 * 12;
}


/*static void exponents_from_scale_factors(MPADecodeContext *s, 
                                         GranuleDef *g,
                                         signed short *exponents)
{
    const unsigned char *bstab, *pretab;
    int len, i, j, k, l, v0, shift, gain, gains[3];
    signed short *exp_ptr;

    exp_ptr = exponents;
    gain = g->global_gain - 210;
    shift = g->scalefac_scale + 1;

    bstab = band_size_long[s->sample_rate_index];
    pretab = mpa_pretab[g->preflag];
    for(i=0;i<g->long_end;i++) {
        v0 = gain - ((g->scale_factors[i] + pretab[i]) << shift);
        len = bstab[i];
        for(j=len;j>0;j--)
            *exp_ptr++ = v0;
    }

    if (g->short_start < 13) {
        bstab = band_size_short[s->sample_rate_index];
        gains[0] = gain - (g->subblock_gain[0] << 3);
        gains[1] = gain - (g->subblock_gain[1] << 3);
        gains[2] = gain - (g->subblock_gain[2] << 3);
        k = g->long_end;
        for(i=g->short_start;i<13;i++) {
            len = bstab[i];
            for(l=0;l<3;l++) {
                v0 = gains[l] - (g->scale_factors[k++] << shift);
                for(j=len;j>0;j--)
                *exp_ptr++ = v0;
            }
        }
    }
}*/

/*static void compute_antialias_integer(MPADecodeContext *s,
                              GranuleDef *g)
{
    signed int *ptr, *p0, *p1, *csa;
    int n, i, j;

    // we antialias only "long" bands
    if (g->block_type == 2) {
        if (!g->switch_point)
            return;
        // XXX: check this for 8000Hz case 
        n = 1;
    } else {
        n = SBLIMIT - 1;
    }
    
    ptr = g->sb_hybrid + 18;
    for(i = n;i > 0;i--) {
        p0 = ptr - 1;
        p1 = ptr;
        csa = &csa_table[0][0];       
        for(j=0;j<4;j++) {
            int tmp0 = *p0;
            int tmp1 = *p1;
            signed long long tmp2= MUL64(tmp0 + tmp1, csa[0]);
            *p0 = FRAC_RND(tmp2 - MUL64(tmp1, csa[2]));
            *p1 = FRAC_RND(tmp2 + MUL64(tmp0, csa[3]));
            p0--; p1++;
            csa += 4;
            tmp0 = *p0;
            tmp1 = *p1;
            tmp2= MUL64(tmp0 + tmp1, csa[0]);
            *p0 = FRAC_RND(tmp2 - MUL64(tmp1, csa[2]));
            *p1 = FRAC_RND(tmp2 + MUL64(tmp0, csa[3]));
            p0--; p1++;
            csa += 4;
        }
        ptr += 18;       
    }
}*/

/*static int lrintf(float x)
{
    return (int)(x);
}*/


static int mp_decode_frame(MPADecodeContext *s, 
                           short *samples)
{
    int i, nb_frames, ch;
    short *samples_ptr;

    init_get_bits(&s->gb, s->inbuf + HEADER_SIZE, 
                  (s->inbuf_ptr - s->inbuf - HEADER_SIZE)*8);
    
    /* skip error protection field */
    if (s->error_protection)
        get_bits(&s->gb, 16);

    //printf("frame %d:\n", s->frame_count);
    switch(s->layer) 
	{
    case 2:
        nb_frames = mp_decode_layer2(s);
        break;
    }
    /* apply the synthesis filter */
    for(ch=0;ch<s->nb_channels;ch++) {
        samples_ptr = samples + ch;
        for(i=0;i<nb_frames;i++) {
            synth_filter(s, ch, samples_ptr, s->nb_channels,
                         s->sb_samples[ch][i]);
            samples_ptr += 32 * s->nb_channels;
        }
    }
    return nb_frames * 32 * sizeof(short) * s->nb_channels;
}

BOOL MP2_decode_frame(HMP2DEC hDec,
			void *data, unsigned int *data_size,
			unsigned char * buf, unsigned int buf_size)
{
	AVCodecContext * avctx = (AVCodecContext *)hDec;
    MPADecodeContext *s = (MPADecodeContext *)avctx->priv_data;
    unsigned int header;
    unsigned char *buf_ptr;
    int len, out_size;
    short *out_samples = (short *)data;

	if(mpa_decode_header(avctx, *((unsigned int*)buf))<0)
		return FALSE;

	buf_ptr = buf;
    while (buf_size > 0) 
	{
		len = s->inbuf_ptr - s->inbuf;
		if (s->frame_size == 0) 
		{
            /* special case for next header for first frame in free
               format case (XXX: find a simpler method) */
			if (s->free_format_next_header != 0) 
			{
				s->inbuf[0] = s->free_format_next_header >> 24;
				s->inbuf[1] = s->free_format_next_header >> 16;
				s->inbuf[2] = s->free_format_next_header >> 8;
				s->inbuf[3] = s->free_format_next_header;
				s->inbuf_ptr = s->inbuf + 4;
				s->free_format_next_header = 0;
				goto got_header;
			}
			/* no header seen : find one. We need at least HEADER_SIZE
				   bytes to parse it */
			len = HEADER_SIZE - len;
			if (len > buf_size)
				len = buf_size;
			if (len > 0) 
			{
				memcpy(s->inbuf_ptr, buf_ptr, len);
				buf_ptr += len;
				buf_size -= len;
				s->inbuf_ptr += len;
			}
			if ((s->inbuf_ptr - s->inbuf) >= HEADER_SIZE) 
			{
				got_header:
				header = (s->inbuf[0] << 24) | (s->inbuf[1] << 16) |
					(s->inbuf[2] << 8) | s->inbuf[3];

				if (check_header(header) < 0) 
				{
					/* no sync found : move by one byte (inefficient, but simple!) */
					memmove(s->inbuf, s->inbuf + 1, s->inbuf_ptr - s->inbuf - 1);
					s->inbuf_ptr--;
							printf("skip %x\n", header);
							/* reset free format frame size to give a chance
							   to get a new bitrate */
							s->free_format_frame_size = 0;
				} 
				else 
				{
					if (decode_header(s, header) == 1) 
					{
							/* free format: prepare to compute frame size */
						s->frame_size = -1;
					}
					/* update codec info */
					avctx->sample_rate = s->sample_rate;
					avctx->channels = s->nb_channels;
					avctx->bit_rate = s->bit_rate;
					avctx->sub_id = s->layer;
					switch(s->layer) 
					{
					case 1:
						avctx->frame_size = 384;
						break;
					case 2:
						avctx->frame_size = 1152;
						break;
					case 3:
						if (s->lsf)
							avctx->frame_size = 576;
						else
							avctx->frame_size = 1152;
						break;
					}
				}
			}
		} 
		else if (s->frame_size == -1) 
		{
            /* free format : find next sync to compute frame size */
			len = MPA_MAX_CODED_FRAME_SIZE - len;
			if (len > buf_size)
			len = buf_size;
            if (len == 0) 
			{
				/* frame too long: resync */
						s->frame_size = 0;
				memmove(s->inbuf, s->inbuf + 1, s->inbuf_ptr - s->inbuf - 1);
				s->inbuf_ptr--;
            } 
			else 
			{
                unsigned char *p, *pend;
                unsigned int header1;
                int padding;

                memcpy(s->inbuf_ptr, buf_ptr, len);
                /* check for header */
                p = s->inbuf_ptr - 3;
                pend = s->inbuf_ptr + len - 4;
                while (p <= pend) 
				{
                    header = (p[0] << 24) | (p[1] << 16) |
                        (p[2] << 8) | p[3];
                    header1 = (s->inbuf[0] << 24) | (s->inbuf[1] << 16) |
                        (s->inbuf[2] << 8) | s->inbuf[3];
                    /* check with high probability that we have a
                       valid header */
                    if ((header & SAME_HEADER_MASK) ==
                        (header1 & SAME_HEADER_MASK)) 
					{
                        /* header found: update pointers */
                        len = (p + 4) - s->inbuf_ptr;
                        buf_ptr += len;
                        buf_size -= len;
                        s->inbuf_ptr = p;
                        /* compute frame size */
                        s->free_format_next_header = header;
                        s->free_format_frame_size = s->inbuf_ptr - s->inbuf;
                        padding = (header1 >> 9) & 1;
                        if (s->layer == 1)
                            s->free_format_frame_size -= padding * 4;
                        else
                            s->free_format_frame_size -= padding;
                        decode_header(s, header1);
                        goto next_data;
                    }
                    p++;
                }
                /* not found: simply increase pointers */
                buf_ptr += len;
                s->inbuf_ptr += len;
                buf_size -= len;
            }
		} 
		else if (len < s->frame_size)
		{
            if (s->frame_size > MPA_MAX_CODED_FRAME_SIZE)
                s->frame_size = MPA_MAX_CODED_FRAME_SIZE;
			len = s->frame_size - len;
			if (len > buf_size)
			len = buf_size;
			memcpy(s->inbuf_ptr, buf_ptr, len);
			buf_ptr += len;
			s->inbuf_ptr += len;
			buf_size -= len;	
		}
next_data:
		if (s->frame_size > 0 && (s->inbuf_ptr - s->inbuf) >= s->frame_size)
		{
			if (avctx->parse_only) 
			{
				/* simply return the frame data */
				*(unsigned char **)data = s->inbuf;
				out_size = s->inbuf_ptr - s->inbuf;
			} 
			else 
			{
				out_size = mp_decode_frame(s, out_samples);
			}
			s->inbuf_ptr = s->inbuf;
			s->frame_size = 0;
			*data_size = out_size;
			break;
		}
    }
	return TRUE;
    //return buf_ptr - buf;
}

void MP2_decode_close(HMP2DEC hDec)
{
	AVCodecContext * avctx = (AVCodecContext *)hDec;
    MPADecodeContext *s = (MPADecodeContext *)avctx->priv_data;
	if(avctx && s)
	{
		free(s);
	}
	if(avctx)
	{
		free(avctx);
	}
}

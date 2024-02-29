// strutil.h

typedef struct _TokensIdx {
	uint32_t count;			// number of tokens found
	uint32_t idx[10];	    // start index of each token
	char * ptr[10];  		// pointer to start of each token
	uint32_t length[10];  		// length of token in bytes or chars
} TokensIdx;

void getTokensIdx(char * s, TokensIdx * t);
int isNum(char * s);
uint32_t readNum(char * s);
size_t d_strlcat(char * dst, const char * src, size_t dstSize);

//----------------------------------------------------------------------------
//  STRING MACROS
//----------------------------------------------------------------------------

// These macros work on char[] arrays on the stack which have a determinable size from sizeof()
#define ssprintf(dst, ...) snprintf(dst, sizeof(dst), __VA_ARGS__)
#define sstrcat(dst, str) d_strlcat(dst, str, sizeof(str));
#define sscatprintf(dst, ...) snprintf(&dst[strlen(dst)], sizeof(dst) - strlen(dst), __VA_ARGS__)

// Boolean string compare.
#define streq(a,b) (strcmp(a,b)==0)

// Boolean string compare.
#define streqn(a,b,n) (strncmp(a,b,n)==0)

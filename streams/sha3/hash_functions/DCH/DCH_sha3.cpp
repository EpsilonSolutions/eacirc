#include "DCH_sha3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//S-box transform.
//This is inv(x) xor 3

static const BitSequence DCH_SBOX[256] = 
  {0x03, 0x02, 0x8D, 0xF7, 0x44, 0xA4, 0x79, 0xB9, 
   0xAE, 0x9E, 0xDE, 0x9B, 0x3E, 0xA9, 0x5E, 0x95, 
   0xDB, 0x71, 0xC3, 0x5B, 0xE3, 0x3D, 0x4F, 0x65, 
   0x93, 0xDD, 0x56, 0x83, 0xA3, 0x80, 0x48, 0x29, 
   0x6F, 0xEE, 0x3A, 0x52, 0x63, 0x55, 0x2F, 0x89, 
   0x73, 0xD3, 0x1C, 0x49, 0x25, 0x88, 0x30, 0x6D, 
   0x4B, 0x8A, 0x6C, 0x2D, 0xA7, 0xC0, 0x43, 0x5D, 
   0x53, 0x21, 0xCC, 0xAA, 0xA8, 0x0F, 0x16, 0xE2, 

   0x35, 0x5C, 0xFB, 0xD6, 0x91, 0x4D, 0xA5, 0x07, 
   0x33, 0x8B, 0x28, 0x1D, 0x15, 0x64, 0x46, 0x90, 
   0x3B, 0x20, 0x6B, 0x8F, 0x82, 0x19, 0x26, 0x62, 
   0x10, 0xC2, 0xC8, 0x60, 0x94, 0x0D, 0x34, 0x42, 
   0x27, 0x54, 0xC9, 0x58, 0xBA, 0xC7, 0x14, 0x4E, 
   0x51, 0x8E, 0xEC, 0xB0, 0x23, 0xEF, 0x2C, 0x31, 
   0x2B, 0xD2, 0x12, 0xDA, 0xEA, 0xF8, 0xD9, 0x7A, 
   0xD8, 0x74, 0x05, 0xB8, 0x87, 0xCE, 0xFD, 0xFF, 

   0x18, 0x57, 0xA2, 0x1E, 0x7F, 0xCF, 0xE7, 0xB3, 
   0x4A, 0x32, 0x24, 0x2E, 0x50, 0x6A, 0x01, 0xF6, 
   0x1B, 0xDC, 0x47, 0x4C, 0x98, 0xBF, 0x0C, 0x5F, 
   0x08, 0xDF, 0xBE, 0x97, 0xAF, 0x0A, 0xC4, 0xA1, 
   0x1F, 0x81, 0x9C, 0xC5, 0x37, 0xC1, 0x45, 0x06, 
   0xCD, 0x38, 0x0E, 0x3F, 0x9F, 0x0B, 0xBD, 0xB4, 
   0x84, 0xE6, 0xED, 0x68, 0xE8, 0xF1, 0xBC, 0xAC, 
   0xC6, 0x67, 0x04, 0x78, 0x96, 0x99, 0xAD, 0xB5, 

   0x11, 0x5A, 0xA6, 0x36, 0x66, 0xBB, 0xA0, 0x9D, 
   0xD1, 0xF4, 0x61, 0x59, 0x86, 0x7E, 0xAB, 0x39, 
   0x2A, 0x72, 0xCB, 0xF5, 0xFA, 0x40, 0xD4, 0xD5, 
   0x13, 0x70, 0x75, 0x7B, 0x9A, 0x09, 0x1A, 0x92, 
   0x17, 0x3C, 0xE5, 0xF3, 0x85, 0xB2, 0xE1, 0xF2, 
   0xF9, 0x77, 0xF0, 0xB7, 0x6E, 0x22, 0xB1, 0x69, 
   0xE0, 0xE4, 0xB6, 0xE9, 0x00, 0x8C, 0xD0, 0xCA, 
   0x41, 0xD7, 0xEB, 0x76, 0x7C, 0xFC, 0x7D, 0xFE};


static const BitSequence *DCH_ROUND_KEY[4] = 
  {&DCH_SBOX[0], &DCH_SBOX[64], &DCH_SBOX[128], &DCH_SBOX[192]};


//Galois field GF(2^8).  gf[i] = a^i in vector form.  Zero element not included.
static const BitSequence dch_gf[256] = 
  {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
   0x1D, 0x3A, 0x74, 0xE8, 0xCD, 0x87, 0x13, 0x26,
   0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75, 0xEA, 0xC9,
   0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0,
   0x9D, 0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35,
   0x6A, 0xD4, 0xB5, 0x77, 0xEE, 0xC1, 0x9F, 0x23,
   0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50, 0xA0,
   0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1,

   0x5F, 0xBE, 0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC,
   0x65, 0xCA, 0x89, 0x0F, 0x1E, 0x3C, 0x78, 0xF0,
   0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F,
   0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2,
   0xD9, 0xAF, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
   0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD, 0x67, 0xCE,
   0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93,
   0x3B, 0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC,

   0x85, 0x17, 0x2E, 0x5C, 0xB8, 0x6D, 0xDA, 0xA9,
   0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A, 0x54,
   0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA,
   0x49, 0x92, 0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73,
   0xE6, 0xD1, 0xBF, 0x63, 0xC6, 0x91, 0x3F, 0x7E,
   0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
   0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4,
   0x95, 0x37, 0x6E, 0xDC, 0xA5, 0x57, 0xAE, 0x41,

   0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D, 0x07, 0x0E,
   0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6,
   0x51, 0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF,
   0xC3, 0x9B, 0x2B, 0x56, 0xAC, 0x45, 0x8A, 0x09,
   0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4, 0xF5,
   0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16,
   0x2C, 0x58, 0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83,
   0x1B, 0x36, 0x6C, 0xD8, 0xAD, 0x47, 0x8E, 0x01};

//Inverse mapping of GF(2^8).  gfinv[i] = j s.t. a^(j) = gf[j] = i.
static const BitSequence dch_gfinv[256] = 
  {0x00, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1A, 0xC6,
   0x03, 0xDF, 0x33, 0xEE, 0x1B, 0x68, 0xC7, 0x4B,
   0x04, 0x64, 0xE0, 0x0E, 0x34, 0x8D, 0xEF, 0x81,
   0x1C, 0xC1, 0x69, 0xF8, 0xC8, 0x08, 0x4C, 0x71,
   0x05, 0x8A, 0x65, 0x2F, 0xE1, 0x24, 0x0F, 0x21,
   0x35, 0x93, 0x8E, 0xDA, 0xF0, 0x12, 0x82, 0x45,
   0x1D, 0xB5, 0xC2, 0x7D, 0x6A, 0x27, 0xF9, 0xB9,
   0xC9, 0x9A, 0x09, 0x78, 0x4D, 0xE4, 0x72, 0xA6,

   0x06, 0xBF, 0x8B, 0x62, 0x66, 0xDD, 0x30, 0xFD,
   0xE2, 0x98, 0x25, 0xB3, 0x10, 0x91, 0x22, 0x88,
   0x36, 0xD0, 0x94, 0xCE, 0x8F, 0x96, 0xDB, 0xBD,
   0xF1, 0xD2, 0x13, 0x5C, 0x83, 0x38, 0x46, 0x40,
   0x1E, 0x42, 0xB6, 0xA3, 0xC3, 0x48, 0x7E, 0x6E,
   0x6B, 0x3A, 0x28, 0x54, 0xFA, 0x85, 0xBA, 0x3D,
   0xCA, 0x5E, 0x9B, 0x9F, 0x0A, 0x15, 0x79, 0x2B,
   0x4E, 0xD4, 0xE5, 0xAC, 0x73, 0xF3, 0xA7, 0x57,

   0x07, 0x70, 0xC0, 0xF7, 0x8C, 0x80, 0x63, 0x0D,
   0x67, 0x4A, 0xDE, 0xED, 0x31, 0xC5, 0xFE, 0x18,
   0xE3, 0xA5, 0x99, 0x77, 0x26, 0xB8, 0xB4, 0x7C,
   0x11, 0x44, 0x92, 0xD9, 0x23, 0x20, 0x89, 0x2E,
   0x37, 0x3F, 0xD1, 0x5B, 0x95, 0xBC, 0xCF, 0xCD,
   0x90, 0x87, 0x97, 0xB2, 0xDC, 0xFC, 0xBE, 0x61,
   0xF2, 0x56, 0xD3, 0xAB, 0x14, 0x2A, 0x5D, 0x9E,
   0x84, 0x3C, 0x39, 0x53, 0x47, 0x6D, 0x41, 0xA2,

   0x1F, 0x2D, 0x43, 0xD8, 0xB7, 0x7B, 0xA4, 0x76,
   0xC4, 0x17, 0x49, 0xEC, 0x7F, 0x0C, 0x6F, 0xF6,
   0x6C, 0xA1, 0x3B, 0x52, 0x29, 0x9D, 0x55, 0xAA,
   0xFB, 0x60, 0x86, 0xB1, 0xBB, 0xCC, 0x3E, 0x5A,
   0xCB, 0x59, 0x5F, 0xB0, 0x9C, 0xA9, 0xA0, 0x51,
   0x0B, 0xF5, 0x16, 0xEB, 0x7A, 0x75, 0x2C, 0xD7,
   0x4F, 0xAE, 0xD5, 0xE9, 0xE6, 0xE7, 0xAD, 0xE8,
   0x74, 0xD6, 0xF4, 0xEA, 0xA8, 0x50, 0x58, 0xAF};


BitSequence dch_multtable[256][256];
int dchIsInitialized = 0;

DCH::DCH(const int numRounds) {
	if (numRounds == -1) {
		dchNumRounds = DCH_NUM_ROUNDS;
	} else {
		dchNumRounds = numRounds;
	}
}

//state is where the initialized state gets returned
int DCH::Init(int hashbitlen){
  int i, j;

  //Make sure the input hash bit length is supported
  if((hashbitlen != 224) && (hashbitlen != 256) &&
     (hashbitlen != 384) && (hashbitlen != 512)){
    return BAD_HASHBITLEN;
  }

  if(dchIsInitialized == 0){
    for(i=0;i<256;i++){
      for(j=0;j<256;j++){
	dch_multtable[i][j] = (j==0) ? 0 : dch_gf[(i+dch_gfinv[j])%255];
      }
    }
    dchIsInitialized = 1;
  } 

  dchState.hashbitlen = hashbitlen;
  dchState.numUnprocessed = 0;
  dchState.unprocessed = (BitSequence*) malloc(DCH_BLOCK_LENGTH_BYTES * sizeof(BitSequence));
  dchState.curr = (BitSequence*) malloc(DCH_BLOCK_LENGTH_BYTES * sizeof(BitSequence));
  memset(dchState.curr, 0, DCH_BLOCK_LENGTH_BYTES);

  dchState.datalen = 0;

  //initialize square-free sequence state
  dchState.p[0] = (BitSequence*) malloc(64*sizeof(unsigned char));
  dchState.p[1] = (BitSequence*) malloc(64*sizeof(unsigned char));
  dchState.p[2] = (BitSequence*) malloc(64*sizeof(unsigned char));
  for(i=0;i<63;i++){
    dchState.p[0][i]= 64-i;
  }
  dchState.p[1][0]=64;
  dchState.p[2][0]=64;
  dchState.top[0] = &(dchState.p[0][63]);
  dchState.top[1] = &(dchState.p[1][0]);
  dchState.top[2] = &(dchState.p[2][0]);
  dchState.parity = 1;
  dchState.small = 0;
  dchState.count = 0;
  dchState.move = 0x60;  //3 << 5
  return SUCCESS;
}

//process as much data as you can, update state
int DCH::Update(const BitSequence *data, 
		  DataLength databitlen){

  int max;

  if (databitlen == 0){
    return SUCCESS;
  }

  //If this is the beginning of the data block, we need to add sequence padding
  if(dchState.numUnprocessed == 0){
    DCH::setNextSequenceValue(&dchState);
    dchState.numUnprocessed = 8;
  }

  max=8*DCH_BLOCK_LENGTH_BYTES - dchState.numUnprocessed;
  if (databitlen < max){
    max = databitlen;
  }
  databitlen -= max;
  dchState.datalen += max;

  memcpy(dchState.unprocessed + (dchState.numUnprocessed / 8), data, (max+7) / 8);
  dchState.numUnprocessed += max;
  data += max/8;
  
  while(dchState.numUnprocessed == DCH_PADDED_BLOCK_LENGTH_BITS){

    DCH::hashOneBlock(&dchState);

    if(databitlen > 0){
      DCH::setNextSequenceValue(&dchState);
      dchState.numUnprocessed = 8;
      max = DCH_PADDED_BLOCK_LENGTH_BITS - 8;
      if(databitlen < max){
	max = databitlen;
      }
      databitlen -= max;
      dchState.datalen += max;

      memcpy(dchState.unprocessed + (dchState.numUnprocessed / 8), data, (max+7)/8);
      dchState.numUnprocessed = max + 8;
      data += max/8;
    } else {
      dchState.numUnprocessed = 0;
    }

  }

  return SUCCESS;
}

//perform final output filtering etc., return result in hashval
int DCH::Final(BitSequence *hashval){
  int i;

  if(dchState.numUnprocessed == 0){
    dchState.numUnprocessed = 8;
    DCH::setNextSequenceValue(&dchState);
  }

  //pad the last block with 1, 0s
  dchState.unprocessed[dchState.numUnprocessed / 8] |= 
    (0x80U >> (dchState.numUnprocessed % 8));
  dchState.unprocessed[dchState.numUnprocessed / 8] &= 
    (0xFF ^ ((1 << (7 - (dchState.numUnprocessed % 8))) - 1));
  memset(&dchState.unprocessed[dchState.numUnprocessed/8 + 1], 0, 
	 DCH_BLOCK_LENGTH_BYTES - dchState.numUnprocessed/8 - 1);

  dchState.numUnprocessed++;

  if(DCH_PADDED_BLOCK_LENGTH_BITS - dchState.numUnprocessed < 64){
    //we're near the end of this block, so the length will 
    //have to go to the next block.  Process this block first, 
    //then prepare the final block.

    DCH::hashOneBlock(&dchState);
    memset(&dchState.unprocessed[0], 0, DCH_BLOCK_LENGTH_BYTES);
    DCH::setNextSequenceValue(&dchState);
  }
  
  //copy length.  Be aware of endianness!
  for(i=0;i<sizeof(DataLength);i++){
    dchState.unprocessed[DCH_BLOCK_LENGTH_BYTES - 1 - i] = 
      ((BitSequence *)(&dchState.datalen))[i];
  }

  //set final sequence "move"
  dchState.unprocessed[0] &= 0x1F;  //00011111

  //hash the final (length) block
  DCH::hashOneBlock(&dchState);

  memcpy(hashval, dchState.curr, dchState.hashbitlen / 8);

  free(dchState.unprocessed);
  free(dchState.curr);
  free(dchState.p[0]);
  free(dchState.p[1]);
  free(dchState.p[2]);

  return SUCCESS;
}

// All-at-once hash.
int DCH::Hash(int hashbitlen, const BitSequence *data, 
		DataLength databitlen, BitSequence *hashval){
  //hashState *state = malloc(sizeof(hashState));
  //HashReturn status = Init(state,  hashbitlen);
	int status = DCH::Init(hashbitlen);
  if (status == SUCCESS){
    status = DCH::Update(data, databitlen);
    if (status == SUCCESS){
      status = DCH::Final(hashval);
    }
  }
  //free(state);
  return status;
}

void DCH::hashOneBlock(hashState *state){
  int round;

  //add previous hash value with message plaintext
  // for Miyaguchi-Preneel iteration
  DCH::addInto(dchState.curr, dchState.unprocessed);

  for(round = 0; round < dchNumRounds; round++){
    DCH::doSBox(dchState.unprocessed);
    DCH::doTransform(dchState.unprocessed);
    DCH::addInto(dchState.unprocessed, DCH_ROUND_KEY[round]);
  }

  //add processed value to previous value and plaintext for M-P iteration
  DCH::addInto(dchState.curr, dchState.unprocessed);

  //clear number of unprocessed
  dchState.numUnprocessed=0;
}

// Obtain the next element of a square-free sequence.  Set it as the first byte.
void DCH::setNextSequenceValue(hashState *state){
  //high-order three bits come from the Towers of Hanoi SF sequence
  //low-order five bits are a counter
  dchState.unprocessed[0] = dchState.move ^ dchState.count;
  dchState.count++;
  if(dchState.count == 32){
    dchState.count = 0;

    //counter has rolled over; generate next ToH sequence value
    if(dchState.parity){
      dchState.move = (dchState.small + 3*((dchState.small + 1) % 3)) << 5;
      dchState.top[dchState.small]--;
      dchState.small++;
      dchState.small %= 3;
      dchState.top[dchState.small]++;
      *(dchState.top[dchState.small]) = 1;
    }
    else{
      BitSequence a=1, b=2;
      if(dchState.small == 1){
	a=0;
      }
      else if(dchState.small == 2){
	b=0;
      }
      if(*(dchState.top[b]) > *(dchState.top[a])){
	//swap a, b
	b ^= a;
	a ^= b;
	b ^= a;
      }
      dchState.move = (b + 3*a) << 5;
      dchState.top[a]++;
      *(dchState.top[a]) = *(dchState.top[b]);
      dchState.top[b]--; 
    }
    dchState.parity ^= 1;
  }
}

void DCH::addInto(BitSequence *to, const BitSequence *from){
  int i;
  for(i=0; i<DCH_BLOCK_LENGTH_BYTES; i++){
    to[i] ^= from[i];
  }
}

void DCH::doSBox(BitSequence *data){
  int i;
  for(i=0; i<DCH_BLOCK_LENGTH_BYTES; i++){
    data[i] = DCH_SBOX[data[i]];
  }
}

void DCH::doTransform(BitSequence *data){
  int i, j, k;
  BitSequence *transformed, *t, x14, x13, x12, x11, x10, x9, x8, x7, x6, x5, x4, x3, x2, x1, x0, y4, y3, y2, y1, y0, *multrow;
  transformed = (BitSequence*) malloc(DCH_BLOCK_LENGTH_BYTES*sizeof(BitSequence));


  for(k=0;k<60;k+=15){
    x14 = x13 = x12 = x11 = x10 = x9 = x8 = x7 = x6 = x5 = x4 = 0;
    x3 = data[63];
    x2 = data[62];
    x1 = data[61];
    x0 = data[60];
    t = &data[59];
    multrow = dch_multtable[k];
    for(j=60; j>0; j -= 15){
      x14 = multrow[x14] ^ *t;t--;
      x13 = multrow[x13] ^ *t;t--;
      x12 = multrow[x12] ^ *t;t--;
      x11 = multrow[x11] ^ *t;t--;
      x10 = multrow[x10] ^ *t;t--;
      x9 = multrow[x9] ^ *t;t--;
      x8 = multrow[x8] ^ *t;t--;
      x7 = multrow[x7] ^ *t;t--;
      x6 = multrow[x6] ^ *t;t--;
      x5 = multrow[x5] ^ *t;t--;
      x4 = multrow[x4] ^ *t;t--;
      x3 = multrow[x3] ^ *t;t--;
      x2 = multrow[x2] ^ *t;t--;
      x1 = multrow[x1] ^ *t;t--;
      x0 = multrow[x0] ^ *t;t--;
    }

    for(j=k/3;j<255;j+=85){
      multrow = dch_multtable[j];
      y4 = multrow[multrow[x14] ^ x9] ^ x4;
      y3 = multrow[multrow[x13] ^ x8] ^ x3;
      y2 = multrow[multrow[x12] ^ x7] ^ x2;
      y1 = multrow[multrow[x11] ^ x6] ^ x1;
      y0 = multrow[multrow[x10] ^ x5] ^ x0;

      for(i=j/5;i<255;i+=51){
	multrow = dch_multtable[i];
      transformed[i % DCH_BLOCK_LENGTH_BYTES] = multrow[multrow[multrow[multrow[y4]
								    ^ y3]
							    ^ y2] 
						    ^ y1]
	                                     ^ y0;
      }
    }
  }
  x1 = x2 = 0;
  x0 = data[63];
  t = &data[62];
  multrow = dch_multtable[48];
  while(t > data){
    x2 = multrow[x2] ^ *t; t--;
    x1 = multrow[x1] ^ *t; t--;
    x0 = multrow[x0] ^ *t; t--;
  }
  multrow = dch_multtable[16];
  transformed[16] = multrow[multrow[x2] ^ x1] ^ x0;
  multrow = dch_multtable[101];
  transformed[33] = multrow[multrow[x2] ^ x1] ^ x0;
  multrow = dch_multtable[186];
  transformed[50] = multrow[multrow[x2] ^ x1] ^ x0;

  x0 = data[63];
  t=&data[62];
  multrow = dch_multtable[63];
  while(t >= data){
    x0 = multrow[x0] ^ *t; t--;
  }
  transformed[63] = x0;

  memcpy(data, transformed, DCH_BLOCK_LENGTH_BYTES);
  free(transformed);
}
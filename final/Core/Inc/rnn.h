#ifndef __RNN_H__
#define __RNN_H__

#define HIDDEN_SIZE 20
#define LAYER_NUM 2

#include "arm_math.h"

void rnn(
	float ax, float ay, float hidden[LAYER_NUM][HIDDEN_SIZE], float output[2],
	float hidden_next[LAYER_NUM][HIDDEN_SIZE]
);

void rnn_dsp(
	float ax, float ay, float hidden[LAYER_NUM][HIDDEN_SIZE], float output[2],
	float hidden_next[LAYER_NUM][HIDDEN_SIZE]
);

void rnn_dsp_q15(
	float ax, float ay, q15_t hidden[LAYER_NUM][HIDDEN_SIZE], float output[2],
	q15_t hidden_next[LAYER_NUM][HIDDEN_SIZE]
);

#endif

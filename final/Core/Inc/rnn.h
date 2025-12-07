#ifndef __RNN_H__
#define __RNN_H__

#define HIDDEN_SIZE 20
#define LAYER_NUM 2

void rnn(
	float ax, float ay, float hidden[LAYER_NUM][HIDDEN_SIZE], float output[2],
	float hidden_next[LAYER_NUM][HIDDEN_SIZE]
);

#endif
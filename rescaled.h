
#ifndef MULT_FACTOR_H
#define	MULT_FACTOR_H

//To minimize the effect of floating point calculation in the kernel,
//it is possible to set this to a value greater than 1.
//It rescales all the hartstone benchmark (deadlines and workloads)
//as well as rescaling min, max and nominal bursts in the kernel.
const int rescaled=1;

#endif //MULT_FACTOR_H

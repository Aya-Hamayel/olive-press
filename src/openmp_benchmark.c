
#include <stdio.h>
#include <math.h>
#include <omp.h>

#define NUM_ITEMS 1000000

/* Simulate computational work for one olive portion */
static double process_olive(int i) {
    double value = (double)i / NUM_ITEMS;

    for (int j = 0; j < 100; j++) {
        value = sin(value) + cos(value * 0.5);
    }

    return value;
}

int main(void) {
    double sequential_sum = 0.0;
    double parallel_sum = 0.0;

    double start = omp_get_wtime();

    /* Sequential execution */
    for (int i = 0; i < NUM_ITEMS; i++) {
        sequential_sum += process_olive(i);
    }

    double sequential_time = omp_get_wtime() - start;

    start = omp_get_wtime();

    /* Parallel execution */
    #pragma omp parallel for reduction(+:parallel_sum)
    for (int i = 0; i < NUM_ITEMS; i++) {
        parallel_sum += process_olive(i);
    }

    double parallel_time = omp_get_wtime() - start;

    printf("Sequential Time: %.6f seconds\n", sequential_time);
    printf("Parallel Time:   %.6f seconds\n", parallel_time);
    printf("Speedup:         %.2fx\n",
           sequential_time / parallel_time);

    printf("Sequential Result: %.6f\n", sequential_sum);
    printf("Parallel Result:   %.6f\n", parallel_sum);

    return 0;
}

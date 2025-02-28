#include <stdio.h>
#include <math.h>
#include <string.h>
#include "matrix_float.c"
#include "read_csv.c"

#define FIX_POINT_A 7
#define FIX_POINT_B 17

int convert_to_fix_point (float f){
    int tmp;

    if (f >= 0) {
        tmp = (int) (f * (1 << FIX_POINT_B) + 0.5);
    }
    else {
        tmp = (int) ((-f) * (1 << FIX_POINT_B) + 0.5);
        tmp = (1 << (FIX_POINT_A + FIX_POINT_B)) - tmp;
    }
    return tmp;
}

////////////
/// These functions can be precomputed:
void print_int_matrix(int row, int col, int** m) {
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            printf("%d ", m[i][j]);
        }
        printf("\n");
    }
}

int get_num_nbs(int r){
    int count = 0;
    for (int i = 0; i < (r+1)*(r+1); i++){
        int xi = i/(r+1);
        int yi = i%(r+1);
        int distsq = xi * xi + yi * yi;
        if (distsq <= r*r ){
            count += 1;
        }
    }

    int num_nbs = (count - (r + 1)) * 4 + 1;
    return num_nbs;
}

int* get_struc(int r){
    int rsq = r*r;
    int* l = malloc(sizeof(int) * (2*r + 1));
    for (int i = 0; i < 2*r+1; i++){
        int count  = 0;
        for (int j =0; j < 2*r+1; j++){
            int distsq = (i - r) * (i - r) + (j - r) * (j - r);
            if (distsq <= rsq){
                count+=1;
            }
        }
        l[i] = count;
    }
    return l;
}

int get_index_from_position(int xi, int yi, int xj, int yj, int r){
    int* l = get_struc(r);
    int core_index = (get_num_nbs(r) - 1)/2;
    if (yi == yj){
        int index = core_index + (xj - xi);
        free(l);
        return index;
    }
    else{
        int diff = 0;
        for (int i = 0; i < abs(yj - yi)-1; i++){
            diff += l[r-i-1];
        }
        if (yi > yj){
            int index = core_index - (l[r]-1)/2 - diff - (l[r-(yi - yj)]+1)/2 + (xj - xi);
            free(l);
            return index;
        }
        else {
            int index = core_index + (l[r]-1)/2 + diff + (l[r-(yi - yj)]+1)/2 + (xj - xi);
            free(l);
            return index;
        }
    }
}

int ** compute_indexset(int r, int num_nbs, int neuron_shape){
    int side_length = sqrt(neuron_shape);
    int** set = malloc(sizeof(int*) * neuron_shape);
    for (int i = 0; i < neuron_shape; i++) {
        set[i] = malloc(sizeof(float) * num_nbs);
    }
    for (int i = 0; i < neuron_shape; i++){
        for (int j = 0 ; j < num_nbs; j++){
            set[i][j] = neuron_shape;
        }
    }
    for (int i = 0; i < neuron_shape; i++){
        int xi = i / side_length;
        int yi = i % side_length;
        for (int j = 0; j < neuron_shape; j++){
            int xj = j / side_length;
            int yj = j % side_length;
            int distsq = (xi - xj)*(xi - xj) + (yi - yj)*(yi - yj);
            if (distsq <= r*r){
                int index = get_index_from_position(xi, yi, xj, yj, r);
                set[i][index] = j;
            }
        }
    }

    return set;
}

float* compute_WE(int num_nbs, int r, int we, int sigmaE){
    float* W = malloc(sizeof(float) * num_nbs);
    int count = 0;
    for (int i = 0; i < (2*r+1)*(2*r+1); i++){
        int xi = i / (2*r + 1);
        int yi = i % (2*r + 1);
        int distsq = (xi - r)* (xi - r) + (yi - r)* (yi - r);
        if (distsq <= r*r){
            W[count] = exp(- distsq/2.0/sigmaE);
            count += 1;
        }
    }

    int l = (num_nbs-1)/2;
    W[l] = 0;

    float W_E_sum = sum(num_nbs, W);
    for (int i = 0; i < num_nbs; i++){
        W[i] = we * W[i] / W_E_sum;
    }

    return W;
}

float* compute_WI(int num_nbs, int r, int wi){
    float* W = malloc(sizeof(float) * num_nbs);
    int count = 0;
    for (int i = 0; i < (2*r+1)*(2*r+1); i++){
        int xi = i / (2*r + 1);
        int yi = i % (2*r + 1);
        int distsq = (xi - r)* (xi - r) + (yi - r)* (yi - r);
        if (distsq <= r*r){
            W[count] = 1;
            count += 1;
        }
    }

    int l = (num_nbs-1)/2;
    W[l] = 0;

    float W_E_sum = sum(num_nbs, W);
    for (int i = 0; i < num_nbs; i++){
        W[i] = wi * W[i] / W_E_sum;
    }

    return W;
}
///////////

float max(float a, float b){
    if (a >= b){
        return a;
    }
    else{
        return b;
    }
}

void stimulate(int neuron_shape, int bs, float lr_act, float threshold, float eps, float** stimulus,
                  float** exc_act_dummy, float** inh_act_dummy, int leaky,
                  int num_E_nbs, int num_I_nbs, float* W_E, float* W_I, int** N_E, int** N_I){

    //float relative_error;
    float** exc_act_dummy_copy = malloc_matrix(bs, neuron_shape + 1);
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < neuron_shape + 1; j++) {
            exc_act_dummy_copy[i][j] = 0;
        }
    }

    float** inh_act_dummy_copy = malloc_matrix(bs, neuron_shape + 1);
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < neuron_shape + 1; j++) {
            inh_act_dummy_copy[i][j] = 0;
        }
    }

    for (int t = 0; t < 50; t++) {

        //float **exc_tm1 = copy_matrix(bs, neuron_shape+1, exc_act_dummy);

        float delta_a_exc, delta_a_inh;

        //int count = 0;

        // Update of activations
        for (int k = 0; k < bs; k++) {
            for (int i = 0; i < neuron_shape; i++) {

                //Update of exhibitory and inhibitory neurons;

//                int x = i/40;
//                int y = i - 40 * x;
//                if (x == 31 && y == 31){
//                    printf("iter = %d, exc_act_dummy[%d][%d] = %d\n",t, x, y, convert_to_fix_point(exc_act_dummy[k][i]));
//                    printf("iter = %d, leaky_exc_act_dummy[%d][%d] = %d\n",t, x, y, convert_to_fix_point(leaky * exc_act_dummy[k][i]));}
                delta_a_exc = - leaky * exc_act_dummy[k][i];
//                int x = i/40;
//                int y = i - 40 * x;
//                if (x == 31 && y == 31){
//                printf("iter = %d, -delta_a_exc[%d][%d] = %d\n",t, x, y, convert_to_fix_point(delta_a_exc));}
                delta_a_inh = - leaky * inh_act_dummy[k][i];
                for (int j = 0; j < num_E_nbs; j++) {
                    delta_a_exc += W_E[j] * exc_act_dummy[k][N_E[i][j]];
                    delta_a_inh += W_E[j] * exc_act_dummy[k][N_E[i][j]];
                }
                for (int j = 0; j < num_I_nbs; j++) {
                    delta_a_exc -= W_I[j] * inh_act_dummy[k][N_I[i][j]];
                }
                delta_a_exc += stimulus[k][i];
                //printf("delta_a_exc = %f\n", delta_a_exc);
                delta_a_exc = lr_act * delta_a_exc;
                //printf("delta_a_exc = %f\n", delta_a_exc);
                //printf("exc_act_dummy[%d][%d] = %f\n",k, i, exc_act_dummy[k][i]);
                exc_act_dummy_copy[k][i] = exc_act_dummy[k][i] + delta_a_exc;
                //printf("exc_act_dummy_copy[%d][%d] = %f\n",k, i, exc_act_dummy_copy[k][i]);
//                if (exc_act_dummy_copy[k][i] > 0.01024 || exc_act_dummy_copy[k][i] < -0.01024){
//                    count+=1;
//                }
                exc_act_dummy_copy[k][i] = max(exc_act_dummy_copy[k][i] - threshold, 0.0) - max(-exc_act_dummy_copy[k][i] - threshold, 0.0);
//                if (exc_act_dummy_copy[k][i] != 0){
//                    count+=1;
//                }

//                if (exc_act_dummy_copy[k][i] != 0){
//                    count+=1;
//                    int x = i/40;
//                    int y = i - 40 * x;
//                printf("iter %d: exc_act_dummy_copy[%d][%d] = %d\n",t, x, y, convert_to_fix_point(exc_act_dummy_copy[k][i]));}


                delta_a_inh = lr_act * delta_a_inh;
                inh_act_dummy_copy[k][i] = inh_act_dummy[k][i] + delta_a_inh;
                inh_act_dummy_copy[k][i] = max(inh_act_dummy_copy[k][i] - threshold, 0.0) - max(-inh_act_dummy_copy[k][i] - threshold, 0.0);

            }
        }


        //printf("count = %d\n", count);

        for (int k = 0; k < bs; k++) {
            for (int i = 0; i < neuron_shape; i++) {
                exc_act_dummy[k][i] = exc_act_dummy_copy[k][i];
                inh_act_dummy[k][i] = inh_act_dummy_copy[k][i];
            }
        }

//            float **da = matrix_minus(bs, neuron_shape+1, exc_act_dummy, exc_tm1);
//
//            float sqrt_da = 0;
//            for (int i = 0; i < bs; i++) {
//                for (int j = 0; j < neuron_shape; j++) {
//                    sqrt_da += da[i][j] * da[i][j];
//                }
//            }
//            sqrt_da = sqrt(sqrt_da);
//
//            float sqrt_exc_tm1 = 0;
//            for (int i = 0; i < bs; i++) {
//                for (int j = 0; j < neuron_shape; j++) {
//                    sqrt_exc_tm1 += exc_tm1[i][j] * exc_tm1[i][j];
//                }
//            }
//            sqrt_exc_tm1 = sqrt(sqrt_exc_tm1);
//
//            relative_error = sqrt_da / (eps + sqrt_exc_tm1);
//
//            free_matrix(bs, da);
//
//            free_matrix(bs, exc_tm1);
        }

//        if (relative_error < eps) {
//            printf("relative_error = %f\n", relative_error);
//            return exc_act_dummy;
//        } else {
//            printf("relative_error = %f\n", relative_error);
//            printf("Update doesn't converge.");
//            return exc_act_dummy;
//        }


}


int main() {

    // Hyperparameters
    int ri = 5;
    int re = 3;
    int wi = 5;
    int we = 30;
    int leaky = wi + we;
    int neuron_shape = 1600;
    int sigmaE = 3;
    int bs = 1;
    int imbed_dim = 97;
    float lr_act = 0.01;
    float threshold = 0.01;
    float eps = 5e-3;

    // precomputed parameters
    int num_E_nbs = get_num_nbs(re);
    int num_I_nbs = get_num_nbs(ri);
//    printf("num_E_nbs = %d", num_E_nbs);
//    printf("num_I_nbs = %d", num_I_nbs);

    int** N_E = compute_indexset(re, num_E_nbs, neuron_shape);
    int** N_I = compute_indexset(ri, num_I_nbs, neuron_shape);

    float* W_E = compute_WE(num_E_nbs, re, we, sigmaE);
    float* W_I = compute_WI(num_I_nbs, ri, wi);

//    print_float_vector(num_E_nbs, W_E);
//    printf("\n");
//    print_float_vector(num_I_nbs, W_I);

    // Read Files, and stimulus can also be precomputed
    float** mat = read_matrix(55529, imbed_dim, "word_embeddings.csv");
    float** word_batch = sample_matrix1(55529, imbed_dim, bs, mat);
    //print_matrix(bs, imbed_dim, mat);

    float** Phi = read_matrix(97, neuron_shape, "codebook.csv");
    //print_matrix(97, neuron_shape, Phi);

    float** stimulus = multiply(bs, imbed_dim, neuron_shape, word_batch, Phi);  // (256, 1600)
    write_matrix(bs, neuron_shape, stimulus, "stimulus.csv");

    // Malloc activations
    float** exc_act_dummy = malloc_matrix(bs, neuron_shape + 1);
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < neuron_shape + 1; j++) {
            exc_act_dummy[i][j] = 0;
        }
    }

    float** inh_act_dummy = malloc_matrix(bs, neuron_shape + 1);
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < neuron_shape + 1; j++) {
            inh_act_dummy[i][j] = 0;
        }
    }

    // Update of activations
    stimulate(neuron_shape, bs, lr_act, threshold, eps, stimulus,
                              exc_act_dummy, inh_act_dummy, leaky, num_E_nbs, num_I_nbs, W_E, W_I, N_E, N_I);

    int count1 = 0;
    for (int i  = 0; i < bs; i++){
        for (int j = 0; j < neuron_shape; j++){
            int x = j/40;
            int y = j - 40 * x;
            if (exc_act_dummy[i][j] != 0 ){
                count1 += 1;
            //printf("exc_act_dummy[%d][%d] = %d\n", x, y, convert_to_fix_point(exc_act_dummy[i][j]));}}
            printf("exc_act_dummy[%d][%d] = %f\n", x, y, exc_act_dummy[i][j]);}
        }
    }

    printf("count1 = %d", count1);


    //print_matrix(bs, neuron_shape, exc_act_dummy);

    //write_matrix(bs, neuron_shape, exc_act_dummy, "exc_act.csv");

    return 0;
}


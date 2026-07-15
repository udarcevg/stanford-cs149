void myNewFunction(float* input, float* output) {
    float * tempInput = new float[16 * 16];
    float * tempOutput = new float[16 * 16];

    transpose(input, tempInput);
    myFunction(tempInput, tempOutput);
    transpose(tempOutput, output);

    delete[] tempInput;
    delete[] tempOutput;


}
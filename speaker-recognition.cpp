#include <ctype.h>
#include <dirent.h>
#include <GRT.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <limits>

#include "mfcc.h"
#include "wav-reader.h"

static const char* kFemaleDatasetDir = "./data/fsew0_v1.1/";
static const char* kMaleDatasetDir = "./data/msak0_v1.1/";

bool load_audio_file_from_directory(
    GRT::TimeSeriesClassificationData& data,
    const char* const directory,
    uint32_t label,
    uint32_t count = std::numeric_limits<uint32_t>::max(),
    uint32_t skip = 0) {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(directory)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            char* suffix = strrchr(ent->d_name, '.');
            if (suffix != NULL && 0 == strcmp(suffix, ".wav")) {
                if (skip > 0) { skip -= 1; continue; }

                // printf("loading from %s\n", ent->d_name);

                std::string path;
                path.append(directory);
                path.append(ent->d_name);
                WavReader reader(path);

                GRT::MatrixDouble training_sample;
                training_sample.push_back(reader.getData());
                training_sample.transpose();
                data.addSample(label, training_sample);

                if (--count == 0) { return true; }
            }
        }
        closedir(dir);
        return false;
    } else {
        perror("Failed to open directory");
        exit(-1);
    }
}

int main(int argc, char* argv[]) {
    using namespace std;
    clock_t begin = clock();

    uint32_t num_cc = 0;
    uint32_t num_gmm = 0;
    uint32_t num_train = 0;
    uint32_t num_pred = 10;

    int c;
    while ((c = getopt (argc, argv, "c:g:t:p:")) != -1) {
        switch (c) {
            case 'c': num_cc = atoi(optarg); break;
            case 'g': num_gmm = atoi(optarg); break;
            case 't': num_train = atoi(optarg); break;
            case 'p': num_pred = atoi(optarg); break;
            default:
                abort();
        }
    }

    // std::cout << "Number of Cepstral Coefficient: " << num_cc << std::endl
    //           << "Number of Gaussian Mixtures   : " << num_gmm << std::endl
    //           << "Number of Training Audio Files: " << num_train << std::endl
    //           << "Number of Prediction Audios   : " << num_pred << std::endl;

    std::cerr << num_cc << ", "
              << num_gmm << ", "
              << num_train << ", "
              << num_pred << ", ";

    GRT::GestureRecognitionPipeline pipeline;

    // Sample rate is 16k, FFT window size 512 => 32 ms, hopsize 128 => 8 ms
    GRT::FFT fft(512, 128, 1,  GRT::FFT::HAMMING_WINDOW, true, false);

    GRT::MFCC::Options options;
    options.sample_rate = 16000;
    options.fft_size = 512 / 2;
    options.start_freq = 300;
    options.end_freq = 8000;
    options.num_tri_filter = 26;
    options.num_cepstral_coeff = num_cc;
    options.lifter_param = 22;
    options.use_vad = true;
    options.noise_level = 30;
    GRT::MFCC mfcc(options);

    GRT::GMM gmm(num_gmm, true, false, 1, 100, 0.001);

    pipeline.addFeatureExtractionModule(fft);
    pipeline.addFeatureExtractionModule(mfcc);
    pipeline.setClassifier(gmm);

    GRT::TimeSeriesClassificationData data(1);
    load_audio_file_from_directory(data, kFemaleDatasetDir, 1, num_train);
    load_audio_file_from_directory(data, kMaleDatasetDir, 2, num_train);
    // std::cout << data.getStatsAsString() << std::endl;

    pipeline.train(data);
    pipeline.getClassifier()->save("classifier.grt");

    GRT::TimeSeriesClassificationData test_data(1);
    load_audio_file_from_directory(test_data, kFemaleDatasetDir, 1,
                                   num_pred, num_train + 1);

    GRT::MatrixDouble test_samples = test_data.getDataAsMatrixDouble();
    vector<uint32_t> classified_result(3, 0);
    for (uint32_t i = 0; i < test_samples.getNumRows(); i++) {
        pipeline.predict(test_samples.getRowVector(i));
        uint32_t label = pipeline.getPredictedClassLabel();
        // std::cout << test_samples.getRowVector(i)[0] << " " << label << std::endl;
        classified_result[label]++;
    }

    double accuracy = 1.0 * classified_result[1] / (classified_result[1] + classified_result[2]);
    // std::cout << "Female: " << classified_result[1] << std::endl;
    // std::cout << "Male  : " << classified_result[2] << std::endl;
    // std::cout << "Rate  : " << accuracy << std::endl;
    std::cerr << accuracy << ", ";

    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    std::cerr << elapsed_secs << std::endl;
}

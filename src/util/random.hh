#include <random>

// Borrowing from
// https://kristerw.blogspot.com/2017/05/seeding-stdmt19937-random-number-engine.html
// and http://www.pcg-random.org/posts/cpps-random_device.html

std::mt19937 get_random_generator();

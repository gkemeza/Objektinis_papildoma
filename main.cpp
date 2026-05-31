#include "functions.cpp"

int main() {
  // Tasks 1 & 2
  runWordFrequency("data/text.txt", "output/word_freq.txt",
                   "output/cross_ref.txt");

  // Task 3
  runUrlExtraction("data/urls_text.txt", "data/tlds.txt", "output/urls.txt");

  return 0;
}
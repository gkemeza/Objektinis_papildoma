#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

// Regex patterns for URL extraction
// https://www.vu.lt/path?q=1 or http://vu.lt
const std::regex RX_FULL(R"(https?://[^\s\)\]\,\"\'<>]+)", std::regex::icase);

// [some text](https://example.com)
const std::regex RX_MARKDOWN(R"(\[[^\]]*\]\((https?://[^\)]+)\))",
                             std::regex::icase);

// www.example.com or www.vu.lt/path
const std::regex
    RX_BARE_WWW(R"(\bwww\.[a-zA-Z0-9\-]+\.[a-zA-Z]{2,}[^\s\)\]\,\"\'<>]*)");

std::string cleanWord(const std::string &raw) {
  std::string result;
  for (unsigned char c : raw)
    if (std::isalpha(c))
      result += std::tolower(c);
  return result;
}

void ensureOutputDir() { std::filesystem::create_directories("output"); }

std::set<std::string> loadTlds(const std::string &path) {
  std::set<std::string> tlds;
  std::ifstream f(path);
  std::string line;

  while (std::getline(f, line)) {
    // Skip comment lines and empty lines
    if (line.empty() || line[0] == '#')
      continue;

    // File is already uppercase, but trim any \r (Windows line endings)
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    tlds.insert(line); // e.g. "COM", "LT", "ORG"
  }
  return tlds;
}

static std::string extractTld(const std::string &url) {
  std::string s = url;

  // Strip protocol: https://www.vu.lt → www.vu.lt
  size_t proto = s.find("://");
  if (proto != std::string::npos)
    s = s.substr(proto + 3);

  // Strip path/query/fragment: www.vu.lt/page?q=1 → www.vu.lt
  size_t slash = s.find('/');
  if (slash != std::string::npos)
    s = s.substr(0, slash);

  size_t query = s.find('?');
  if (query != std::string::npos)
    s = s.substr(0, query);

  // Get last segment after final dot: www.vu.lt → lt
  size_t dot = s.rfind('.');
  if (dot == std::string::npos)
    return "";

  std::string tld = s.substr(dot + 1);

  // Uppercase for comparison against IANA list
  std::transform(tld.begin(), tld.end(), tld.begin(), ::toupper);
  return tld;
}

void extractPattern(const std::string &content, const std::regex &pattern,
                    std::set<std::string> &out, int captureGroup = 0) {
  auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it)
    out.insert((*it)[captureGroup].str());
}

std::string readAll(const std::string &path) {
  std::ifstream f(path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void runUrlExtraction(const std::string &inputPath, const std::string &tldPath,
                      const std::string &outputPath) {

  std::string content = readAll(inputPath);
  if (content.empty()) {
    std::cerr << "Cannot open or empty: " << inputPath << "\n";
    return;
  }

  // Load valid TLDs
  std::set<std::string> validTlds = loadTlds(tldPath);
  if (validTlds.empty()) {
    std::cerr << "TLD list empty or missing: " << tldPath << "\n";
    return;
  }

  // Extract candidates
  std::set<std::string> candidates;
  extractPattern(content, RX_MARKDOWN, candidates, 1);
  extractPattern(content, RX_FULL, candidates, 0);
  extractPattern(content, RX_BARE_WWW, candidates, 0);

  // Validate against IANA TLD list
  std::set<std::string> validUrls;
  for (const auto &url : candidates) {
    std::string tld = extractTld(url);
    if (validTlds.count(tld)) // O(log n) lookup
      validUrls.insert(url);
    else
      std::cerr << "[REJECTED] " << url << " (TLD: " << tld << ")\n";
  }

  ensureOutputDir();

  std::ofstream out(outputPath);
  if (!out) {
    std::cerr << "Cannot open output: " << outputPath << "\n";
    return;
  }

  for (const auto &url : validUrls)
    out << url << "\n";

  std::cout << "[URL Extraction] Done. Valid URLs: " << validUrls.size()
            << "\n";
}

void runWordFrequency(const std::string &inputPath,
                      const std::string &freqOutputPath,
                      const std::string &crossRefOutputPath) {
  std::ifstream in(inputPath);
  if (!in) {
    std::cerr << "Cannot open: " << inputPath << "\n";
    return;
  }

  std::map<std::string, int> freq;
  std::map<std::string, std::set<int>> crossRef;

  std::string line;
  int lineNum = 0;

  while (std::getline(in, line)) {
    lineNum++;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
      std::string word = cleanWord(token);
      if (!word.empty()) {
        freq[word]++;
        crossRef[word].insert(lineNum);
      }
    }
  }

  ensureOutputDir();

  std::ofstream freqOut(freqOutputPath);
  for (const auto &[word, count] : freq)
    if (count > 1)
      freqOut << word << " : " << count << "\n";

  std::ofstream crossOut(crossRefOutputPath);
  for (const auto &[word, lines] : crossRef) {
    if (freq[word] > 1) {
      crossOut << word << " : ";
      bool first = true;
      for (int ln : lines) {
        if (!first)
          crossOut << " ";
        crossOut << ln;
        first = false;
      }
      crossOut << "\n";
    }
  }

  std::cout << "[Task 1 & 2] Done. Unique words: " << freq.size() << "\n";
}
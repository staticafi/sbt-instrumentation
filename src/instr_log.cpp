#include <fstream>
#include <string>

using namespace std;

class Logger {
    string file;
  public:
    Logger (string path);
    void write_error (const string &text);
    void write_info (const string &text);
};

Logger::Logger (string path) {
  file = path;
  ofstream ofs;
  ofs.open(path, std::ofstream::out | std::ofstream::trunc);
  ofs.close();
}

void Logger::write_error(const string &text)
{
    std::ofstream log_file(
        file, ios_base::out | ios_base::app );
    log_file << "Error: " << text << endl;
}

void Logger::write_info(const string &text)
{
    std::ofstream log_file(
        file, ios_base::out | ios_base::app );
    log_file << "Info: " << text << endl;
}

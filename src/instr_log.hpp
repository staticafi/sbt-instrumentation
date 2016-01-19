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

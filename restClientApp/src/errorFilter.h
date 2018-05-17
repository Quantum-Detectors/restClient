#ifndef ERROR_FILTER_H
#define ERROR_FILTER_H

#include <string>
#include <vector>


class ErrorFilter
{
 public:
  bool newError(std::string error);
  void clearErrors();

 private:
  std::vector<std::string> mErrors;
};

#endif

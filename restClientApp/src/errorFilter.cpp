#include <algorithm>

#include "errorFilter.h"


bool ErrorFilter::newError(std::string error)
{
  if (std::find(mErrors.begin(), mErrors.end(), error) == mErrors.end()) {
    // If this is a new error message we add it and return true
    mErrors.push_back(error);
    return true;
  }
  return false;
}

void ErrorFilter::clearErrors()
{
  mErrors.clear();
}
